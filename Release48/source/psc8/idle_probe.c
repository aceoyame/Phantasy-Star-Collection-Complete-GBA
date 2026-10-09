/* Loop detector for the Z80 slice scheduler.
 *
 * The guest only changes state at slice boundaries (interrupts, new key state) or by its own writes. The verified C core
 * (core/z80f.h) is run on a private copy of the register state for a bounded number of instructions, with these rules:
 *   - every memory write stores the value that is already there,
 *   - every port read comes from a port that is constant for the frame (keys, GG start button) or is the VDP status with nothing pending,
 *   - there are no port writes, and only plain instructions (no HALT, DI/EI, IX/IY, block I/O ...) are executed.
 * Under those rules the only thing that can change between two passes through the same code is the register file, and two kinds of
 * loop can be passed without executing them:
 *
 *  1. Pure loop: the state returns to exactly the state the probe started with.  Nothing in this slice can change it, so the whole slice
 *     is passed (the caller advances guest time to the slice end).
 *  2. Counter loop (timeouts, delay loops):  the probe finds an anchor instruction that is part of a counter idiom
 *         DEC BC|DE|HL ; LD A,hi ; OR lo ; JR NZ|JP NZ         (A and F are overwritten before they are read)
 *         DJNZ                                                  (B counts down)
 *     and then checks that one pass from the anchor back to the anchor leaves every register unchanged but the counter (one lower),
 *     that no instruction of the pass reads or writes the counter's registers apart from the idiom, and measures the T-states of the pass.
 *     Every further pass is then identical (the pass only depends on the state the pass proved constant), until the counter runs out.
 *     K passes are skipped by lowering the counter by K and advancing guest time by K*T, always leaving the last two counts and at least one
 *     full pass for the real core, so exits and the final flags are produced by real execution.
 *
 * The probe never touches the guest's state until it has proved a loop: writes are only compared, the register state is a private copy.
 * The cost is bounded (PROBE_STEPS instructions) and it aborts at the first instruction that is not purely a poll.
 */
