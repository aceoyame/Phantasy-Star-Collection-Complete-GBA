/* PSC8 cartridge engine. Independent RAM lifetime from PSC and PSIV. */
#include <stdint.h>
#include <stddef.h>
#include "fast_opll.h"
#define R16(a) (*(volatile uint16_t*)(a))
#define R32(a) (*(volatile uint32_t*)(a))
#define HOT __attribute__((section(".hot")))
#define DATAHOT __attribute__((section(".hotdata")))
#define N 304
_Static_assert(N==304&&59736/N==196&&59736%N==152,"audio_catchup is derived for 304 samples per 59736-cycle frame");
static const uint8_t *cart DATAHOT;
static uint32_t cart_size DATAHOT,cart_mask DATAHOT;
static uint8_t ram[8192],sram[32768],vram[16384] __attribute__((aligned(4))),cram[64],vr[16],banks[4];
/* The two low ROM banks hold the common kernel and are cached once.
 * Higher banks remain direct ROM mappings: copying every bank switch was
 * substantially slower in PS1 and Gaiden's bank-heavy scenes. */
static uint8_t cart_page[2][16384] __attribute__((aligned(4)));
static uint32_t sram_base,sram_lim;   /* persistent window of the cartridge SRAM in GBA SRAM: base address and length (0 = not persistent) */
static uint8_t tile_dirty_bits[64] __attribute__((aligned(4)));
static uint8_t regs_changed,mode_gg,enable_fm,status,stereo,fm_control;
static uint8_t palette_dirty=1;
static uint8_t fm_addr,fm_active;
static uint16_t scanline,keys,keys_tab=0xffff;
static uint32_t cycles_next,sample_next,sample_phase,frames,ports_fm,ports_gg;
static FastOPLL fastopll;
#define NBUF 4   /* audio ring: light frames may run up to two frames ahead to absorb heavy ones */
static int8_t pcm[NBUF][2][N+32] __attribute__((aligned(4)));
static volatile uint32_t audio_frame,late_frames;
static volatile uint8_t playbuf;
static volatile uint8_t ready[NBUF];
static const uint32_t silence[4]={0,0,0,0};
static uint32_t spread[256];   /* EWRAM: frees 1 KiB of IWRAM for the FM envelope code */
static uint8_t map_dirty_bits[128] __attribute__((aligned(4)));
static uint16_t map_base_cache=0xffff;
#define PSG_TONE_ASM
#include "psg.h"
#include "../additional/pager.h"
void *memset(void *p,int c,size_t n){uint8_t *d=p;while(n--)*d++=c;return p;}
void *memcpy(void *p,const void *s,size_t n){uint8_t *d=p;const uint8_t *q=s;while(n--)*d++=*q++;return p;}
void __aeabi_memcpy(void *p,const void*s,size_t n){memcpy(p,s,n);}
void __aeabi_memcpy4(void *p,const void*s,size_t n){memcpy(p,s,n);}
void __aeabi_memclr(void *p,size_t n){memset(p,0,n);}
void __aeabi_memclr4(void *p,size_t n){memset(p,0,n);}
#include "z80a.h"
#include "z80a_glue.h"
#define RMAP ((const uint8_t**)z80a_ctx)
/* VDP access state shared with the core's VRAM-write fast path */
#define vaddr Z80A_U32(Z80A_VADDR)
#define vcode Z80A_U8(Z80A_VCODE)
#define cmd_first Z80A_U8(Z80A_CMDFIRST)
#define readbuf Z80A_U8(Z80A_READBUF)
#define sprite_dirty Z80A_U8(Z80A_SPRDIRTY)
#define cmd_lo Z80A_U8(Z80A_CMDLO)
static void vdp_bases(void){Z80A_U32(Z80A_NB)=(vr[2]&14u)<<10;Z80A_U32(Z80A_SB)=(vr[5]&0x7eu)<<7;}              /* 68 read pages of 1 KiB; RMAP[a>>10][a] is the byte at Z80 address a */
static uint32_t zcyc, slice_base, slice_budget;      /* guest T-state counter and the running slice */
static const uint8_t *cart_bank(uint8_t bank){
 uint32_t off=((uint32_t)bank<<14)&cart_mask;
 return off<32768u?cart_page[off>>14]:cart+off;
}
static void update_map(void){
 const uint8_t *s1=cart_bank(banks[1]),*s2=cart_bank(banks[2])-0x4000;
 const uint8_t *s3=(banks[0]&8)?sram+((banks[0]&4)?16384:0)-0x8000:cart_bank(banks[3])-0x8000;
 RMAP[0]=cart_page[0];
 for(unsigned i=1;i<16;i++)RMAP[i]=s1;
 for(unsigned i=16;i<32;i++)RMAP[i]=s2;
 for(unsigned i=32;i<48;i++)RMAP[i]=s3;
 for(unsigned i=48;i<56;i++)RMAP[i]=ram-0xc000;
 for(unsigned i=56;i<64;i++)RMAP[i]=ram-0xe000;
 for(unsigned i=64;i<68;i++)RMAP[i]=RMAP[i-64]-0x10000;
 Z80A_U8(Z80A_SRAMEN)=(banks[0]&8)?1:0;
}
/* Block output to the VDP data port (called by the core for OTIR, for "OUTI;JP NZ,<the OUTI>" loops and for runs of consecutive OUTIs (unrolled
 * uploads) when the port is the VDP data port and the access is not CRAM). It does exactly what the per-byte path does - VRAM[addr]=byte, READBUF,
 * address+1 mod 16 KiB, tile/name-table/sprite dirty marks - in runs, and charges the guest the same T-states, with the slice ending at the
 * same instruction boundary as the per-byte path would. mode 0: OTIR (21 T per repeat, 16 for the last). mode 1: OUTI (16 T) + JP NZ (10 T). mode 2: n consecutive OUTI (16 T each).
 * In: ctx HL, BC, F, PC (address after the OUTI/OTIR), R6NOW (budget left before the instruction, > 0). Out: the same fields. */
