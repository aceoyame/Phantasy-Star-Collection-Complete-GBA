/* Genesis runtime: bus, VDP and scheduler (CP08).
 *
 * VDP/bus behaviour is the CP07 behaviour (the 8 adventures all initialise and
 * idle correctly with it); what changed:
 *   - VRAM/RAM are stored as host 16-bit words so name-table/tile fetches are
 *     single loads (byte a lives at ((u8*)md_vram)[a^1]);
 *   - every store that changes renderer-visible state bumps M.vgen/M.cgen, so
 *     the renderer can skip frames in which nothing changed;
 *   - counters/milestones record what the guest has done (debug menu);
 *   - the scheduler is resumable, so the debug menu can single-step it.
 */
#ifndef MD_ZTRACE_RD
#define MD_ZTRACE_RD(a) ((void)0)
#endif
#ifndef MD_ZTRACE
#define MD_ZTRACE(a,v) ((void)0)   /* host-only hook: bus_write8 traffic to the Z80 area / YM2612 / Z80 control registers */
#endif
#include "md.h"
#include "mdsnd.h"
#ifdef AOT_ON
extern u32 aot_rgen[128];
#define RGEN(a) do{ if(((a)&0xFFFFu)<0x8000u) AOT_RGEN_ADD(((a)>>8)&0x7Fu,1u); }while(0)
#else
#define RGEN(a) do{}while(0)
#endif

static const u8*kern_seen;
static u32 kern_valid;
static const u8*class_seen;
static u32 class_valid;
static u16 glyph_lut[256] __attribute__((aligned(4)));
static u32 glyph_lut_key=0x10000u;
#include "native_class_index.h"
#include "native_kern_index.h"
u16 md_ram[32768] __attribute__((aligned(4)));
u16 md_vram[32768] __attribute__((aligned(4)));
u8  md_zram[8192] __attribute__((aligned(4)));
u16 md_cram[64];
u16 md_vsram[64] __attribute__((aligned(4)));
MD M HOTDATA;
u32 md_vdirty[64];             /* 2048 bits: 32-byte VRAM blocks changed since the renderer last looked (see mdrender.c) */
#define VDIRTY(byteaddr) (md_vdirty[((byteaddr)&0xFFFFu)>>10]|=1u<<((((byteaddr)&0xFFFFu)>>5)&31u))

#define ROMBYTE(a) (m68k_romp[(a)>>12][(a)&0xFFFu])
#define IS_VDP(a)  (((a)&0xE700E0u)==0xC00000u)

static inline u32 bswap16(u32 v){ return ((v&0xFFu)<<8)|((v>>8)&0xFFu); }

/* ---------------- VDP ---------------- */
u32 md_vdp_status(void){
  u32 s=0x3400u|0x200u; if(M.line>=224) s|=8; if(M.vint_pending) s|=0x80; return s;
}
static void vdp_data_write(u32 v);

static inline void vram_put16(u32 a,u32 v){        /* a: address register value, v: word as written */
  u32 i=(a&0xFFFEu)>>1;
  if(a&1u) v=bswap16(v);
  if(md_vram[i]!=(u16)v){ md_vram[i]=(u16)v; M.vgen++; VDIRTY(i<<1); }
  M.n_vram_wr++;
}
static inline void cram_put(u32 i,u32 v){ v&=0xEEEu; if(md_cram[i]!=(u16)v){ md_cram[i]=(u16)v; M.cgen++; } M.n_cram_wr++; }
static inline void vsram_put(u32 i,u32 v){ v&=0x7FFu; if(md_vsram[i]!=(u16)v){ md_vsram[i]=(u16)v; M.vgen++; M.rfull=1; } M.n_vsram_wr++; }

static inline u32 dma_read(u32 src){
  src&=0xFFFFFEu;
  if(src<m68k_romsize){ u32 h=*(const u16*)(m68k_romp[src>>12]+(src&0xFFFu)); return ((h&0xFFu)<<8)|(h>>8); }
  if(src>=0xE00000u) return md_ram[(src&0xFFFFu)>>1];
  return bus_read16(src);
}

ROMFN __attribute__((noinline)) static void vdp_dma(void){
  u32 mode=M.reg[23]>>6, len=((u32)M.reg[20]<<8)|M.reg[19]; if(!len) len=0x10000u;
  MD_EVENT(EV_DMA); M.n_dma++;
  if(mode<=1u){
    u32 src=(((u32)M.reg[23]&0x7Fu)<<17)|((u32)M.reg[22]<<9)|((u32)M.reg[21]<<1);
    M.dma_src=src; M.dma_len=len; M.dma_dst=M.addr; M.n_dma_words+=len;
    u32 c=M.code&0xFu;
    if(c==1u&&M.reg[15]==2u&&!(M.addr&1u)){
      u32 a=M.addr, chg=0;
      for(u32 i=0;i<len;i++){ u32 w=dma_read(src); src+=2u; u32 idx=(a&0xFFFFu)>>1; if(md_vram[idx]!=(u16)w){ md_vram[idx]=(u16)w; chg=1; VDIRTY(idx<<1); } a+=2u; }
      if(chg) M.vgen++;
      M.addr=a; M.n_vram_wr+=len; MD_EVENT(EV_VRAM);
    } else {
      for(u32 i=0;i<len;i++){ u32 w=dma_read(src); src+=2u; vdp_data_write(w); }
    }
    M.reg[19]=M.reg[20]=0;
  } else if(mode==2u){ M.fill_pending=1; }
  else { /* VRAM copy */
    u32 src=((u32)M.reg[22]<<8)|M.reg[21]; u8*p=(u8*)md_vram; u32 chg=0; M.n_copy++;
    for(u32 i=0;i<len;i++){ u8 b=p[(src&0xFFFFu)^1u]; u8*d=&p[(M.addr&0xFFFFu)^1u]; if(*d!=b){ *d=b; chg=1; VDIRTY(M.addr); } src++; M.addr+=M.reg[15]; }
    if(chg) M.vgen++;
  }
}
ROMFN __attribute__((noinline)) static void vdp_fill(u32 v){
  u32 len=((u32)M.reg[20]<<8)|M.reg[19]; if(!len) len=0x10000u;
  M.fill_pending=0; M.n_fill++;
  if((M.code&0xFu)==1u){
    u32 a=M.addr&0xFFFEu;
    if(md_vram[a>>1]!=(u16)v){ md_vram[a>>1]=(u16)v; M.vgen++; VDIRTY(a); }
    u8*p=(u8*)md_vram; u8 b=(u8)(v>>8); u32 inc=M.reg[15], addr=M.addr, chg=0;
    for(u32 i=0;i<len;i++){ u8*d=&p[addr&0xFFFFu]; if(*d!=b){ *d=b; chg=1; VDIRTY(addr); } addr+=inc; }
    M.addr=addr; if(chg) M.vgen++;
  } else {
    for(u32 i=0;i<len;i++){
      if((M.code&0xFu)==3u) cram_put((M.addr>>1)&63u,v); else if((M.addr>>1)<40u) vsram_put(M.addr>>1,v);
      M.addr+=M.reg[15];
    }
  }
}
static void vdp_data_write(u32 v){
  M.n_data_wr++;
  if(M.fill_pending){ vdp_fill(v); return; }
  switch(M.code&0xFu){
    case 1: vram_put16(M.addr,v); MD_EVENT(EV_VRAM); break;
    case 3: cram_put((M.addr>>1)&63u,v); MD_EVENT(EV_CRAM); break;
    case 5: { u32 i=(M.addr>>1)&63u; if(i<40u) vsram_put(i,v); MD_EVENT(EV_VSRAM); } break;
  }
  M.addr+=M.reg[15];
}
/* CP12: translated longword transfers commonly write both halves to the
 * VDP data port.  Keep their order/counters/generations, but classify the
 * bus and data target once, like PSIV's direct device path. */
void md_vdp_write16(u32 v){ C.fx++; vdp_data_write((u16)v); }
void md_vdp_write32(u32 v){
  C.fx+=2u;
  if(!M.fill_pending&&(M.code&0xFu)==1u){
    u32 a=M.addr, inc=M.reg[15];
    vram_put16(a,v>>16);
    vram_put16(a+inc,(u16)v);
    M.addr=a+2u*inc; M.n_data_wr+=2u; MD_EVENT(EV_VRAM);
    return;
  }
  vdp_data_write(v>>16); vdp_data_write((u16)v);
}
/* CP13: the shared 0x3218..0x3226 routine transfers RAM longwords to the
 * VDP.  Both memories already have the same word layout: copy native words
 * directly instead of reversing them through RD32/WR32 for every MOVE.
 * Return 0 without touching state for any uncommon bus/VDP configuration. */
/* The transfer loop runs in IWRAM; guards and once-per-batch accounting
 * remain in ROM. Even addresses use native halfword loads, including two-byte alignment. */