#include <stdint.h>
#ifdef PROBE_HOST
#include <stdio.h>
#endif
#include "z80a.h"
static int pr_abort;
uint32_t psc8_probe_dbg[4];
#ifdef PROBE_HOST
uint32_t psc8_probe_rinc;     /* host test: refresh-register increments of the skipped instructions (the GBA core derives LD A,R from the T-state budget) */
#define RINC(n) (ri+=(n))
#else
#define RINC(n) ((void)0)
#endif   /* last failed probe: reason, step, pc, opcode (debug counters read by tools/bench.py) */
#ifdef PROBE_HOST            /* host test build (tools/sms_host.c): memory through the host's mapper */
extern uint32_t host_rd8(uint32_t a);
static inline uint32_t probe_rd(uint32_t a){return host_rd8(a&0xffffu);}
#else
static inline uint32_t probe_rd(uint32_t a){a&=0xffffu;return ((const uint8_t*)((const uint32_t*)z80a_ctx)[a>>10])[a];}
#endif
static void probe_wr(uint32_t a,uint32_t v){
 a&=0xffffu;
 if(a<0x8000u)return;                                     /* ROM area: ignored by the guest memory map */
 if(a<0xc000u||a>=0xfffcu||probe_rd(a)!=(v&255u))pr_abort=1000+(int)(a>>8);   /* cartridge RAM, mapper registers, or a write that changes memory */
}
static uint32_t pr_status;
static uint32_t probe_in(uint32_t p){
 p&=255u;
 if(Z80A_U8(Z80A_FASTIN+p))return Z80A_U8(Z80A_VALIN+p);
 if((p&0xc1u)==0x81u&&!pr_status&&!Z80A_U8(Z80A_INTP)&&!Z80A_U8(Z80A_CMDFIRST))return 0;   /* VDP status poll with nothing to clear */
 pr_abort=2+(int)p;return 0xff;
}
#define RD8(a) probe_rd(a)
#define WR8(a,v) probe_wr((a),(v))
#define PORT_IN(p) probe_in(p)
#define PORT_OUT(p,v) (pr_abort=1)
#define Z80F_STATIC static
#include "core/z80f.h"
#include "core/z80_idle_cases.inc"
#define PROBE_STEPS 144
#define PROBE_MIN_SKIP 6u          /* a counter skip must save at least this many passes to be worth the probe */
/* no libgcc divide on this target: shift/subtract */
static uint32_t udiv(uint32_t n,uint32_t d){uint32_t q=0,r=0;for(int i=31;i>=0;i--){r=(r<<1)|((n>>i)&1u);if(r>=d){r-=d;q|=1u<<i;}}return q;}
/* registers an opcode reads or writes, as a mask (B=1 C=2 D=4 E=8 H=16 L=32); (HL) counts as H|L. Only opcodes of Z80_IDLE_CASES occur. */
static uint32_t op_touch(uint32_t op){
 static const uint8_t r8m[8]={1,2,4,8,16,32,48,0},pm[4]={3,12,48,0};
 uint32_t x=op>>6,y=(op>>3)&7,z=op&7,p=(op>>4)&3;
 switch(x){
  case 0:switch(z){
   case 0:return op==0x10?1u:0;
   case 1:return (y&1)?(pm[p]|48u):pm[p];
   case 2:return op==0x02||op==0x0A?3u:op==0x12||op==0x1A?12u:(op==0x22||op==0x2A)?48u:0;
   case 3:return pm[p];
   case 4:case 5:case 6:return r8m[y];
   default:return 0;}
  case 1:return r8m[y]|r8m[z];
  case 2:return r8m[z];
  default:switch(op){
   case 0xC1:case 0xC5:return 3;
   case 0xD1:case 0xD5:return 12;
   case 0xE1:case 0xE5:case 0xE9:case 0xF9:return 48;
   case 0xD9:return 63;
   case 0xEB:return 60;
   default:return 0;}
 }
}
/* counter idiom at address a? returns the counter pair (0=BC 1=DE 2=HL) + 1, 0 if it is not one; 4 = DJNZ */
static uint32_t idiom_at(uint32_t a){
 uint32_t o=probe_rd(a);
 if(o==0x10)return 4;
 if(o!=0x0B&&o!=0x1B&&o!=0x2B)return 0;
 uint32_t p=o>>4,hi=probe_rd(a+1),lo=probe_rd(a+2),br=probe_rd(a+3);
 if(hi!=0x78u+2u*p||lo!=0xB1u+2u*p||(br!=0x20&&br!=0xC2))return 0;
 return p+1;
}
#define COMMIT() do{Z80A_U32(Z80A_A)=A;Z80A_U32(Z80A_F)=F;Z80A_U32(Z80A_BC)=(B<<8)|C;Z80A_U32(Z80A_DE)=(D<<8)|E;Z80A_U32(Z80A_HL)=(H<<8)|L;\
 Z80A_U32(Z80A_PC)=PC;Z80A_U32(Z80A_SP)=SP;Z80A_U32(Z80A_AF2)=((uint32_t)z->a_<<8)|z->f_;Z80A_U32(Z80A_BC2)=((uint32_t)z->b_<<8)|z->c_;\
 Z80A_U32(Z80A_DE2)=((uint32_t)z->d_<<8)|z->e_;Z80A_U32(Z80A_HL2)=((uint32_t)z->h_<<8)|z->l_;}while(0)