static void mark_bits(uint8_t *bits,unsigned e0,unsigned e1){                 /* set bit indexes e0..e1 (inclusive) */
 unsigned b0=e0>>3,b1=e1>>3;uint8_t m0=(uint8_t)(0xffu<<(e0&7)),m1=(uint8_t)(0xffu>>(7-(e1&7)));
 if(b0==b1){bits[b0]|=(uint8_t)(m0&m1);return;}
 bits[b0]|=m0;for(unsigned k=b0+1;k<b1;k++)bits[k]=0xff;bits[b1]|=m1;
}
HOT static void vdp_block_dirty(unsigned a,unsigned c){                          /* bytes a..a+c-1 (a+c <= 16384, c <= 1024) were written */
 mark_bits(tile_dirty_bits,a>>5,(a+c-1)>>5);
 unsigned d0=(a-Z80A_U32(Z80A_NB))&16383u,d1=d0+c;                              /* name table: 2 KiB at NB, 2 bytes per entry */
 if(d1>16384u){mark_bits(map_dirty_bits,0,(d1-16385u)>>1);}                      /* wrapped: the part from offset 0 */
 else if(d0<2048u){unsigned hi=d1>2048u?2048u:d1;mark_bits(map_dirty_bits,d0>>1,(hi-1u)>>1);}
 unsigned s0=(a-Z80A_U32(Z80A_SB))&16383u,s1=s0+c;                              /* sprite table: Y bytes [0,64), X/N bytes [128,256) */
 if(s1>16384u||s0<64u||(s0<256u&&s1>128u))sprite_dirty=1;
}
/* Mapped ROM/RAM/SRAM sources cannot alias the separate VDP array.
 * Match the original byte stream; aligned halfword accesses reduce bus work. */
__attribute__((noinline)) HOT static void vram_span(uint8_t *dst,const uint8_t *src,unsigned n){
 unsigned i=0;
 if(!(((uintptr_t)dst|(uintptr_t)src)&1u))for(;i+1<n;i+=2)*(uint16_t*)(dst+i)=*(const uint16_t*)(src+i);
 for(;i<n;i++)dst[i]=src[i];
}
HOT void z80a_blk_c(uint32_t mode){
 uint32_t hl=Z80A_U32(Z80A_HL)&0xffffu,bc=Z80A_U32(Z80A_BC),f=Z80A_U32(Z80A_F),pc=Z80A_U32(Z80A_PC)&0xffffu;int32_t r=(int32_t)Z80A_U32(Z80A_R6NOW);
 uint32_t b=(bc>>8)&255u,cnt=b?b:256u,n;
 if(mode==2){                                                  /* unrolled upload: OUTI directly followed by more OUTIs (B only counts down) */
  uint32_t nb=(uint32_t)(r+15)/16u;if(nb>512u)nb=512u;n=1;
  while(n<nb){uint32_t x=pc+2u*(n-1u);if(RMAP[x>>10][x]!=0xEDu||RMAP[(x+1u)>>10][x+1u]!=0xA3u)break;n++;}
 }else{uint32_t nb=mode?(uint32_t)(r-1)/26u+1u:(uint32_t)(r+20)/21u;n=cnt<nb?cnt:nb;}
 uint32_t a=vaddr,h=hl,left=n,last=0;
 while(left){
  uint32_t c=1024u-(h&1023u);if(c>left)c=left;if(c>16384u-a)c=16384u-a;
  const uint8_t *src=RMAP[h>>10]+h;uint8_t *dst=vram+a;
  vram_span(dst,src,c);
  last=src[c-1u];vdp_block_dirty(a,c);
  a=(a+c)&16383u;h=(h+c)&0xffffu;left-=c;
 }
 vaddr=a;readbuf=(uint8_t)last;cmd_first=0;
 uint32_t b2=(b-n)&255u;
 f=(f&~0x40u)|2u|(b2?0u:0x40u);
 int32_t r2;
 if(mode==2){r2=r-16*(int32_t)n;pc=(pc+2u*(n-1u))&0xffffu;}
 else if(mode==0){if(n==cnt)r2=r-21*(int32_t)n+5;else{r2=r-21*(int32_t)n;pc=(pc-2u)&0xffffu;}}
 else{
  int32_t ra=r-26*(int32_t)(n-1u)-16;
  if(ra<=0){r2=ra;}                                            /* the slice ended after the OUTI: the JP NZ has not run */
  else if(n==cnt){r2=ra-10;pc=(pc+3u)&0xffffu;}                /* last byte: JP NZ falls through */
  else{r2=ra-10;pc=(pc-2u)&0xffffu;}                           /* JP NZ taken, back to the OUTI */
 }
 Z80A_U32(Z80A_HL)=h;Z80A_U32(Z80A_BC)=(bc&~0xff00u)|(b2<<8);Z80A_U32(Z80A_F)=f;Z80A_U32(Z80A_PC)=pc;Z80A_U32(Z80A_R6NOW)=(uint32_t)r2;
}