/* PSIV-style native 32-bit transfer: both guest memories already store the
 * original word order, so an aligned longword needs one read, compare, and
 * store rather than two halfword cycles.  The guest's changed-halfword count
 * remains exact (it feeds M.vgen), and the dirty tile is marked only once
 * when both halves of a longword change.  Fall back for halfword-aligned but
 * not word-aligned addresses; none of the wrap or MMIO guards are relaxed. */
typedef u32 __attribute__((may_alias)) MDNativeWord;
ROMFN __attribute__((noinline,used)) static u32 vram_words_unaligned(u32 s,u32 d,u32 n){
  const u16 *in=md_ram+(s>>1);u16 *out=md_vram+(d>>1);u32 changed=0;
  for(u32 i=0;i<n*2u;i++){
    u32 v=in[i];if(out[i]==v)continue;
    out[i]=(u16)v;changed++;VDIRTY(d+i*2u);
  }
  return changed;
}
/* CP56: guarded transfers contain 1..8 longwords and touch at most two
 * tile bits. Gather those two bits and commit each dirty word once.
 * The native loop still compares/counts both guest halfwords separately. */
#ifndef MD_HOST
HOT __attribute__((naked,noinline)) static u32 vram_native_words(u32 s,u32 d,u32 n){
  __asm__ volatile(
    "orr r3,r0,r1\n"
    "tst r3,#3\n"
    "bne vram_words_unaligned\n"
    "push {r4-r9,lr}\n"
    "mov r8,r1\n"
    "and r6,r1,#28\n"
    "rsb r6,r6,#32\n"
    "mov r6,r6,lsr #2\n"
    "mov r3,#0\n"
    "mov r7,#0\n"
    "ldr r9,=0xffff\n"
    "ldr r4,=md_ram\n"
    "add r0,r4,r0\n"
    "ldr r4,=md_vram\n"
    "add r1,r4,r1\n"
    "1:\n"
    "ldr r4,[r0],#4\n"
    "ldr r5,[r1]\n"
    "eors r5,r5,r4\n"
    "beq 2f\n"
    "str r4,[r1]\n"
    "tst r5,r9\n"
    "addne r3,r3,#1\n"
    "movs r5,r5,lsr #16\n"
    "addne r3,r3,#1\n"
    "cmp r6,#0\n"
    "orrgt r7,r7,#1\n"
    "orrle r7,r7,#2\n"
    "2:\n"
    "add r1,r1,#4\n"
    "sub r6,r6,#1\n"
    "subs r2,r2,#1\n"
    "bne 1b\n"
    "cmp r7,#0\n"
    "beq 4f\n"
    "ldr r0,=md_vdirty\n"
    "mov r1,r8,lsr #10\n"
    "add r0,r0,r1,lsl #2\n"
    "mov r1,r8,lsr #5\n"
    "and r1,r1,#31\n"
    "mov r2,#1\n"
    "mov r2,r2,lsl r1\n"
    "tst r7,#1\n"
    "beq 3f\n"
    "ldr r4,[r0]\n"
    "orr r4,r4,r2\n"
    "str r4,[r0]\n"
    "3:\n"
    "tst r7,#2\n"
    "beq 4f\n"
    "movs r2,r2,lsl #1\n"
    "moveq r2,#1\n"
    "addeq r0,r0,#4\n"
    "ldr r4,[r0]\n"
    "orr r4,r4,r2\n"
    "str r4,[r0]\n"
    "4:\n"
    "mov r0,r3\n"
    "pop {r4-r9,lr}\n"
    "bx lr\n"
    ".ltorg\n");
}
#else
HOT __attribute__((noinline)) static u32 vram_native_words(u32 s,u32 d,u32 n){
  if((s|d)&3u)return vram_words_unaligned(s,d,n);
  const MDNativeWord *src=(const MDNativeWord*)(md_ram+(s>>1));
  MDNativeWord *dst=(MDNativeWord*)(md_vram+(d>>1));
  u32 changed=0,dirty=0,start=(d&31u)>>2;
  for(u32 i=0;i<n;i++){
    u32 v=src[i],diff=v^dst[i];
    if(diff){dst[i]=v;changed+=((diff&65535u)!=0u)+((diff>>16)!=0u);
      dirty|=1u<<(start+i>=8u);}
  }
  if(dirty&1u)VDIRTY(d);
  if(dirty&2u)VDIRTY(d+32u);
  return changed;
}
#endif

extern void WR8(u32 a,u32 v);
extern void WR32(u32 a,u32 v);
extern u32 aot_rgen[128];
/* AOT of the shared graphics bit loop, including its ADD.B leaf call.
 * Only complete iterations within the original event budget are combined.
 * The stack and destination must be ordinary, nonoverlapping RAM; all
 * writes still use WR8/WR32 to retain bus and RAM-code generation counts. */
ROMFN __attribute__((noinline)) u32 md_native_decode_bits(u32 room,u32 sr,u32 hi){
  u32 sp=(C.a[7]-4u)&0xFFFFFEu,d=C.a[3]&0xFFFFFFu;
  if(room<52u||C.a[1]!=0x3A8Eu||sp<0xE00000u||(sp&0xFFFFu)>=0xFFFCu||d<0xE00000u||(d&0xFFFFu)>0xFFF8u)return 0;
  u32 ds=d&0xFFFFu,ss=sp&0xFFFFu;
  if(ds<ss+4u&&ss<ds+8u)return 0;
  if(room>=416u&&(C.d[1]&0xFFFFu)>=7u){
    /* Four pixels per native word, with the guest's halfword byte order.
     * This is the original eight ADD.B/call/store/DBRA iterations as one
     * native operation; final flags and every write generation are kept. */
    static const u32 pixels[16]={0x02020202u,0x02010202u,0x01020202u,0x01010202u,0x02020201u,0x02010201u,0x01020201u,0x01010201u,0x02020102u,0x02010102u,0x01020102u,0x01010102u,0x02020101u,0x02010101u,0x01020101u,0x01010101u};
    u32 v=C.d[0]&255u;
    WR32(C.a[7]-4u,hi|0x3A64u);
    if(!(ds&3u)){
      MDNativeWord *p=(MDNativeWord*)(md_ram+(ds>>1));
      p[0]=pixels[v>>4];p[1]=pixels[v&15u];
    }else for(u32 i=0;i<8u;i++)((u8*)md_ram)[(ds+i)^1u]=(v&(128u>>i))?1u:2u;
    if(ss<0x8000u)AOT_RGEN_ADD(ss>>8,7u);
    if(ss+2u<0x8000u)AOT_RGEN_ADD((ss+2u)>>8,7u);
    for(u32 i=0;i<8u;i++)if(ds+i<0x8000u)AOT_RGEN_ADD((ds+i)>>8,1u);
    C.fx+=15u;C.a[3]+=8u;C.d[0]&=~255u;
    C.d[1]=(C.d[1]&0xFFFF0000u)|((C.d[1]-8u)&0xFFFFu);
    C.d[3]=(v&1u)?1u:2u;C.sr=(u16)((sr&~31u)|4u|((v&1u)?19u:0u));
    u32 ones=(v&0x55u)+((v>>1)&0x55u);
    ones=(ones&0x33u)+((ones>>2)&0x33u);ones=(ones+(ones>>4))&15u;
    return 416u-2u*ones;
  }
  u32 used=0,n=0;
  do{
    u32 v=C.d[0]&255u,r=(v<<1)&255u,carry=v>>7;
    WR32(C.a[7]-4u,hi|0x3A64u);
    C.d[0]=(C.d[0]&~255u)|r;
    sr=(sr&~31u)|(carry?17u:0u)|(((v^r)&128u)?2u:0u)|(!r?4u:0u)|(r&128u?8u:0u);
    C.d[3]=carry?1u:2u;
    WR8(C.a[3],C.d[3]);C.a[3]++;
    C.d[1]=(C.d[1]&0xFFFF0000u)|((C.d[1]-1u)&0xFFFFu);
    used+=carry?50u:52u;n++;
  }while(n<8u&&room-used>=52u&&(C.d[1]&0xFFFFu)!=0xFFFFu);
  C.sr=(u16)sr;return used;
}