int psc8_idle_probe(unsigned vdp_status,uint32_t avail,uint32_t *used){
 static Z80F zs;Z80F *z=&zs;static uint8_t init;
 if(!init){z80f_tables();init=1;}
 pr_abort=0;pr_status=vdp_status;*used=0;
 if(Z80A_U8(Z80A_HALTED)||Z80A_U8(Z80A_INTP))return 0;
 uint32_t v,_fb,_fw,cyc=0,irq_check=0;(void)irq_check;
 uint32_t A=Z80A_U32(Z80A_A)&255u,F=Z80A_U32(Z80A_F)&255u;
 v=Z80A_U32(Z80A_BC);uint32_t B=(v>>8)&255u,C=v&255u;v=Z80A_U32(Z80A_DE);uint32_t D=(v>>8)&255u,E=v&255u;
 v=Z80A_U32(Z80A_HL);uint32_t H=(v>>8)&255u,L=v&255u;
 uint32_t PC=Z80A_U32(Z80A_PC)&0xffffu,SP=Z80A_U32(Z80A_SP)&0xffffu;
 v=Z80A_U32(Z80A_AF2);z->a_=(uint8_t)(v>>8);z->f_=(uint8_t)v;v=Z80A_U32(Z80A_BC2);z->b_=(uint8_t)(v>>8);z->c_=(uint8_t)v;
 v=Z80A_U32(Z80A_DE2);z->d_=(uint8_t)(v>>8);z->e_=(uint8_t)v;v=Z80A_U32(Z80A_HL2);z->h_=(uint8_t)(v>>8);z->l_=(uint8_t)v;
 z->iff1=Z80A_U8(Z80A_IFF1);z->iff2=Z80A_U8(Z80A_IFF2);
 const uint32_t A0=A,F0=F,B0=B,C0=C,D0=D,E0=E,H0=H,L0=L,PC0=PC,SP0=SP;
 const uint8_t a_0=z->a_,f_0=z->f_,b_0=z->b_,c_0=z->c_,d_0=z->d_,e_0=z->e_,h_0=z->h_,l_0=z->l_;
 /* anchor state (counter loops) */
 uint32_t ri=0,an_ri=0;
 uint32_t an_kind=0,an_pc=0,an_mask=0,an_cyc=0,touch=0,s_bc=0,s_de=0,s_hl=0,s_sp=0,s_a=0,s_f=0,fin=0;
 uint8_t sa_a=0,sa_f=0,sa_b=0,sa_c=0,sa_d=0,sa_e=0,sa_h=0,sa_l=0;
 for(unsigned n=0;n<PROBE_STEPS;n++){
  uint32_t pc0=PC;
  if(an_kind){
   if(pc0==an_pc){                                             /* back at the anchor: one full pass done */
    if(fin){COMMIT();*used=cyc;
#ifdef PROBE_HOST
     psc8_probe_rinc=ri;
#endif
     return 2;}                      /* the last pass was executed after the skip: A and F are exact */
    uint32_t bc=(B<<8)|C,de=(D<<8)|E,hl=(H<<8)|L,cnt,ok=SP==s_sp&&z->a_==sa_a&&z->f_==sa_f&&z->b_==sa_b&&z->c_==sa_c&&z->d_==sa_d&&z->e_==sa_e&&z->h_==sa_h&&z->l_==sa_l;
    if(an_kind==1){ok=ok&&de==s_de&&hl==s_hl&&bc==((s_bc-1u)&0xffffu);cnt=bc?bc:0x10000u;}
    else if(an_kind==2){ok=ok&&bc==s_bc&&hl==s_hl&&de==((s_de-1u)&0xffffu);cnt=de?de:0x10000u;}
    else if(an_kind==3){ok=ok&&bc==s_bc&&de==s_de&&hl==((s_hl-1u)&0xffffu);cnt=hl?hl:0x10000u;}
    else{ok=ok&&A==s_a&&F==s_f&&C==(s_bc&255u)&&de==s_de&&hl==s_hl&&B==(((s_bc>>8)-1u)&255u);cnt=B?B:0x100u;}
    uint32_t T=cyc-an_cyc;
    if(!ok||(touch&an_mask)||!T||cnt<2u+PROBE_MIN_SKIP||cyc>=avail){psc8_probe_dbg[0]=0xfffeu;psc8_probe_dbg[1]=n;psc8_probe_dbg[2]=pc0;psc8_probe_dbg[3]=(ok?1u:0u)|((touch&an_mask)?2u:0u)|(cnt<2u+PROBE_MIN_SKIP?4u:0)|(cyc>=avail?8u:0);return 0;}
    uint32_t k=cnt-2u,kt=udiv(avail-cyc,T);                          /* counts that may be skipped; time left, in passes */
    if(kt<2u)return 0;
    kt-=1u;if(k>kt)k=kt;                                          /* always leave a full pass of time for the real core */
    if(k<PROBE_MIN_SKIP)return 0;
    if(an_kind==4){B=(B-k)&255u;cyc+=k*T;COMMIT();*used=cyc;
#ifdef PROBE_HOST
     psc8_probe_rinc=ri+k*(ri-an_ri);
#endif
     return 2;}   /* A and F are the same in every pass: nothing to replay */
    k-=1u;cyc+=k*T;fin=1;
#ifdef PROBE_HOST
    ri+=k*(ri-an_ri);
#endif                                         /* skip k-1 passes, then execute the last one so that A and F come out exactly */
    if(an_kind==1){bc=(bc-k)&0xffffu;B=bc>>8;C=bc&255u;}
    else if(an_kind==2){de=(de-k)&0xffffu;D=de>>8;E=de&255u;}
    else{hl=(hl-k)&0xffffu;H=hl>>8;L=hl&255u;}
   }
  }else{
   uint32_t pk=probe_rd(pc0);
   if(pk==0x10||pk==0x0B||pk==0x1B||pk==0x2B){uint32_t id=idiom_at(pc0);
    if(id){an_kind=id==4?4:id;an_pc=pc0;an_cyc=cyc;an_ri=ri;an_mask=id==4?1u:id==1?3u:id==2?12u:48u;
     s_bc=(B<<8)|C;s_de=(D<<8)|E;s_hl=(H<<8)|L;s_sp=SP;s_a=A;s_f=F;sa_a=z->a_;sa_f=z->f_;sa_b=z->b_;sa_c=z->c_;sa_d=z->d_;sa_e=z->e_;sa_h=z->h_;sa_l=z->l_;touch=0;}}
  }
  uint32_t op=FETCH();cyc+=z80f_cyc_00[op];RINC(1);
  if(an_kind&&!(an_kind==4?pc0==an_pc:pc0-an_pc<3u))touch|=op_touch(op);
  switch(op){
   Z80_IDLE_CASES
   case 0xCB:{uint32_t o2=FETCH(),rr=o2&7u,nn=(o2>>3)&7u;
    if((o2>>6)!=1u){pr_abort=1;break;}
    cyc+=rr==6?12u:8u;RINC(1);if(an_kind)touch|=rr==6?48u:rr<6?1u<<rr:0u;
    uint32_t t=rr==6?RD8((H<<8)|L):rr==0?B:rr==1?C:rr==2?D:rr==3?E:rr==4?H:rr==5?L:A;
    if(rr==6)OP_BITM(t,nn);else OP_BIT(t,nn);}break;
   default:pr_abort=1;break;
  }
  if(pr_abort){
#ifdef PROBE_HOST
   extern int probe_dbg;if(probe_dbg)fprintf(stderr,"probe abort %d at step %u op %02x pc %04x\n",pr_abort,n,(unsigned)op,(unsigned)pc0);
#endif
   psc8_probe_dbg[0]=(uint32_t)pr_abort;psc8_probe_dbg[1]=n;psc8_probe_dbg[2]=pc0;psc8_probe_dbg[3]=op;
   return 0;}
  if(PC==PC0&&SP==SP0&&A==A0&&F==F0&&B==B0&&C==C0&&D==D0&&E==E0&&H==H0&&L==L0&&
     z->a_==a_0&&z->f_==f_0&&z->b_==b_0&&z->c_==c_0&&z->d_==d_0&&z->e_==e_0&&z->h_==h_0&&z->l_==l_0)return 1;
 }
 psc8_probe_dbg[0]=0xffffu;psc8_probe_dbg[1]=PROBE_STEPS;psc8_probe_dbg[2]=PC;psc8_probe_dbg[3]=0;
#ifdef PROBE_HOST
 {extern int probe_dbg;if(probe_dbg)fprintf(stderr,"probe nocycle pc0 %04x pc %04x sp0 %04x sp %04x\n",(unsigned)PC0,(unsigned)PC,(unsigned)SP0,(unsigned)SP);}
#endif
 return 0;
}