HOT static void sram_write(unsigned a,uint8_t v){unsigned off=(a&16383)+((banks[0]&4)?16384:0);sram[off]=v;if(off<sram_lim)*(volatile uint8_t*)(sram_base+off)=v;}
/* called by the core for writes it cannot complete itself: mapper registers (RAM already updated) and cartridge SRAM */
/* A mapper write changes just one 16-KiB window. Keep the other
 * mappings and fixed first KiB in place; page-0 mirrors follow bank 1. */
__attribute__((noinline,minsize)) HOT static void update_mapper_window(unsigned bi,uint8_t old){
 unsigned lo=32,hi=48;const uint8_t *p;
 if(bi==1){lo=1;hi=16;p=cart_bank(banks[1]);}
 else if(bi==2){lo=16;hi=32;p=cart_bank(banks[2])-0x4000;}
 else{
  if(bi==3&&(banks[0]&8))return;
  if(bi==0&&!((old^banks[0])&12))return;
  p=(banks[0]&8)?sram+((banks[0]&4)?16384:0)-0x8000:cart_bank(banks[3])-0x8000;
 }
 #pragma clang loop unroll(disable)
 for(unsigned i=lo;i<hi;i++)RMAP[i]=p;
 if(bi==1){RMAP[65]=RMAP[66]=RMAP[67]=p-0x10000;}
 if(bi==0)Z80A_U8(Z80A_SRAMEN)=(banks[0]&8)?1:0;
}
HOT void z80a_wr_slow(uint32_t a,uint32_t v){
 if(a>=0xfffc){unsigned bi=a&3;uint8_t old=banks[bi];banks[bi]=(uint8_t)v;if(old!=v)update_mapper_window(bi,old);}
 else if(a>=0x8000&&a<0xc000&&(banks[0]&8))sram_write(a,(uint8_t)v);
}
__attribute__((noinline)) static uint8_t psc8_z80_in(uint8_t p);
__attribute__((noinline)) static void psc8_z80_out(uint8_t p,uint8_t v);
static void audio_catchup(void);
/* The two port entry points the core calls: VDP-status polling and PSG writes (the busiest ports in PCM / wait loops) are
 * handled here from IWRAM; everything else goes to the ROM-resident general handlers. */
/* CP16 recovery: GG vertical-counter polling is read-only and is used repeatedly
 * by Gaiden. Answer it within the native Z80 port bridge, preserving the
 * exact psc8_z80_in scanline discontinuity at 0xda. */