HOT __attribute__((noinline)) int md_native_vram_copy(u32 src,u32 n){
  if(!n||n>8u)return 0;
  src&=0xFFFFFEu;
  /* Decompiled 3218..3226 transfer also feeds CRAM every displayed frame.
   * Preserve all guest writes, halfword masks, wrapping and generations. */
  if((M.code&15u)==3u&&!M.fill_pending&&src>=0xe00000u&&
     (src&65535u)+n*4u<=65536u){
    u32 addr=M.addr,inc=M.reg[15];const u16 *in=md_ram+((src&65535u)>>1);
    for(u32 j=0;j<n*2u;j++){
      cram_put((addr>>1)&63u,in[j]);addr+=inc;
    }
    M.addr=addr;M.n_data_wr+=n*2u;C.fx+=n*2u;MD_EVENT(EV_CRAM);
    return 1;
  }
  u32 s=src&0xFFFFu,d=M.addr&0xFFFFu,bytes=n*4u;
  if(src<0xE00000u||(d&1u)||s+bytes>0x10000u||d+bytes>0x10000u||
     M.fill_pending||(M.code&15u)!=1u||M.reg[15]!=2u) return 0;
  M.vgen+=vram_native_words(s,d,n);M.n_vram_wr+=n*2u;M.n_data_wr+=n*2u;
  M.addr+=bytes;C.fx+=n*2u;MD_EVENT(EV_VRAM);
  return 1;
}

/* CP54: complete constant longword writes to vertical scroll RAM. */
__attribute__((section(".aotmore")))
#ifndef MD_HOST
__attribute__((target("thumb")))
#endif
__attribute__((noinline)) int md_native_vsram_fill(u32 value,u32 n){
 if(!n||M.fill_pending||(M.code&15u)!=5u)return 0;
 u32 a=M.addr,inc=M.reg[15],v=value,changes=0,writes=0;
 /* CP55: contiguous aligned scroll longwords share one native transfer.
  * Count changed halfwords individually, including unchanged repeats. */
 if(inc==2u&&!(a&3u)&&n<=20u&&((a&127u)+n*4u)<=80u){
  u32 packed=((value>>16)&0x7ffu)|((value&0x7ffu)<<16);
  MDNativeWord *p=(MDNativeWord *)(void *)(md_vsram+((a>>1)&63u));
  for(u32 k=0;k<n;k++){
   u32 old=p[k];if(old!=packed){p[k]=packed;
    changes+=((old&65535u)!=(packed&65535u))+((old>>16)!=(packed>>16));}
  }
  a+=n*4u;writes=n*2u;
 }else for(u32 k=0;k<n*2u;k++){
  u32 ix=(a>>1)&63u;
  if(ix<40u){u32 word=(v>>16)&0x7FFu;
   if(md_vsram[ix]!=(u16)word){md_vsram[ix]=(u16)word;changes++;}writes++;}
  a+=inc;v=(v<<16)|(v>>16);
 }
 M.vgen+=changes;M.n_vsram_wr+=writes;if(changes)M.rfull=1;
 M.n_data_wr+=n*2u;M.addr=a;C.fx+=n*2u;MD_EVENT(EV_VSRAM);return 1;
}

__attribute__((section(".aotmore"),noinline))
#ifndef MD_HOST
__attribute__((target("thumb")))
#endif
u32 md_native_scroll_batch(u32 room){
 u32 count=C.d[2]&65535u,n=room>>5;
 u32 max=count>12u?count-12u:count<12u?count+1u:0u;
 if(n>max)n=max;
 if(!n||(C.a[6]&0xFFFFFEu)!=0xC00000u||!md_native_vsram_fill(C.d[0],n))return 0;
 u32 last=count-n+1u,res=(last-12u)&65535u;
 u32 flags=(last<12u?1u:0u)|(res&32768u?8u:0u)|
      ((((last^12u)&(last^res)&32768u)!=0u)?2u:0u);
 C.d[2]=(C.d[2]&0xFFFF0000u)|((count-n)&65535u);
 return (n<<5)|flags;
}

/* Decompiled 34B0..34CE: three rotate/extract/multiply/shift components per
 * palette word. Whole 370-cycle colors only; incomplete colors retain AOT.
 * Includes full products and dead-register residue, CCR and RAM generations. */
ROMFN __attribute__((noinline)) u32 md_native_palette_fade(u32 room,u32 sr){
 if(room<370u)return 0;
 u32 src=C.a[1]&0xfffffeu,dst=C.a[0]&0xfffffeu;
 if(src<0xe00000u||dst<0xe00000u)return 0;
 u32 s=src&65535u,d=dst&65535u,used=0,factor=C.d[2]&65535u;
 do{
  u32 v=md_ram[s>>1];
  u32 pr=((v>>8)&15u)*factor,pg=((v>>4)&15u)*factor,pb=(v&15u)*factor;
  u32 last=(pb&65535u)>>6;
  u32 color=(u16)((((pr&65535u)>>6)<<8)|(((pg&65535u)>>6)<<4)|last);
  md_ram[d>>1]=(u16)color;C.fx++;
  if(d<0x8000u)AOT_RGEN_ADD(d>>8,1u);
  C.d[0]=(C.d[0]&0xffff0000u)|v;
  C.d[1]=(pb&0xffff0000u)|last;C.d[3]=color;C.d[5]=65535u;
  C.d[4]=(C.d[4]&0xffff0000u)|((C.d[4]-1u)&65535u);
  C.a[0]+=2u;C.a[1]+=2u;s+=2u;d+=2u;used+=370u;
  sr=(sr&~31u)|((pb&32u)?17u:0u)|(last?0u:4u);
 }while((C.d[4]&65535u)!=65535u&&room-used>=370u&&s<65536u&&d<65536u);
 C.sr=(u16)sr;return used;
}

/* Decompiled 34A6..34A8: complete 24-cycle RAM longword copy/DBRA pairs.
 * Each source word is read before its destination is written, including
 * forward overlap. RGEN counts both halfwords; FX counts one longword. */
HOT __attribute__((noinline)) u32 md_native_palette_copy(u32 room){
 u32 src=C.a[1]&0xfffffeu,dst=C.a[0]&0xfffffeu;
 if(room<24u||src<0xe00000u||dst<0xe00000u||((src|dst)&3u))return 0;
 u32 s=src&65535u,d=dst&65535u,used=0,count=C.d[4]&65535u,n=0;
 if(s>=0xfffcu||d>=0xfffcu)return 0;
 do{
  *(MDNativeWord*)((u8*)md_ram+d)=*(const MDNativeWord*)((const u8*)md_ram+s);
  if(d<0x8000u)AOT_RGEN_ADD(d>>8,2u);
  s+=4u;d+=4u;count=(count-1u)&65535u;n++;used+=24u;
 }while(count!=65535u&&room-used>=24u&&s<0xfffcu&&d<0xfffcu);
 C.fx+=n;C.a[0]+=n*4u;C.a[1]+=n*4u;
 C.d[4]=(C.d[4]&0xffff0000u)|count;
 return used;
}

/* Complete 28-cycle MOVE.W/ADD.W/MOVE.W/DBRA iterations at 0x248A.
 * The source is immutable during VDP writes. No RAM/code writes are elided;
 * each VDP word still uses the normal device path, including fill/CRAM/VSRAM.
 * The trace annotation is only used by the host oracle, never the GBA build. */
#ifndef MD_NATIVE_WORD_CLOCK
#define MD_NATIVE_WORD_CLOCK(offset) ((void)0)
#endif
ROMFN __attribute__((noinline)) u32 md_native_nametable(u32 room,u32 sr){
  u32 src=C.a[0]&0xFFFFFEu,count=(C.d[0]&65535u)+1u;
  if(room<56u||count<2u||((C.a[6]-4u)&0xFFFFFEu)!=0xC00000u)return 0;
  u32 ram=src>=0xE00000u;
  if(!ram&&src>=m68k_romsize)return 0;
  u32 used=0,n=0,out=0,sum=0,add=C.d[3]&65535u;
  while(n<count&&room-used>=28u){
    u32 a=src+n*2u,value;
    if(ram){if((src&65535u)+n*2u>=65536u)break;value=md_ram[(a&65535u)>>1];}
    else {if(a+1u>=m68k_romsize)break;const u8*p=m68k_romp[a>>12]+(a&4095u);value=((u32)p[0]<<8)|p[1];}
    sum=value+add;out=sum&65535u;
    MD_NATIVE_WORD_CLOCK(used+24u);
    md_vdp_write16(out);used+=28u;n++;
  }
  MD_NATIVE_WORD_CLOCK(0u);
  if(!n)return 0;
  C.a[0]+=n*2u;C.d[1]=(C.d[1]&0xFFFF0000u)|out;
  C.d[0]=(C.d[0]&0xFFFF0000u)|((C.d[0]-n)&65535u);
  C.sr=(u16)((sr&~31u)|((sum&65536u)?16u:0u)|(out?0u:4u)|((out&32768u)?8u:0u));
  return used;
}

/* Shared 0x3B9C four-byte tile packer, including its real stack return.
 * This 88-cycle leaf fits a scanline budget. Reads precede the destination
 * write; the return address is read afterwards, retaining stack overlap. */