HOT uint32_t z80a_in_c(uint32_t p){p&=255;if(p-0x80u<0x40u&&(p&1)){cmd_first=0;uint8_t s=status;status=0;Z80A_U8(Z80A_INTP)=0;Z80A_U32(Z80A_INCNT)++;return s;}if(p==0x7eu)return scanline<=0xdau?scanline:scanline-6u;return psc8_z80_in((uint8_t)p);}
HOT void z80a_out_c(uint32_t p,uint32_t v){p&=255;if(p-0x40u<0x40u){audio_catchup();psg_write((uint8_t)v);return;}psc8_z80_out((uint8_t)p,(uint8_t)v);}
static inline uint32_t cyc_now(void){return slice_base+slice_budget-Z80A_U32(Z80A_R6NOW);}
static uint8_t psc8_z80_in(uint8_t p){
 if(mode_gg && p<7){ports_gg++;switch(p){case 0:return (keys&8)?0x40:0xc0;case 1:case 4:return 0x7f;case 2:case 5:case 6:return 0xff;default:return 0;}}
 if(p==0xf2 && !mode_gg)return enable_fm?fm_control:2;
 if(p>=0x40 && p<0x80){if(!(p&1))return scanline<=0xda?scanline:scanline-6;return (cyc_now()%228)>>1;}
 if(p>=0x80 && p<0xc0){cmd_first=0;if(p&1){uint8_t s=status;status=0;Z80A_U8(Z80A_INTP)=0;return s;}uint8_t v=readbuf;readbuf=vram[vaddr];vaddr=(vaddr+1)&16383;return v;}
 if(p>=0xc0){if(!(p&1)){uint8_t v=0xff;if(keys&64)v&=~1;if(keys&128)v&=~2;if(keys&32)v&=~4;if(keys&16)v&=~8;if(keys&1)v&=~16;if(keys&2)v&=~32;return v;}return 0xff;}
 return 0xff;
}
/* ---- audio: block rendering with catch-up at every PSG/FM/stereo write (bit-exact with the per-sample synth) */
static int16_t aud_l[N+8],aud_r[N+8],aud_f[N+8];   /* EWRAM (IWRAM is reserved for code) */
static uint32_t due_cyc,due_ph;                       /* guest cycle at which the next output sample is due */
static unsigned aud_due,aud_done;
static uint8_t psg_frame_used,fm_frame_used;
static inline uint32_t cyc_now(void);
HOT static void audio_render_to(unsigned k){
 if(k<=aud_done)return;
 unsigned n=k-aud_done;
 /* PSG muted by the FM unit's control register (FM-only mode): nothing is audible, so it is not rendered
  * (its phases simply pause, which cannot be heard when it is re-enabled) */
 if(psg_enabled(enable_fm,fm_control)){
  /* Clear lazily: FM-only frames never read or write the PSG accumulators. */
  if(!psg_frame_used){uint32_t *l=(uint32_t*)aud_l,*r=(uint32_t*)aud_r;for(unsigned i=0;i<N/2;i++){l[i]=0;r[i]=0;}psg_frame_used=1;}
  psg_render(aud_l+aud_done,aud_r+aud_done,n,stereo,1);
 }
 if(enable_fm&&(fm_control&1)&&fm_active){fm_frame_used=1;fastopll_render(&fastopll,aud_f+aud_done,n);if(!fastopll_active(&fastopll))fm_active=0;}
 else if(enable_fm)for(unsigned i=aud_done;i<k;i++)aud_f[i]=0;
 aud_done=k;
}
/* Output sample k (k = 0, 1, ...) is due T(k) = 196*k + ((a+k)>>1) guest cycles after due_cyc: 59736/304 = 196.5 cycles per
 * sample, and the Bresenham remainder due_ph = 152*a + b keeps b fixed while a alternates 0/1. So the number of samples due
 * at `now` is found in closed form instead of one loop pass per sample (host-verified against the loop, tools/catchup_test.c). */
HOT static void audio_catchup(void){
 int32_t dl=(int32_t)(cyc_now()-due_cyc);
 if(dl>=0&&aud_due<N){
  unsigned a=due_ph>=152u,b=due_ph-152u*a,m=1;
  if(dl>=196){
   m=((uint32_t)dl*334u)>>16;                         /* ~ 2*dl/393, off by at most one */
   while(196u*m+((a+m)>>1)>(uint32_t)dl)m--;
   while(196u*(m+1)+((a+m+1)>>1)<=(uint32_t)dl)m++;
   m++;
  }
  unsigned room=N-aud_due;if(m>room)m=room;
  due_cyc+=196u*m+((a+m)>>1);due_ph=b+(((a+m)&1u)?152u:0u);aud_due+=m;
 }
 audio_render_to(aud_due);
}
static void psc8_z80_out(uint8_t p,uint8_t v){
 if(mode_gg && p==6){audio_catchup();stereo=v;ports_gg++;return;}
 if(!mode_gg && enable_fm && p>=0xf0 && p<=0xf2){
  ports_fm++;audio_catchup();
  if(p==0xf2){fm_control=v&3;Z80A_U8(Z80A_VALIN+0xf2)=psc8_z80_in(0xf2);}
  else if(p==0xf0){fm_addr=v&0x3f;fastopll_addr(&fastopll,v);}
  else {fastopll_data(&fastopll,v);if((fm_addr>=0x20&&fm_addr<=0x28&&(v&0x10))||(fm_addr==0x0e&&(v&0x1f)))fm_active=1;}
  return;
 }
 if(p>=0x40 && p<0x80){audio_catchup();psg_write(v);return;}
 if(p>=0x80 && p<0xc0){
  if(p&1){if(!cmd_first){cmd_lo=v;cmd_first=1;}else{cmd_first=0;vcode=v>>6;vaddr=(((v&63)<<8)|cmd_lo)&16383;if(vcode==2){unsigned ri=v&15;uint8_t old=vr[ri];vr[ri]=cmd_lo;regs_changed=1;if(ri==2||ri==5)vdp_bases();if(ri==0||ri==1||ri==10)Z80A_U8(Z80A_BREAK)=1;if(ri==2&&old!=cmd_lo){for(unsigned k=0;k<128;k++)map_dirty_bits[k]=0xff;map_base_cache=0xffff;}if(old!=cmd_lo&&(ri==0||ri==1||ri==5||ri==6))sprite_dirty=1;}else if(!vcode){readbuf=vram[vaddr];vaddr=(vaddr+1)&16383;}}}
  else{cmd_first=0;if(vcode==3){cram[vaddr&(mode_gg?63:31)]=v;palette_dirty=1;}else{unsigned a=vaddr;vram[a]=v;unsigned ti=a>>5;tile_dirty_bits[ti>>3]|=(uint8_t)(1u<<(ti&7));unsigned nb=(vr[2]&14u)<<10,delta=(a-nb)&16383u;if(delta<2048u){unsigned e=delta>>1;map_dirty_bits[e>>3]|=(uint8_t)(1u<<(e&7));}unsigned sb=(vr[5]&0x7eu)<<7,sd=(a-sb)&16383u;if(sd<64u||(sd>=128u&&sd<256u))sprite_dirty=1;}vaddr=(vaddr+1)&16383;readbuf=v;}
 }
}
/* tile and name-table conversion run from IWRAM (dirty-driven) */
__attribute__((noinline)) HOT static void gfx_tiles(void){
 volatile uint32_t *bg=(void*)0x06000000,*obj=(void*)0x06010000;
 for(unsigned group=0;group<16;group++){uint32_t mask=((uint32_t*)tile_dirty_bits)[group];if(!mask)continue;((uint32_t*)tile_dirty_bits)[group]=0;for(unsigned part=0;part<4;part++){unsigned k=group*4+part,bits=mask&255u;mask>>=8;if(!bits)continue;
  for(unsigned b=0;b<8;b++)if(bits&(1u<<b)){unsigned t=k*8+b;const uint32_t *row=(const uint32_t*)(vram+t*32);
   for(unsigned y=0;y<8;y++){uint32_t w=row[y];uint32_t v=spread[w&255]|(spread[(w>>8)&255]<<1)|(spread[(w>>16)&255]<<2)|(spread[w>>24]<<3);bg[t*8+y]=v;obj[t*8+y]=v;}}}}
}
__attribute__((noinline,minsize)) HOT static void gfx_map(unsigned base){
 for(unsigned group=0;group<32;group++){uint32_t mask=((uint32_t*)map_dirty_bits)[group];if(!mask)continue;((uint32_t*)map_dirty_bits)[group]=0;for(unsigned part=0;part<4;part++){unsigned k=group*4+part,bits=mask&255u;mask>>=8;if(!bits)continue;for(unsigned b=0;b<8;b++)if(bits&(1u<<b)){unsigned i=k*8+b,off=(base+i*2)&16383;unsigned v=vram[off]|(vram[(off+1)&16383]<<8);unsigned tile=(v&511)|((v&0x600)<<1)|((v&0x800)<<1);R16(0x06008000+i*2)=tile;R16(0x06008800+i*2)=(v&0x1000)?tile:0x200;}}}
}
static void gfx(void){
 gfx_tiles();
 if(palette_dirty){palette_dirty=0;volatile uint16_t *pal=(void*)0x05000000;for(unsigned i=0;i<32;i++){unsigned r,g,b;if(mode_gg){unsigned v=cram[i*2]|(cram[i*2+1]<<8);r=(v&15)*2+((v&15)>>3);g=((v>>4)&15)*2+(((v>>4)&15)>>3);b=((v>>8)&15)*2+(((v>>8)&15)>>3);}else{unsigned v=cram[i];r=(v&3)*10+((v&3)==3);g=((v>>2)&3)*10+(((v>>2)&3)==3);b=((v>>4)&3)*10+(((v>>4)&3)==3);}unsigned v=r|(g<<5)|(b<<10);pal[(i>>4)*16+(i&15)]=v;pal[256+(i&15)]=pal[16+(i&15)];}}
 unsigned base=(vr[2]&14)<<10;
 if(base!=map_base_cache){map_base_cache=(uint16_t)base;for(unsigned k=0;k<128;k++)map_dirty_bits[k]=0xff;}
 gfx_map(base);
 unsigned x=(256-vr[8]+(mode_gg?48:8))&255,y=(vr[9]+(mode_gg?24:16))%224;
 /* GBA window offsets: GG's 160x144 image is centered at (40,8). */
 if(mode_gg){x=(x-40)&255;y=(y-8)&255;}
 R16(0x04000010)=R16(0x04000014)=x;R16(0x04000012)=R16(0x04000016)=y;
 if(sprite_dirty){sprite_dirty=0;unsigned sat=(vr[5]&0x7e)<<7,count=0;volatile uint16_t *oam=(void*)0x07000000;for(unsigned i=0;i<64;i++){
  unsigned sy=vram[(sat+i)&16383];if(sy==208)break;sy=(sy+1)&255;int dy=(int)(int8_t)sy;if(sy<224)dy=sy;dy-=(mode_gg?24:16);dy+=(mode_gg?8:0);
  int dx=vram[(sat+128+i*2)&16383]-(mode_gg?48:8)+(mode_gg?40:0);if(vr[0]&8)dx-=8;
  unsigned tile=vram[(sat+129+i*2)&16383]|((vr[6]&4)<<6),tall=vr[1]&2;if(tall)tile&=~1u;
  if(dx<-8 || dx>=240 || dy<-16 || dy>=160)continue;
  unsigned n=count++*4;oam[n]=(dy&255)|(tall?0x8000:0);oam[n+1]=dx&511;oam[n+2]=tile|0x400;
 }for(unsigned i=count;i<128;i++)oam[i*4]=0x200;}
 R16(0x04000000)=0x40|0x100|0x200|0x1000|0x2000;
 R16(0x04000048)=0x13;R16(0x0400004a)=0;
 R16(0x04000040)=mode_gg?0x28c8:0x00f0;R16(0x04000044)=mode_gg?0x0898:0x00a0;
 if(!(vr[1]&0x40))R16(0x04000048)=0;
}
static void rearm(unsigned b,unsigned skip){
 R16(0x040000c6)=0;R16(0x040000d2)=0;
 R32(0x040000bc)=(uint32_t)&pcm[b][0][skip];R32(0x040000c0)=0x040000a0;R32(0x040000c4)=0xb6400004;
 R32(0x040000c8)=(uint32_t)&pcm[b][1][skip];R32(0x040000cc)=0x040000a4;R32(0x040000d0)=0xb6400004;
}
static void irq(void){if(R16(0x04000202)&0x10){unsigned next=(playbuf+1u)&(NBUF-1);if(ready[next]){playbuf=next;ready[next]=0;rearm(playbuf,32);}else{late_frames++;R16(0x040000c6)=0;R16(0x040000d2)=0;R32(0x040000bc)=R32(0x040000c8)=(uint32_t)silence;R32(0x040000c4)=R32(0x040000d0)=0xb7400004;}audio_frame++;}R16(0x04000202)=0x10;R16(0x03007ff8)|=0x10;}
static void audio_start(void){
 R16(0x04000084)=0x80;R16(0x04000082)=0x9a0c;R16(0x04000080)=0;R16(0x04000088)=0x200;
 R32(0x03007ffc)=(uint32_t)irq;R16(0x04000200)=0x10;R16(0x04000202)=0xffff;
 rearm(0,0);R32(0x04000104)=0x00c4fed0;R32(0x04000100)=0x0080fc64;R16(0x04000208)=1;
 __asm__ volatile("mrs r0,cpsr\n bic r0,r0,#0x80\n msr cpsr_c,r0":::"r0","memory");
}
HOT static void cpu_until(unsigned target){
 /* HALT can skip directly to the next scheduler boundary only while no IRQ/NMI
  * is pending. Otherwise let the core service the interrupt and batch-run the
  * remaining instructions to the boundary. */
 int32_t budget=(int32_t)(target-zcyc);if(budget<=0)return;
 if(Z80A_U8(Z80A_HALTED) && !Z80A_U8(Z80A_INTP)){zcyc=target;return;}
 slice_base=zcyc;slice_budget=(uint32_t)budget;
 int32_t rem=z80a_run(budget);zcyc=target-(uint32_t)rem;
}
/* Runs the guest for one frame in slices that end only at events: line 192 (vblank), the line-counter
 * interrupt line (when enabled) and the frame end. Line bookkeeping for the lines in between is done
 * after each slice; the V counter is computed by the core from the cycles elapsed in the slice. */