ROMFN __attribute__((noinline)) u32 md_native_pack_tile(u32 room,u32 sr){
  u32 src=C.a[2]&0xFFFFFFu,dst=C.a[4]&0xFFFFFEu,sp=C.a[7]&0xFFFFFEu;
  u32 ram=src>=0xE00000u,vd=dst==0xC00000u||dst==0xC00002u;
  if(room<88u||sp<0xE00000u||(sp&65535u)>65530u||(!vd&&dst<0xE00000u))return 0;
  if(ram){if((src&65535u)>65532u)return 0;}
  else if(src>=m68k_romsize||m68k_romsize-src<4u)return 0;
  u32 b0,b1,b2,b3;
  if(ram){u32 o=src&65535u;b0=((const u8*)md_ram)[(o)^1u];b1=((const u8*)md_ram)[(o+1u)^1u];b2=((const u8*)md_ram)[(o+2u)^1u];b3=((const u8*)md_ram)[(o+3u)^1u];}
  else {b0=ROMBYTE(src);b1=ROMBYTE(src+1u);b2=ROMBYTE(src+2u);b3=ROMBYTE(src+3u);}
  u32 out=((b0<<12)|(b1<<8)|(b2<<4)|b3)&65535u;
  C.a[2]+=4u;C.d[0]=(C.d[0]&0xFFFF0000u)|out;
  MD_NATIVE_WORD_CLOCK(76u);
  if(vd)md_vdp_write16(out);
  else {md_ram[(dst&65535u)>>1]=(u16)out;C.fx++;RGEN(dst);}
  MD_NATIVE_WORD_CLOCK(0u);
  C.pc=((u32)md_ram[(sp&65535u)>>1]<<16)|md_ram[((sp&65535u)>>1)+1u];C.a[7]+=4u;
  C.sr=(u16)((sr&~31u)|(b0&16u)|(out?0u:4u)|((out&32768u)?8u:0u));
  return 88u;
}

static u32 vdp_data_read(void){
  u32 v=0; M.n_data_rd++;
  switch(M.code&0xFu){
    case 0: v=md_vram[(M.addr&0xFFFEu)>>1]; break;
    case 8: v=md_cram[(M.addr>>1)&63u]; break;
    case 4: v=((M.addr>>1)&63u)<40u?md_vsram[(M.addr>>1)&63u]:0u; break;
  }
  M.addr+=M.reg[15]; return v;
}
static void vdp_reg_write(u32 r,u32 v){
  u8 old=M.reg[r]; M.reg[r]=(u8)v; M.n_reg_wr++; MD_EVENT(EV_REG);
  if(old!=(u8)v&&((MD_REG_RENDER_MASK>>r)&1u)){ M.vgen++; M.rfull=1; }
  if(r==1u){ if((v&0x40u)&&!(old&0x40u)) MD_EVENT(EV_DISPON); if(v&0x20u) MD_EVENT(EV_VINT_EN); }
}
__attribute__((noinline)) static void vdp_ctrl_write(u32 v){
  M.n_ctrl_wr++; MD_EVENT(EV_CTRL);
  if(M.pending){
    M.pending=0; M.addr=((u32)M.first&0x3FFFu)|((v&3u)<<14); M.code=(u8)((((u32)M.first>>14)&3u)|((v>>2)&0x3Cu));
    if((M.code&0x20u)&&(M.reg[1]&0x10u)) vdp_dma();
    return;
  }
  if((v&0xC000u)==0x8000u){ u32 r=(v>>8)&0x1Fu; if(r<24u) vdp_reg_write(r,v); M.code=0; return; }
  M.first=(u16)v; M.pending=1; M.addr=(M.addr&0xC000u)|(v&0x3FFFu); M.code=(u8)((M.code&0x3Cu)|((v>>14)&3u));
}


/* ---------------- serial EEPROM (X24C01 protocol, 128 bytes) ----------------
 * The SEGANET adventures keep their save in a 24C01-class EEPROM bit-banged through 0x200001:
 * write bit0 = SDA, bit1 = SCL; read bit0 = SDA (open drain: master AND device).  Mode-1 X24C01
 * framing: START, 7-bit word address + R/W, ACK, data bytes (4-byte write pages), STOP.
 * Every completed byte write is mirrored to GBA SRAM (md_eeprom_store in mdmain.c). */
enum { EE_IDLE, EE_ADDR, EE_WRITE, EE_READ };
EEP md_ee;
__attribute__((noinline,section(".text.ee_write"))) static void ee_write(u32 v){   /* cold: runs from ROM, not IWRAM */
  u32 sda=v&1u, scl=(v>>1)&1u, osda=md_ee.sda_in, oscl=md_ee.scl;
  md_ee.sda_in=(u8)sda; md_ee.scl=(u8)scl;
  if(oscl&&scl){
    if(osda&&!sda){ md_ee.state=EE_ADDR; md_ee.bit=0; md_ee.shift=0; md_ee.sda_out=1; md_ee.rise=0; return; }   /* START */
    if(!osda&&sda){ md_ee.state=EE_IDLE; md_ee.sda_out=1; return; }                               /* STOP */
    return;
  }
  if(md_ee.state==EE_IDLE) return;
  if(!oscl&&scl){                                       /* rising edge: sample */
    md_ee.rise=1;
    if(md_ee.bit<8u){ if(md_ee.state!=EE_READ) md_ee.shift=(u8)((md_ee.shift<<1)|sda); }
    else if(md_ee.state==EE_READ&&sda) md_ee.nack=1;     /* master did not acknowledge: stop sending */
    return;
  }
  if(oscl&&!scl){                                       /* falling edge: advance (only after a clock pulse: the SCL fall that follows START is not a data clock) */
    if(!md_ee.rise) return;
    md_ee.rise=0;
    md_ee.bit++;
    if(md_ee.bit==8u){
      if(md_ee.state==EE_ADDR){ md_ee.addr=(u8)((md_ee.shift>>1)&0x7Fu); md_ee.rw=md_ee.shift&1u; md_ee.sda_out=0; }
      else if(md_ee.state==EE_WRITE){ u32 a=md_ee.addr&0x7Fu; if(md_ee.mem[a]!=md_ee.shift){ md_ee.mem[a]=md_ee.shift; md_eeprom_store(a,md_ee.shift); }
        md_ee.addr=(u8)((a&~3u)|((a+1u)&3u)); md_ee.sda_out=0; }
      else md_ee.sda_out=1;                              /* READ: release SDA for the master's ACK */
    } else if(md_ee.bit>=9u){
      md_ee.bit=0; md_ee.shift=0; md_ee.sda_out=1;
      if(md_ee.state==EE_ADDR) md_ee.state=md_ee.rw?EE_READ:EE_WRITE, md_ee.nack=0;
      if(md_ee.state==EE_READ){
        if(md_ee.nack){ md_ee.state=EE_IDLE; return; }
        md_ee.rbyte=md_ee.mem[md_ee.addr&0x7Fu]; md_ee.addr=(u8)((md_ee.addr+1u)&0x7Fu); md_ee.sda_out=(md_ee.rbyte>>7)&1u;
      }
    } else if(md_ee.state==EE_READ) md_ee.sda_out=(md_ee.rbyte>>(7u-md_ee.bit))&1u;
  }
}
static inline u32 ee_read(void){ return (u32)(md_ee.sda_in&md_ee.sda_out); }