int psc8_idle_probe(unsigned vdp_status,uint32_t avail,uint32_t *used);   /* idle_probe.c (ROM): is the guest in a pure loop or a counter loop? */
static uint8_t idle_armed;static uint32_t idle_probes,idle_skips;
/* Called at the start of every slice with the time the slice may use. The core counts reads of constant ports (keys) and VDP status polls;
 * a slice with hundreds of them was a polling loop. If the probe proves the guest is in an endless pure loop (returns 1) the slice is passed
 * without being executed; if it proves a counter loop (timeout / delay, returns 2) the counter is lowered and guest time advanced by the
 * passes it skipped, and the rest of the slice runs normally. Either way the next slice is probed too. */
#ifdef PROBE_CHECK   /* debug build: every counter skip is replayed by the assembly core from the saved state and compared (heartbeat words 20/21) */
static uint32_t chk_n,chk_bad;
static const uint16_t chk_off[11]={Z80A_A,Z80A_F,Z80A_BC,Z80A_DE,Z80A_HL,Z80A_PC,Z80A_SP,Z80A_AF2,Z80A_BC2,Z80A_DE2,Z80A_HL2};
static const uint16_t chk_mask[11]={255,255,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff};
#endif
HOT static int idle_slice(uint32_t avail){
 unsigned cnt=Z80A_U32(Z80A_INCNT);Z80A_U32(Z80A_INCNT)=0;
 if(cnt<128u&&!idle_armed)return 0;
 idle_armed=0;
 if(!avail||Z80A_U8(Z80A_HALTED)||Z80A_U8(Z80A_INTP))return 0;
#ifdef PROBE_CHECK
 uint32_t pre[11],post[11];for(unsigned i=0;i<11;i++)pre[i]=Z80A_U32(chk_off[i]);
#endif
 uint32_t used;idle_probes++;int r=psc8_idle_probe(status,avail,&used);
#ifdef PROBE_CHECK
 if(r==2){
  for(unsigned i=0;i<11;i++){post[i]=Z80A_U32(chk_off[i]);Z80A_U32(chk_off[i])=pre[i];}
  Z80A_U32(Z80A_VLINE0)=0;Z80A_U32(Z80A_VOFF)=0;slice_base=zcyc;slice_budget=used;
  z80a_run((int32_t)used);chk_n++;
  for(unsigned i=0;i<11;i++){if((Z80A_U32(chk_off[i])&chk_mask[i])!=(post[i]&chk_mask[i])){chk_bad++;break;}}
  for(unsigned i=0;i<11;i++)Z80A_U32(chk_off[i])=post[i];
  Z80A_U32(Z80A_INCNT)=0;
 }
#endif
 idle_armed=(uint8_t)r;idle_skips+=(r!=0);if(r==2)zcyc+=used;return r;
}
__attribute__((noinline)) HOT static void frame_cpu(unsigned target){
 unsigned line=0,linecounter=vr[10];uint32_t line0=zcyc;
 while((int32_t)(zcyc-target)<0){
  unsigned ev=262;
  if(line<192){ev=192;if(vr[0]&16){unsigned l2=line+linecounter+1u;if(l2<ev)ev=l2;}}
  uint32_t evc=line0+ev*228u;if((int32_t)(evc-target)>0)evc=target;
  Z80A_U32(Z80A_VLINE0)=line;Z80A_U8(Z80A_BREAK)=0;
  int32_t av=(int32_t)(evc-zcyc);
  if(idle_slice(av>0?(uint32_t)av:0u)==1&&av>0)zcyc=evc;
  else{Z80A_U32(Z80A_VOFF)=zcyc-(line0+line*228u);cpu_until(evc);}
  while(line<262&&(int32_t)(zcyc-(line0+(line+1u)*228u))>=0){
   scanline=++line;
   if(line==192){status|=128;if(vr[1]&32)(Z80A_U8(Z80A_INTP)=1,Z80A_U8(Z80A_INTDATA)=0xff);}
   if(line<=192){if(!linecounter--){linecounter=vr[10];if(vr[0]&16)(Z80A_U8(Z80A_INTP)=1,Z80A_U8(Z80A_INTDATA)=0xff);}}
  }
 }
}
__attribute__((noinline)) HOT static void frame_audio_begin(void){
 due_cyc=sample_next;due_ph=sample_phase;
 { uint32_t sn=sample_next,ph=sample_phase; /* the frame always spans exactly N sample periods */
   ph+=(59736u%N)*N; uint32_t adv=59736u/N*N+ph/N; ph%=N; sample_next=sn+adv; sample_phase=ph; }
 aud_due=aud_done=0;psg_frame_used=fm_frame_used=0;
}
__attribute__((noinline)) HOT static void frame_audio_end(unsigned b){
 audio_render_to(N);
 int8_t *pl=pcm[b][0],*pr=pcm[b][1];
 if(!psg_frame_used){for(unsigned i=0;i<N;i++){int f=aud_f[i]>>3;pl[i]=pr[i]=(int8_t)(f<-128?-128:f>127?127:f);}return;}
 if(!fm_frame_used){for(unsigned i=0;i<N;i++){int l=aud_l[i]>>6,r=aud_r[i]>>6;pl[i]=(int8_t)(l<-128?-128:l>127?127:l);pr[i]=(int8_t)(r<-128?-128:r>127?127:r);}return;}
 for(unsigned i=0;i<N;i++){int f=aud_f[i]>>3;f=(f>127?127:f<-128?-128:f)*64;int l=(aud_l[i]+f)>>6,r=(aud_r[i]+f)>>6;pl[i]=(int8_t)(l<-128?-128:l>127?127:l);pr[i]=(int8_t)(r<-128?-128:r>127?127:r);}
}
static void frame(unsigned b){
 keys=(~R16(0x04000130))&0x3ff;
 /* ports whose value is constant for the whole frame are answered by the core itself (IN fast path) */
 if(keys!=keys_tab){keys_tab=keys;for(unsigned p=0;p<0x100;p++){unsigned fast=(p<0x40)||(p>=0xc0);Z80A_U8(Z80A_FASTIN+p)=(uint8_t)fast;if(fast)Z80A_U8(Z80A_VALIN+p)=psc8_z80_in((uint8_t)p);}}
 unsigned target=cycles_next+59736u;cycles_next=target;
 frame_audio_begin();
 frame_cpu(target);
 frame_audio_end(b);
 frames++;gfx();
 volatile uint32_t*d=(void*)0x0203e000;d[0]=0x50534338;d[1]=frames;d[2]=Z80A_U32(Z80A_PC);d[3]=ports_fm;d[4]=ports_gg;d[5]=audio_frame;d[6]=late_frames;d[7]=zcyc;d[8]=(uint32_t)&fastopll;d[9]=fm_control;d[10]=stereo;d[11]=Z80A_U32(Z80A_BC);d[12]=Z80A_U32(Z80A_DE);d[13]=Z80A_U32(Z80A_HL);d[14]=idle_probes;d[15]=idle_skips;{extern uint32_t psc8_probe_dbg[4];d[16]=psc8_probe_dbg[0];d[17]=psc8_probe_dbg[1];d[18]=psc8_probe_dbg[2];d[19]=psc8_probe_dbg[3];}
#ifdef PROBE_CHECK
 d[20]=chk_n;d[21]=chk_bad;
#endif
#ifdef PSC8_DEBUG_STOP   /* test builds: freeze at the end of guest frame N (N in the word at 0x0203e0f0) so that tools/blktest.py can compare machine states */
 {volatile uint32_t *st=(void*)0x0203e0f0;if(*st&&frames>=*st)for(;;);}
#endif

}
/* Only immutable, byte-verified Gaiden kernels can use native spans.
 * A remapped page falls back to the regular core at each entry. */
static void setup_gaiden_spans(void){
 Z80A_U32(0x1C4)=0;
 if(!mode_gg||cart_size!=262144u)return;
 {static const uint8_t expected[]={0xff,0xc5,0x6,0x8,0x7d,0xcb,0x4,0x38,0x5,0xdd,0x7e,0x0,0xdd,0x23,0x12,0x13,0x10,0xf2};
  for(unsigned i=0;i<sizeof expected;i++)if(cart_page[0][1076+i]!=expected[i])return;}
 {static const uint8_t expected[]={0x6,0x8,0xfd,0x7e,0x0,0xfd,0x23,0xad,0xcb,0x4,0x38,0x5,0xdd,0x7e,0x0,0xdd,0x23,0x12,0x13,0x10,0xed};
  for(unsigned i=0;i<sizeof expected;i++)if(cart_page[0][1124+i]!=expected[i])return;}
 {static const uint8_t expected[]={0xf5,0xd5,0x3a,0xe5,0xc5,0xcb,0x3f,0xcb,0x3f,0xcb,0x3f,0xed,0x44,0x82,0xc6,0x6,0xe6,0x1f,0x57,0x3a,0xe6,0xc5,0xcb,0x3f,0xcb,0x3f,0xcb,0x3f,0x83,0xc6};
  for(unsigned i=0;i<sizeof expected;i++)if(cart_page[0][1209+i]!=expected[i])return;}
 Z80A_U32(0x1C4)=(uint32_t)cart_page[0];
}
void psc8_main(void){
 const uint32_t *config=(void*)0x0203ff00;cart=(void*)config[0];cart_size=config[1];mode_gg=config[2]&1;enable_fm=(config[2]&2)&&!mode_gg;if(config[2]&8){sram_base=0x0e004000u;sram_lim=16384u;}                    /* PSI: 16 KiB bank at 0x0E004000 */
 else if(config[2]&0x10){                                                  /* Phantasy Star Gaiden: first 4 KiB of the cartridge SRAM at 0x0E006000 */
  volatile uint8_t*m=(volatile uint8_t*)0x0e007f10u;sram_base=0x0e006000u;sram_lim=4096u;
  if(m[0]!='G'||m[1]!='G'||m[2]!='S'||m[3]!='V'||m[4]!=1){for(unsigned i=0;i<4096u;i++)((volatile uint8_t*)sram_base)[i]=0xffu;m[4]=1;m[3]='V';m[2]='S';m[1]='G';m[0]='G';}
 }
 cart_mask=cart_size-1;for(unsigned i=0;i<8192;i++)((uint32_t*)cart_page)[i]=((const uint32_t*)cart)[i];banks[1]=0;banks[2]=1;banks[3]=2;stereo=0xff;noise_lfsr=0x8000;
 update_map();
 for(unsigned i=0;i<32768u;i++)sram[i]=0xffu;for(unsigned i=0;i<sram_lim;i++)sram[i]=*(volatile uint8_t*)(sram_base+i);
 for(unsigned i=0;i<4;i++)volumes[i]=15;
 for(unsigned v=0;v<256;v++){uint32_t s=0;for(unsigned bit=0;bit<8;bit++)s|=((v>>(7-bit))&1)<<(bit*4);spread[v]=s;}
 for(unsigned i=0;i<64;i++)tile_dirty_bits[i]=0xff;for(unsigned i=0;i<128;i++)map_dirty_bits[i]=0xff;palette_dirty=sprite_dirty=1;
 R16(0x04000204)=0x4317;R16(0x04000000)=0x80;R16(0x04000008)=0x1002;R16(0x0400000a)=0x1100;
 for(unsigned i=0;i<8192;i++)R32(0x06000000+i*4)=0;
 for(unsigned i=0;i<8192;i++)R32(0x06010000+i*4)=0;
 fastopll_reset(&fastopll);
 z80a_setup();z80a_reset();update_map();setup_gaiden_spans();zcyc=0;
 Z80A_U32(Z80A_VRAMPTR)=(uint32_t)vram;Z80A_U32(Z80A_TILEBITS)=(uint32_t)tile_dirty_bits;Z80A_U32(Z80A_MAPBITS)=(uint32_t)map_dirty_bits;sprite_dirty=1;vdp_bases();
 frame(0);frame(1);memcpy(pcm[0][0]+N,pcm[1][0],32);memcpy(pcm[0][1]+N,pcm[1][1],32);ready[1]=1;audio_start();
 uint32_t seen=0;
 unsigned prod=2;(void)seen;
 for(;;){
  unsigned b=prod;
  if(b==playbuf||ready[b])continue;                  /* ring full: wait for the audio IRQ to consume a buffer */
  frame(b);
  unsigned prev=(b-1u)&(NBUF-1);memcpy(pcm[prev][0]+N,pcm[b][0],32);memcpy(pcm[prev][1]+N,pcm[b][1],32);
  ready[b]=1;prod=(b+1u)&(NBUF-1);
 }
}