/* ---------------- bus ---------------- */
static u32 io_read(u32 a){
  a&=0x1Fu; M.n_io_rd++;
  if(a==0x01) return 0xA0;
  if(a==0x03){ /* port 1 */
    u32 th=M.pad_th&0x40u,p=M.pad,r;
    if(th){ r=0x7Fu; if(p&1) r&=~1u; if(p&2) r&=~2u; if(p&4) r&=~4u; if(p&8) r&=~8u; if(p&16) r&=~0x10u; if(p&32) r&=~0x20u; }
    else  { r=0x33u; if(p&1) r&=~1u; if(p&2) r&=~2u; if(p&64) r&=~0x10u; if(p&128) r&=~0x20u; }
    return r|0x80u;
  }
  if(a==0x05||a==0x07) return 0x7F;
  if(a==0x09) return M.ctrl1;
  return 0;
}
u8 bus_read8(u32 a){
  a&=0xFFFFFFu;
  if(a<m68k_romsize) return ROMBYTE(a);
  if(a>=0xE00000u) return ((u8*)md_ram)[(a&0xFFFFu)^1u];
  if(a>=0xA04000u&&a<0xA06000u) return 0;
  if(a>=0xA00000u&&a<0xA10000u){ MD_ZTRACE_RD(a); return md_zram[a&0x1FFFu]; }
  if(a>=0xA10000u&&a<0xA10020u) return (u8)io_read(a);
#ifdef EE_TRACE
  if((a&0xFFFFFEu)==0x200000u) fprintf(stderr,"EER -> %u (state %u bit %u sda_out %u pc %06x)\n",ee_read(),md_ee.state,md_ee.bit,md_ee.sda_out,C.pc);
#endif
  if((a&0xFFFFFEu)==0x200000u) return (a&1u)?(u8)ee_read():0u;
  if(a==0xA11100u||a==0xA11101u) return 0x00;
  if(IS_VDP(a)){ u32 w=bus_read16(a&~1u); return (a&1u)?(u8)w:(u8)(w>>8); }
  M.n_unm_rd++; M.last_unm_rd=a; return 0xFF;
}
u16 bus_read16(u32 a){
  a&=0xFFFFFEu;
  if(a<m68k_romsize){ u32 h=*(const u16*)(m68k_romp[a>>12]+(a&0xFFFu)); return (u16)(((h&0xFFu)<<8)|(h>>8)); }
  if(a>=0xE00000u) return md_ram[(a&0xFFFFu)>>1];
  if(IS_VDP(a)){
    u32 p=a&0x1Fu;
    if(p<4){ C.fx++; return (u16)vdp_data_read(); }
    if(p<8){ if(M.pending){ C.fx++; M.pending=0; } M.n_status_rd++; C.lrd++; return (u16)md_vdp_status(); }
    if(p<0xC){ C.fx++; M.n_hv_rd++; return (u16)((((M.line>0xEA?M.line-6:M.line)&0xFF)<<8)); }
    return 0;
  }
  return (u16)((bus_read8(a)<<8)|bus_read8(a+1u));
}
u32 bus_read32(u32 a){ return ((u32)bus_read16(a)<<16)|bus_read16(a+2u); }
void bus_write8(u32 a,u8 v){
  a&=0xFFFFFFu; C.fx++;
  if(a>=0xE00000u){ ((u8*)md_ram)[(a&0xFFFFu)^1u]=v; RGEN(a); return; }
  if(a==0xA130F1u){ M.sram_on=v&1u; return; }
#ifdef EE_TRACE
  if((a&0xFFFFFEu)==0x200000u) fprintf(stderr,"EEW f%u %06x <- %02x (pc %06x)\n",M.frame,a,v&0xFF,C.pc);
#endif
  if(a==0x200001u){ ee_write(v); return; }
  if(a>=0xA04000u&&a<0xA06000u){ M.n_ym_wr++; mdsnd_ym_68k(a,v); MD_ZTRACE(a,v); return; }
  if(a>=0xA00000u&&a<0xA10000u){ md_zram[a&0x1FFFu]=v; M.n_z80_wr++; MD_ZTRACE(a,v); return; }
  if(a>=0xA10000u&&a<0xA10020u){ u32 r=a&0x1Fu; M.n_io_wr++; if(r==0x03) M.pad_th=v; if(r==0x09) M.ctrl1=v; return; }
  if(IS_VDP(a)){ if((a&0x1Fu)>=0x10u&&(a&0x1Fu)<0x18u){ M.n_psg_wr++; mdsnd_psg_68k(v); MD_ZTRACE(a,v); return; } bus_write16(a&~1u,(u16)(v|(v<<8))); return; }
  if(a==0xA11100u){ mdsnd_busreq(v); return; }
  if(a==0xA11200u){ mdsnd_reset(v); return; }
  M.n_unm_wr++; M.last_unm_wr=a; MD_ZTRACE(a,v);
}
void bus_write16(u32 a,u16 v){
  a&=0xFFFFFEu; C.fx++;
  if(a>=0xE00000u){ md_ram[(a&0xFFFFu)>>1]=v; RGEN(a); return; }
  if(IS_VDP(a)){ u32 p=a&0x1Fu; if(p<4) vdp_data_write(v); else if(p<8) vdp_ctrl_write(v); else if(p>=0x10u&&p<0x18u){ M.n_psg_wr++; mdsnd_psg_68k(v); MD_ZTRACE(a|0x1000000u,v); } return; }
  bus_write8(a,(u8)(v>>8)); bus_write8(a+1u,(u8)v);
}
void bus_write32(u32 a,u32 v){ bus_write16(a,(u16)(v>>16)); bus_write16(a+2u,(u16)v); }

/* ---------------- machine ---------------- */
ROMFN void md_reset_game(void){
  kern_seen=0;kern_valid=0;class_seen=0;class_valid=0;glyph_lut_key=0x10000u;
  u32*z;
  z=(u32*)md_ram;  for(u32 i=0;i<sizeof md_ram/4;i++) z[i]=0;
  z=(u32*)md_vram; for(u32 i=0;i<sizeof md_vram/4;i++) z[i]=0;
  z=(u32*)md_zram; for(u32 i=0;i<sizeof md_zram/4;i++) z[i]=0;
  for(u32 i=0;i<64;i++){ md_cram[i]=0; md_vsram[i]=0; }
  z=(u32*)&M; for(u32 i=0;i<sizeof M/4;i++) z[i]=0;
  for(u32 i=0;i<EV_COUNT;i++) M.ev[i]=0xFFFFFFFFu;
  M.pad_th=0x40; M.vgen=1; M.cgen=1; M.rfull=1;
  for(u32 i=0;i<64;i++) md_vdirty[i]=0;
  md_ee.state=0; md_ee.bit=0; md_ee.sda_in=1; md_ee.sda_out=1; md_ee.scl=1; md_ee.rise=0; md_ee.armed=0;   /* EEPROM contents survive a game reset */
  C.spin_enable=1;
  m68k_reset();
}
ROMFN void md_init_machine(const u32*table,u32 size){
  for(u32 i=0;i<128u;i++) m68k_romp[i]=(i<(size>>12))?(const u8*)(table[i]&~1u):(const u8*)0;
  m68k_romsize=size; m68k_ram=md_ram; m68k_init_tables();
  md_eeprom_load();
  md_reset_game();
}
ROMFN void md_begin_frame_input(u32 keys){
  M.pad=(u16)(((keys>>6)&3u)|((keys&32u)>>3)|((keys&16u)>>1)|((keys&2u)<<3)|((keys&1u)<<5)|((keys&256u)>>2)|((keys&8u)<<4));
}

/* Whole-scanline fast forward.  Called after a scanline when the CPU is provably idle: either STOPped, or the last
 * m68k_exec fast-forwarded a loop whose iterations touch no memory/VDP/IO state and read nothing that depends on
 * the scanline (C.idle_ok).  Nothing but an interrupt can change such a CPU, so the scanlines up to the next one
 * where an interrupt could be raised are executed as one chunk: the line-start bookkeeping is applied in closed
 * form and the CPU gets a single multi-line limit, which the loop fast-forward consumes in one division-free step.
 * The result is bit-identical to running the lines one by one (checked per frame by `mdhost spin`). */
static void idle_skip(void){
  u32 mask=(C.sr>>8)&7u; s32 ln=M.next_line, stop=MD_LINES-1;           /* the last line is left to the normal path (frame end) */
  if((M.reg[1]&32u)&&mask<6u&&ln<=224&&stop>224) stop=224;              /* VINT is raised at the start of line 224 */
  if((M.reg[0]&16u)&&mask<4u&&ln<=224){ s32 hl=ln+M.hc; if(hl<=224&&hl<stop) stop=hl; }   /* line where the HINT counter underflows */
  s32 k=stop-ln; if(k<2) return;
  if(ln<=224&&stop-1>=224) M.vint_pending=1;
  { s32 last=stop-1<224?stop-1:224, dec=last-ln+1;                       /* lines on which the HINT counter runs */
    if(dec>0){
      s32 h=M.hc;
      if(dec<=h) M.hc=h-dec;
      else { s32 r=M.reg[10],rem=dec-h-1; M.hint_pending=1; if(r==0) M.hc=0; else { while(rem>=r+1) rem-=r+1; M.hc=r-rem; } }
    }
    if(stop-1>224) M.hc=M.reg[10];                                      /* below the active area the counter is reloaded every line */
  }
  M.line=stop-1; M.next_line=stop; M.line_start=M.line_end; M.line_end+=(u32)k*MD_LINE_CYCLES;
  if(C.stopped) C.cycles=M.line_end; else m68k_exec(M.line_end);
}


/* ---------------- save arming ----------------
 * The SEGANET adventures keep their save behind a small library (ROM 0x4A7A..): the cold-start code at ROM 0x3B6 loads the
 * EEPROM into the RAM shadow at 0xFFF960, validates its 18 checksummed blocks, stores the "blocks reset" mask at 0xFFFA22 and
 * sets 0xFFFFE8 = 0x1234, which is what lets the once-per-frame write-back (0x4AAE, called from the vertical-blank handler)
 * run.  The cartridge's real entry point (0x214 -> 0x600) never passes through that code, so without help the save was never
 * loaded and the write-back stayed disabled for ever (nothing was ever written to the EEPROM, whatever the EEPROM model did).
 * Once the RAM image has been launched we do exactly what 0x3B6 does: fill the shadow from the (already SRAM-backed) EEPROM
 * image, run the game's own validation (0x4D3A) on the reference interpreter outside guest time, then arm the write-back. */
__attribute__((noinline,section(".text.md_arm"))) static void md_arm_save(void){
  if((bus_read32(0xFFFCD0u)>>16)!=0x00FFu||bus_read16(0xFFFBD2u)!=0x4EF9u) return;       /* RAM image not launched yet */
  md_ee.armed=1;
  u32 d[8],a[8],usp=C.usp,ssp=C.ssp,pc=C.pc,cyc=C.cycles; u16 sr=C.sr; u8 stopped=C.stopped,spin=C.spin_enable;
  for(u32 i=0;i<8;i++){ d[i]=C.d[i]; a[i]=C.a[i]; }
  for(u32 i=0;i<64u;i++) bus_write16(0xFFF960u+2u*i,(u16)((md_ee.mem[2u*i]<<8)|md_ee.mem[2u*i+1u]));
  C.spin_enable=0; C.stopped=0; C.sr=0x2700; C.a[7]-=4u; bus_write32(C.a[7],0x400u); u32 sp=C.a[7]+4u;
  C.pc=0x4D3Au; C.d[6]=0;
  for(u32 n=0;n<400000u&&!(C.pc==0x400u&&C.a[7]==sp);n++) ref_step();
  u32 d7=C.d[7];
  bus_write16(0xFFFFE8u,0x1234u); bus_write32(0xFFFA22u,d7);
  for(u32 i=0;i<8;i++){ C.d[i]=d[i]; C.a[i]=a[i]; }
  C.usp=usp; C.ssp=ssp; C.pc=pc; C.sr=sr; C.cycles=cyc; C.stopped=stopped; C.spin_enable=spin;
}

int md_run(u32 single){
  for(;;){
    if(!M.line_active){
      s32 ln=M.next_line; M.line=ln; M.line_active=1;
      if(ln==224) M.vint_pending=1;
      if(ln<=224){ if(--M.hc<0){ M.hc=M.reg[10]; M.hint_pending=1; } } else M.hc=M.reg[10];
      M.line_start=M.line_end; M.line_end+=MD_LINE_CYCLES;                /* nominal timing: no drift from instruction overshoot */
    }
    if(M.vint_pending&&(M.reg[1]&32u)&&((C.sr>>8)&7u)<6u){ M.vint_pending=0; C.idle_ok=0; m68k_interrupt(6); M.n_vint++; MD_EVENT(EV_VINT); }
    else if(M.hint_pending&&(M.reg[0]&16u)&&((C.sr>>8)&7u)<4u){ M.hint_pending=0; C.idle_ok=0; m68k_interrupt(4); M.n_hint++; }
    if(C.stopped){ if((s32)(M.line_end-C.cycles)>0) C.cycles=M.line_end; }
    else if((s32)(M.line_end-C.cycles)>0){
      if(single==1u){ m68k_exec(C.cycles+1u); if((s32)(M.line_end-C.cycles)>0) return 0; }
      else m68k_exec(M.line_end);
    }
    if((s32)(C.cycles-M.line_end)>=0){
      M.line_active=0; M.next_line=M.line+1;
      if(M.next_line>=MD_LINES){ M.next_line=0; M.frame++; if(!md_ee.armed&&md_cfg.index>=2u) md_arm_save(); return 1; }
      if(single) return 0;
      if(C.spin_enable&&(C.stopped||C.idle_ok)) idle_skip();
    }
  }
}

/* CP18: ARM7 long-multiply helper for immutable 68000 DIVU #10 sites.
 * The guest cycles/exception/flags are still handled by the AOT source.
 * Math: floor(n/10) = (n * 0xCCCCCCCD) >> 35, for every 32-bit n.
 * Keep in ROM (IWRAM is within 16 bytes of the reserved stack area). */
__attribute__((noinline,section(".text.rom"))) u32 aot_div10_arm(u32 n){
  return (u32)(((unsigned long long)n * 0xCCCCCCCDull) >> 35);
}
/* CP21: exact floor(n / 20) for all 2^32 inputs. ARM7 UMULL is
 * much cheaper than the software division inside hot Genesis VRAM AOT. */
__attribute__((noinline,section(".text.rom"))) u32 aot_div20_arm(u32 n){
  return (u32)(((unsigned long long)n * 0xCCCCCCCDull) >> 36);
}

/* CP23: ARM7 exact reciprocal for the native Genesis VBlank spin wait.
 * The spin's guest cycles and state changes remain in the AOT function. */
__attribute__((noinline,section(".text.rom"))) u32 aot_div18_arm(u32 n){
  return (u32)(((unsigned long long)n * 0x38E38E39ull) >> 34);
}

/* Native AOT byte copy/fill in ordinary 68K RAM. Callers batch only whole
 * MOVE/DBRA pairs before their event limit. Forward overlap is intentional;
 * do not replace this loop with memcpy or memmove. WR8's exact side-effect
 * and RAM-code generation counts are retained, including unchanged bytes. */
#ifndef MD_HOST
__attribute__((target("thumb")))
#endif
ROMFN __attribute__((noinline)) u32 md_native_ram_bytes(u32 src,u32 dst,u32 n,u32 fill){
 src&=0xFFFFFFu;dst&=0xFFFFFFu;
 if(n<4u||dst<0xE00000u||(dst&65535u)+n>65536u)return 0;
 if(fill>255u&&(src<0xE00000u||(src&65535u)+n>65536u))return 0;
 u32 s=src&65535u,d=dst&65535u,d0=d,left=n;
 u8*p=(u8*)md_ram;
 if(fill<=255u){
  u32 word=fill*0x01010101u;
  if(d&1u){p[(d++)^1u]=(u8)fill;left--;}
  if((d&2u)&&left>=2u){md_ram[d>>1]=(u16)word;d+=2u;left-=2u;}
  while(left>=4u){*(MDNativeWord*)(p+d)=word;d+=4u;left-=4u;}
  if(left>=2u){md_ram[d>>1]=(u16)word;d+=2u;left-=2u;}
  while(left--){p[(d++)^1u]=(u8)fill;}
 }else{
  /* Equal byte parity permits forward halfword copies. Four-byte copies
   * also require equal word alignment: a two-byte overlap must continue
   * using halfwords to retain its original propagation. */
  if(!((s^d)&1u)){
   if(d&1u){p[d^1u]=p[s^1u];d++;s++;left--;}
   if(!((s^d)&3u)){
    if((d&2u)&&left>=2u){md_ram[d>>1]=md_ram[s>>1];d+=2u;s+=2u;left-=2u;}
    while(left>=4u){*(MDNativeWord*)(p+d)=*(const MDNativeWord*)(p+s);d+=4u;s+=4u;left-=4u;}
   }
   while(left>=2u){md_ram[d>>1]=md_ram[s>>1];d+=2u;s+=2u;left-=2u;}
  }
  while(left--){p[d^1u]=p[s^1u];d++;s++;}
 }
 C.fx+=n;
 for(u32 a=d0,end=d0+n;a<end&&a<0x8000u;){
  u32 b=(a+256u)&~255u;if(b>end)b=end;if(b>0x8000u)b=0x8000u;
  AOT_RGEN_ADD(a>>8,b-a);a=b;
 }
 return n;
}

/* CP29: native font-row gather, preserving complete 28-cycle MOVE.L,
 * ADDA.W and DBRA iterations. Read both words before either write so all
 * forward/backward overlap retains the original propagation. Unusual
 * mappings and wrapping longwords retain the generated instruction path. */
#ifndef MD_HOST
__attribute__((target("thumb")))
#endif
ROMFN __attribute__((noinline)) u32 md_native_gather_rows(u32 room,u32 sr){
 u32 src=C.a[1],dst=C.a[3]&0xFFFFFEu;
 if(room<56u||dst<0xE00000u)return 0;
 u32 d=dst&65535u,n=(C.d[0]&65535u)+1u,done=0,used=0,v=0;
 s32 stride=(s16)C.d[1];
 /* CP34: common character-row gather (8 native longword rows, fixed
  * positive stride). Read one aligned source longword before writing its
  * aligned destination longword. This is precisely the two guest word reads
  * followed by two word writes in the prior path; it preserves forward
  * overlap propagation, RGEN, CCR, and the fixed 28-cycle guest charge.
  * All partial-event, wrap and unaligned paths retain the CP33 loop. */
 if(n==8u && room>=224u && stride>0 && !(stride&3u) &&
    !(src&3u) && !(d&3u) && d>=0x8000u && d<=0xFFDCu &&
    (src&0xFF0000u)==0xFF0000u && (src&65535u)>=0x8000u &&
    (src&65535u)+7u*(u32)stride+4u<=65536u){
  MDNativeWord *base=(MDNativeWord*)md_ram;
  u32 soff=(src&65535u)>>2,doff=d>>2,step=(u32)stride>>2;
  #pragma clang loop unroll(full)
  for(u32 j=0;j<8u;j++){
   u32 w=base[soff];base[doff++]=w;soff+=step;
   v=(w<<16)|(w>>16);
  }
  C.a[1]+=8u*(u32)stride;C.a[3]+=32u;C.fx+=8u;
  C.d[0]=(C.d[0]&0xFFFF0000u)|((C.d[0]-8u)&65535u);
  C.sr=(u16)((sr&~15u)|(v?0u:4u)|((v&0x80000000u)?8u:0u));
  return 224u;
 }
 while(done<n&&room-used>=28u&&d<0xFFFCu){
  u32 a=src&0xFFFFFEu,s=a&65535u;
  if(a<0xE00000u||s>=0xFFFCu)break;
  u16 hi=md_ram[s>>1],lo=md_ram[(s>>1)+1u];
  md_ram[d>>1]=hi;md_ram[(d>>1)+1u]=lo;
  RGEN(d);RGEN(d+2u);
  v=((u32)hi<<16)|lo;src+=(u32)stride;d+=4u;done++;used+=28u;
 }
 if(!done)return 0;
 C.a[1]=src;C.a[3]+=done*4u;C.fx+=done;
 C.d[0]=(C.d[0]&0xFFFF0000u)|((C.d[0]-done)&65535u);
 C.sr=(u16)((sr&~15u)|(v?0u:4u)|((v&0x80000000u)?8u:0u));
 return used;
}

/* CP29: the companion one-pixel-shifted compositor. Its first write occurs
 * before the second source read, including when the byte ranges overlap.
 * Keep exact 76/86/114/124-cycle pairs and one/two write counts. */
#ifndef MD_HOST
__attribute__((target("thumb")))
#endif
ROMFN __attribute__((noinline)) u32 md_native_shifted_nibbles(u32 room,u32 sr){
 u32 src=C.a[1]&0xFFFFFFu,dst=C.a[2]&0xFFFFFFu;
 if(room<76u||src<0xE00000u||dst<0xE00000u)return 0;
 u32 s=src&65535u,d=dst&65535u,n=(C.d[0]&65535u)+1u;
 if(n>65536u-s)n=65536u-s;if(n>65535u-d)n=65535u-d;
 if(!n)return 0;
 u32 t=C.d[5]&255u,done=0,used=0,lo=0,out=0,x=0,writes=0,flags=0;
 u8*p=(u8*)md_ram;
 while(done<n&&room-used>=76u){
  /* CP33: guest iterations range 76..124 cycles.  Under a full-iteration
   * allowance the old native path remains unchanged.  For short event
   * windows, only ordinary RAM with independent current source/destination
   * may be predicted; a declined preflight makes no guest state changes. */
  if(room-used<124u){
   if(s==d || s==d+1u) break;
   u32 probe=p[s^1u];
   u32 eh=(probe>>4)==t,el=(probe&15u)==t;
   if(room-used<124u-eh*10u-el*38u) break;
  }
  u32 v=p[s^1u],hi=v>>4,eh=hi==t;
  out=p[d^1u];if(!eh)out=(out&240u)|hi;
  p[d^1u]=(u8)out;RGEN(d);writes++;
  lo=p[s^1u]&15u;x=(v&8u)?16u:0u;
  u32 el=lo==t;
  if(!el){lo<<=4;out=(p[(d+1u)^1u]&15u)|lo;p[(d+1u)^1u]=(u8)out;RGEN(d+1u);writes++;x=0;}
  flags=el?(4u|x):((out?0u:4u)|((out&128u)?8u:0u));
  used+=124u-eh*10u-el*38u;s++;d++;done++;
 }
 if(!done)return 0;
 C.a[1]+=done;C.a[2]+=done;C.fx+=writes;
 C.d[0]=(C.d[0]&0xFFFF0000u)|((C.d[0]-done)&65535u);
 C.d[2]=(C.d[2]&~255u)|lo;C.d[3]=(C.d[3]&~255u)|out;
 C.sr=(u16)((sr&~31u)|flags);
 return used;
}

ROMFN __attribute__((noinline)) u32 aot_div12_arm(u32 n){
 return (u32)(((u64)n*0xAAAAAAABu)>>35);
}

/* CP26: exact complete byte blends from the adventure nibble compositor.
 * Guest reads precede the write even when source and destination overlap.
 * Short event budgets and nonordinary RAM keep the generated instruction path. */
ROMFN __attribute__((noinline)) u32 md_native_nibble_blend(u32 room,u32 sr){
 u32 src=C.a[1]&0xFFFFFFu,dst=C.a[2]&0xFFFFFFu;
 if(room<128u||src<0xE00000u||dst<0xE00000u)return 0;
 u32 s=src&65535u,d=dst&65535u,n=(C.d[0]&65535u)+1u;
 if(n>65536u-s)n=65536u-s;
 if(n>65536u-d)n=65536u-d;
 u32 transparent=C.d[5]&255u,used=0,done=0,lo=0,out=0;
 u8*p=(u8*)md_ram;
 while(done<n&&room-used>=128u){
  u32 v=p[s^1u],old=p[d^1u];
  u32 h=v>>4,l=v&15u,eh=h==transparent,el=l==transparent;
  if(eh)h=old>>4;if(el)l=old&15u;
  lo=l;out=(h<<4)|l;p[d^1u]=(u8)out;
  if(d<0x8000u)AOT_RGEN_ADD(d>>8,1u);
  used+=96u+eh*18u+el*14u;s++;d++;done++;
 }
 if(!done)return 0;
 C.a[1]+=done;C.a[2]+=done;C.fx+=done;
 C.d[0]=(C.d[0]&0xFFFF0000u)|((C.d[0]-done)&65535u);
 C.d[2]=(C.d[2]&~255u)|lo;C.d[3]=(C.d[3]&~255u)|out;
 C.sr=(u16)((sr&~31u)|(out?0u:4u)|((out&128u)?8u:0u));
 return used;
}


/* CP27: AOT kerning lookup skips only complete nonmatching 26-cycle pairs.
 * Verify the immutable ROM keys and nonzero values once per mapped page. */
ROMFN __attribute__((noinline)) u32 md_native_kern_scan(u32 room,u32 sr){
 u32 a=C.a[0]&0xFFFFFEu,offset=a-0x2B856u;
 if(room<14u||offset>92u*4u||(offset&3u)||md_cfg.index<2u||md_cfg.index>8u||m68k_romsize<0x2B856u+93u*4u)return 0;
 const u8*page=m68k_romp[0x2Bu],*table=page+0x856u;
 if(kern_seen!=page){
  u32 valid=1;
  for(u32 j=0;j<92u;j++){
   u32 r=kern_records[j],o=(r&65535u)*4u;
   if((((u32)table[o]<<8)|table[o+1])!=(r>>16)||!(table[o+2]|table[o+3])){valid=0;break;}
  }
  if(table[92u*4u+2u]|table[92u*4u+3u])valid=0;
  kern_valid=valid;kern_seen=page;
 }
 if(!kern_valid)return 0;
 u32 begin=offset>>2,target=C.d[1]&65535u,find=(target<<16)|begin,lo=0,hi=92;
 while(lo<hi){u32 m=(lo+hi)>>1;if(kern_records[m]<find)lo=m+1;else hi=m;}
 u32 end=(lo<92u&&(kern_records[lo]>>16)==target)?(kern_records[lo]&65535u):92u,n=end-begin;
#ifdef MD_HOST
 u32 cap=(u32)(((u64)room*0x4EC4EC4FULL)>>35);
#else
 u32 low,high,magic=0x4EC4EC4Fu;
 __asm__("umull %0,%1,%2,%3":"=&r"(low),"=&r"(high):"r"(room),"r"(magic));
 u32 cap=high>>3;
#endif
 if(n>cap)n=cap;
 if(n==end-begin){
  u32 o=end*4u,key=((u32)table[o]<<8)|table[o+1],diff=(target-key)&65535u;
  u32 flags=(target<key)|((((key^target)&(target^diff)&32768u)!=0u)<<1)|((diff&32768u)?8u:0u)|(diff?0u:4u);
  u32 used=n*26u;
  if(target==key&&room-used>=14u){C.a[0]+=n*4u+2u;C.sr=(u16)((sr&~15u)|flags);return (used+14u)|0x40000000u;}
  if(end==92u&&target!=key&&room-used>=24u){C.a[0]+=n*4u+4u;C.sr=(u16)((sr&~15u)|flags|4u);return (used+24u)|0x80000000u;}
 }
 if(!n)return 0;
 u32 o=(begin+n-1u)*4u,key=((u32)table[o]<<8)|table[o+1],diff=(target-key)&65535u;
 u32 flags=(target<key)|((((key^target)&(target^diff)&32768u)!=0u)<<1)|((diff&32768u)?8u:0u);
 C.a[0]+=n*4u;C.sr=(u16)((sr&~15u)|flags);return n*26u;
}
/* Verified immutable class lists: native lookup retains complete 26-cycle
 * nonmatching iterations, then the original 24-cycle match or 14-cycle
 * terminator when that complete exit fits before the next guest event. */
ROMFN __attribute__((noinline)) u32 md_native_class_scan(u32 room,u32 sr){
 u32 a=C.a[1]&0xFFFFFFu,k,begin=a-0x2B7B9u;
 if(room<14u||md_cfg.index<2u||md_cfg.index>8u||m68k_romsize<0x2B7E1u)return 0;
 if(begin<=10u)k=0;
 else if((begin=a-0x2B7C5u)<=19u)k=1;
 else if((begin=a-0x2B7DAu)<=1u)k=2;
 else if((begin=a-0x2B7DDu)<=3u)k=3;
 else return 0;
 const u8*page=m68k_romp[0x2Bu];
 if(class_seen!=page){
  u32 valid=1;
  for(u32 j=0;j<4u;j++)for(u32 q=0;q<=class_len[j];q++)
   if(page[(class_base[j]&4095u)+q]!=class_bytes[j][q])valid=0;
  class_valid=valid;class_seen=page;
 }
 if(!class_valid)return 0;
 u32 target=C.d[1]&255u,end=target<128u?class_first[k][target]:class_len[k];
 if(end<begin)end=(k==1u&&target==76u&&begin<=12u)?12u:class_len[k];
 u32 n=end-begin;
#ifdef MD_HOST
 u32 budget=(u32)(((u64)room*0x4EC4EC4FULL)>>35);
#else
 u32 low,high,magic=0x4EC4EC4Fu;
 __asm__("umull %0,%1,%2,%3":"=&r"(low),"=&r"(high):"r"(room),"r"(magic));
 u32 budget=high>>3;
#endif
 if(n>budget)n=budget;u32 used=n*26u;
 if(n==end-begin){
  u32 exit=end==class_len[k]?14u:24u;
  if(room-used>=exit){C.a[1]+=n+(exit==24u);C.sr=(u16)(sr|4u);return (used+exit)|(exit==24u?0x40000000u:0x80000000u);}
 }
 if(n){C.a[1]+=n;C.sr=(u16)(sr&~4u);}return used;
}

/* Cache only palette arithmetic and instruction costs, never guest pixels.
 * Each packed entry contains output byte and original 80/84/88-cycle cost. */
#ifndef MD_HOST
__attribute__((target("thumb")))
#endif
ROMFN __attribute__((noinline)) static void build_glyph_lut(u32 c0,u32 c1){
 u32 e0=((((c0<<4)|c0)&255u)<<8)|88u,e1=((((c0<<4)|c1)&255u)<<8)|84u;
 u32 e2=((((c1<<4)|c0)&255u)<<8)|84u,e3=((((c1<<4)|c1)&255u)<<8)|80u;
 MDNativeWord*p=(MDNativeWord*)glyph_lut;u32 word=e3|(e3<<16);
 for(u32 i=0;i<128u;i++)p[i]=word;
 for(u32 i=1;i<16u;i++){glyph_lut[i]=(u16)e1;glyph_lut[i<<4]=(u16)e2;}
 glyph_lut[0]=(u16)e0;glyph_lut_key=c0|(c1<<8);
}

/* CP28: expand packed font data into two selected colors. RAM reads precede
 * writes even for overlap; ROM sources stop at their mapped page boundary.
 * Complete guest iterations cost 80, 84 or 88 cycles, including DBRA. */
#ifndef MD_HOST
__attribute__((target("thumb")))
#endif
ROMFN __attribute__((noinline)) u32 md_native_glyph_bytes(u32 room,u32 sr){
 u32 src=C.a[1]&0xFFFFFFu,dst=C.a[0]&0xFFFFFFu;
 if(room<80u||dst<0xE00000u)return 0;
 u32 s=src&65535u,d=dst&65535u,n=(C.d[0]&65535u)+1u,is_ram=src>=0xE00000u;
 const u8*q;
 if(is_ram){q=(const u8*)md_ram;if(n>65536u-s)n=65536u-s;}
 else{
  if(src>=m68k_romsize)return 0;
  q=m68k_romp[src>>12];s=src&4095u;
  if(n>4096u-s)n=4096u-s;if(n>m68k_romsize-src)n=m68k_romsize-src;
 }
 if(n>65536u-d)n=65536u-d;
 u32 c0=C.d[4]&255u,c1=C.d[5]&255u,used=0,done=0,last=0,out=0;
 if(glyph_lut_key!=(c0|(c1<<8)))build_glyph_lut(c0,c1);
 u8*p=(u8*)md_ram;
 while(done<n&&room-used>=80u){
  u32 v=q[is_ram?(s^1u):s],entry=glyph_lut[v],step=entry&255u;
  if(step>room-used)break;
  out=entry>>8;p[d^1u]=(u8)out;
  last=v;used+=step;s++;d++;done++;
 }
 if(!done)return 0;
 for(u32 a=dst&65535u,end=a+done;a<end&&a<0x8000u;){
  u32 b=(a+256u)&~255u;if(b>end)b=end;if(b>0x8000u)b=0x8000u;
  AOT_RGEN_ADD(a>>8,b-a);a=b;
 }
 C.a[1]+=done;C.a[0]+=done;C.fx+=done;
 C.d[0]=(C.d[0]&0xFFFF0000u)|((C.d[0]-done)&65535u);
 C.d[1]=(C.d[1]&~255u)|last;C.d[2]=(C.d[2]&~255u)|(last&15u);C.d[3]=(C.d[3]&~255u)|out;
 C.sr=(u16)((sr&~31u)|(((last&240u)?c1:c0)&16u)|(out?0u:4u)|((out&128u)?8u:0u));return used;
}

/* CP54: native return-from-exception for a RAM stack. Atlas entry and RAM
 * code validation already establish the opcode. Rare bus stacks retain the
 * reference instruction, including all of its diagnostics and bus effects. */
__attribute__((section(".aotmore"),noinline))
#ifndef MD_HOST
__attribute__((target("thumb")))
#endif
u32 md_native_rte(void){
 u32 old=C.sr,a=C.a[7];
 if((a&0xFFFFFFu)>=0xE00000u && !(a&1u) && (a&65535u)<=65528u){
  const u16 *p=md_ram+((a&65535u)>>1);u32 status=p[0],pc=((u32)p[1]<<16)|p[2];
  C.a[7]=a+6u;m68k_set_sr((u16)status);C.pc=pc;C.cycles+=4u;
 }else ref_step();
 return C.stopped||((old^C.sr)&0x2700u)?2u:0u;
}

/* CP58: one native checksum transaction; only complete 16-cycle iterations
 * over aligned ordinary ROM/RAM are batched. Partial instructions and MMIO
 * use the original AOT body. No guest writes, events or IRQ edges occur here. */
typedef u32 __attribute__((may_alias)) ChecksumWord;
__attribute__((section(".aotmore"),noinline))
u32 md_native_checksum(u32 room,u32 sr){
 u32 a=C.a[0]&0xffffffu,count=(C.d[1]&65535u)+1u;
 if(room<32u||(a&3u))return 0;
 u32 n=room>>4;if(n>count)n=count;
 u32 ram=a>=0xe00000u,avail;
 const ChecksumWord*p;
 if(ram){avail=(65536u-(a&65535u))>>2;p=(const ChecksumWord*)(md_ram+((a&65535u)>>1));}
 else {if(a>=m68k_romsize)return 0;avail=(4096u-(a&4095u))>>2;
  u32 tail=(m68k_romsize-a)>>2;if(avail>tail)avail=tail;
  p=(const ChecksumWord*)(m68k_romp[a>>12]+(a&4095u));}
 if((uintptr_t)p&3u)return 0;
 if(n>avail)n=avail;if(n<2u)return 0;
 u32 sum=C.d[0],before=0,value=0;
 for(u32 k=0;k<n;k++){
  u32 v=p[k];
  if(ram)value=(v<<16)|(v>>16);
  else value=((v&255u)<<24)|((v&0xff00u)<<8)|((v>>8)&0xff00u)|(v>>24);
  before=sum;sum+=value;
 }
 C.d[0]=sum;C.d[1]=(C.d[1]&0xffff0000u)|((C.d[1]-n)&65535u);C.a[0]+=n*4u;
 C.sr=(u16)((sr&~31u)|(sum?0u:4u)|((sum>>28)&8u)|((~(before^value)&(before^sum))>>30&2u)|(sum<value?17u:0u));
 return n*16u;
}
