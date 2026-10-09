/* VDP -> GBA Mode 4 renderer (CP08).
 *
 * Output semantics are identical to the original CP07 display(): the 320x224
 * (or 256x224, 240-line) Genesis image is point-sampled to 240x160
 * (x=(dx*W)/240, y=(dy*H)/160), palette indices are CRAM entries 0..63 and the
 * priority/sprite/window rules are unchanged.  What changed is the cost:
 *   - only the 240x160 sampled pixels are computed;
 *   - planes are decoded one 8-pixel cell at a time, and a line whose cells are
 *     all blank skips the per-pixel work entirely;
 *   - the sprite attribute table is walked once per frame, not once per line;
 *   - nothing is rendered unless VRAM/VSRAM/CRAM/registers actually changed
 *     (md.c bumps M.vgen/M.cgen only when a stored value changes);
 *   - the image is double buffered.
 * A generic per-pixel path (render_line_gen) handles the rarer modes
 * (per-column vertical scroll, window plane) and is the semantic reference for
 * tools_host/md_host/mdhost.c, which compares both paths against the original
 * renderer.
 */
#include "md.h"

u32 mdr_vgen=0xFFFFFFFFu, mdr_cgen=0xFFFFFFFFu;
static u32 blank_flag;
/* Conservative tile -> visible eight-line bands; ordinary EWRAM. */
static u32 tile_bands[2048],tile_index_valid,tile_index_age;

#define VB(a)  (((const u8*)md_vram)[((a)&0xFFFFu)^1u])           /* byte at logical VRAM address */
#define VWA(a) ((u32)md_vram[((a)&0xFFFFu)>>1])                   /* word at even logical address */
/* Aligned tile rows are two native halfwords: one word load plus ROR16
 * produces the original big-endian nibble order. */
typedef u32 __attribute__((may_alias)) NativeTileRow;
static inline u32 trow(u32 ta){
  u32 v=((const NativeTileRow*)md_vram)[(ta&0xFFFCu)>>2];
  return (v<<16)|(v>>16);
}
static inline u32 nib_rev(u32 r){
  r=((r>>4)&0x0F0F0F0Fu)|((r&0x0F0F0F0Fu)<<4);
  r=((r>>8)&0x00FF00FFu)|((r&0x00FF00FFu)<<8);
  return (r>>16)|(r<<16);
}

/* guest x for each output column; rebuilt when the width mode changes */
static u16 xm[240] HOTDATA; static u32 xm_w HOTDATA;
static ROMFN void build_xm(u32 w){
  xm_w=w;
  if(w==320u){ for(u32 dx=0;dx<240u;dx++) xm[dx]=(u16)((dx*320u)/240u); }
  else { for(u32 dx=0;dx<240u;dx++) xm[dx]=(u16)((dx*256u)/240u); }
}

/* ---- sprites: parsed once per frame ---- */
typedef struct { s16 top; u8 w, h; u16 attr; s16 sx; } Spr;
static Spr spr[80]; static u32 nspr;
static u8 sbuf[320+8] HOTDATA __attribute__((aligned(4)));
ROMFN __attribute__((noinline)) static void spr_prepare(void){
  u32 sat=((u32)(M.reg[5]&0x7Fu))<<9, link=0, n=0, maxs=(M.reg[12]&1u)?80u:64u;
  nspr=0;
  do{
    u32 e=sat+link*8u;
    u32 sy=VWA(e)&0x3FFu, sz=VWA(e+2u)>>8; link=VWA(e+2u)&0x7Fu;
    u32 attr=VWA(e+4u), sx=VWA(e+6u)&0x1FFu;
    Spr*s=&spr[nspr++];
    s->top=(s16)((s32)sy-128); s->w=(u8)(((sz>>2)&3u)+1u); s->h=(u8)((sz&3u)+1u); s->attr=(u16)attr; s->sx=(s16)((s32)sx-128);
    n++;
  }while(link&&n<maxs);
}
/* Draw all sprites that cover guest line y into sbuf[0..319]; returns 0 if none. */
static u32 spr_line(s32 y){
  u32 drawn=0,any=0;
  for(u32 i=0;i<nspr&&drawn<20u;i++){
    const Spr*s=&spr[i]; s32 top=s->top, h8=(s32)s->h*8;
    if(y<top||y>=top+h8) continue;
    if(!any){ u32*p=(u32*)sbuf; for(u32 k=0;k<(320u+8u)/4u;k++) p[k]=0; any=1; }
    drawn++;
    u32 attr=s->attr; s32 ly=y-top; if(attr&0x1000u) ly=h8-1-ly;
    u32 w=s->w,h=s->h,hf=(attr&0x800u)!=0, palb=((attr>>13)&3u)*16u+((attr&0x8000u)?0x80u:0u);
    for(u32 tc=0;tc<w;tc++){
      u32 col=hf?(w-1u-tc):tc;
      u32 tile=(attr&0x7FFu)+col*h+(u32)(ly>>3);
      u32 row=trow((tile<<5)+(u32)(ly&7)*4u);
      s32 X0=s->sx+(s32)tc*8;
      if(X0>=320||X0<=-8) continue;
      for(u32 j=0;j<8u;j++){
        s32 X=X0+(s32)j; if(X<0||X>=320) continue;
        u32 ni=hf?7u-j:j; u32 v=(row>>(28u-4u*ni))&15u;
        if(v&&!sbuf[X]) sbuf[X]=(u8)(palb+v);
      }
    }
  }
  return any;
}

/* ---- generic reference-shaped path ---- */
static u8 lbB[240] __attribute__((aligned(4))), lbA[240] __attribute__((aligned(4)));
ROMFN __attribute__((noinline)) static void plane_gen(u32 which,s32 y,u8*out,u32 width){
  u32 base= which==0? ((u32)(M.reg[2]&0x38)<<10) : ((u32)(M.reg[4]&7)<<13);
  u32 s=M.reg[16]&3u; s32 pw= s==0?32:s==1?64:128;
  s=(M.reg[16]>>4)&3u; s32 ph= s==0?32:s==1?64:128;
  u32 hs=((u32)(M.reg[13]&0x3F))<<10; u32 hmode=M.reg[11]&3u, vmode=(M.reg[11]>>2)&1u;
  s32 hline= hmode==0?0: hmode==2? (y&~7) : y;
  u32 ha=hs+(u32)hline*4u+(which==0?0u:2u); s32 hsc=(s32)(VWA(ha)&0x3FFu);
  (void)width;
  for(u32 dx=0;dx<240;dx++){
    s32 x=xm[dx]; s32 col=x>>4; s32 vs; if(vmode){ u32 vi=(u32)col*2u+(which==0?0u:1u); if(vi>=40u) vi-=40u; vs=(s32)md_vsram[vi]; } else vs=(s32)md_vsram[which==0?0:1];
    s32 py=(y+vs)&(ph*8-1); s32 px=(x-hsc)&(pw*8-1);
    u32 ea=base+(u32)(((py>>3)*pw+(px>>3))*2); u32 e=VWA(ea);
    s32 tx=px&7, ty=py&7; if(e&0x800u) tx=7-tx; if(e&0x1000u) ty=7-ty;
    u32 row=trow(((e&0x7FFu)<<5)+(u32)ty*4u); u32 ci=(row>>(28u-4u*(u32)tx))&15u;
    out[dx]= ci? (u8)(((e>>13)&3u)*16u+ci+((e&0x8000u)?0x80u:0u)) : 0;
  }
}
ROMFN __attribute__((noinline)) static void window_gen(s32 y,u8*outA){
  u32 wh=M.reg[17], wv=M.reg[18]; s32 right=(wh>>7)&1, hp=(wh&0x1F)*16; s32 down=(wv>>7)&1, vp=(wv&0x1F)*8;
  s32 inv= down? (y>=vp) : (y<vp);
  u32 base=((u32)(M.reg[3]&0x3E)<<10); s32 ww=(M.reg[12]&1)?64:32;
  for(u32 dx=0;dx<240;dx++){
    s32 x=xm[dx];
    s32 inh= right? (x>=hp) : (x<hp);
    if(!(inv||inh)) continue;
    u32 ea=base+(u32)(((y>>3)*ww+(x>>3))*2); u32 e=VWA(ea);
    s32 tx=x&7, ty=y&7; if(e&0x800u) tx=7-tx; if(e&0x1000u) ty=7-ty;
    u32 row=trow(((e&0x7FFu)<<5)+(u32)ty*4u); u32 ci=(row>>(28u-4u*(u32)tx))&15u;
    outA[dx]= ci? (u8)(((e>>13)&3u)*16u+ci+((e&0x8000u)?0x80:0)) : 0;
  }
}

/* ---- fast path: vertical scroll is full-screen, no window on this line ---- */
typedef struct { u32 row[42]; u8 attr[42]; u32 any; u32 f; } PL;
static PL plb HOTDATA; static PL pla;
/* CP28: cache name-table decoding across rows of the same tile band.
 * The guest VDP is immutable during a render call; tags reset on every call. */
typedef struct {u32 desc[42],tag,uniform;} PlaneCells;
static PlaneCells cell_cache[2];
static u32 cell_base[2],cell_vs[2],cell_xmask,cell_ymask,cell_shift,cell_hbase,cell_hmode;
static ROMFN void cells_begin(void){
 u32 mode=M.reg[16]&3u;cell_shift=mode==0u?5u:mode==1u?6u:7u;
 mode=(M.reg[16]>>4)&3u;u32 ph=mode==0u?32u:mode==1u?64u:128u;
 cell_xmask=(1u<<(cell_shift+3u))-1u;cell_ymask=ph*8u-1u;
 cell_base[0]=(M.reg[2]&0x38u)<<10;cell_base[1]=(M.reg[4]&7u)<<13;
 cell_vs[0]=md_vsram[0];cell_vs[1]=md_vsram[1];
 cell_hbase=(M.reg[13]&63u)<<10;cell_hmode=M.reg[11]&3u;
 cell_cache[0].tag=cell_cache[1].tag=0xFFFFFFFFu;
}
ROMFN __attribute__((noinline)) static void prepare_plane_row(u32 which,PL*p,u32 nb,u32 c0,u32 ncells){
 u32 cm=(1u<<cell_shift)-1u;PlaneCells*cache=&cell_cache[which];
 u32 e=VWA(nb+(c0&cm)*2u),first=((e&2047u)<<5)|((e&0x1000u)?28u:0u)|((e&0x800u)<<5),diff=0;
 cache->desc[0]=first;p->attr[0]=(u8)(((e>>9)&48u)|((e>>8)&128u));
 for(u32 k=1;k<ncells;k++){
  e=VWA(nb+((c0+k)&cm)*2u);
  u32 desc=((e&2047u)<<5)|((e&0x1000u)?28u:0u)|((e&0x800u)<<5);
  cache->desc[k]=desc;diff|=desc^first;
  p->attr[k]=(u8)(((e>>9)&48u)|((e>>8)&128u));
 }
 cache->uniform=diff==0u;
}
static void plane_cells(u32 which,s32 y,PL*p,u32 ncells){
 u32 hline=cell_hmode==0u?0u:cell_hmode==2u?((u32)y&~7u):(u32)y;
 u32 hsc=VWA(cell_hbase+hline*4u+which*2u)&1023u,b0=(0u-hsc)&cell_xmask;
 p->f=b0&7u;
 u32 py=((u32)y+cell_vs[which])&cell_ymask,nb=cell_base[which]+((py>>3)<<(cell_shift+1u));
 u32 c0=b0>>3,tag=(nb<<7)|c0;PlaneCells*cache=&cell_cache[which];
 if(cache->tag!=tag){prepare_plane_row(which,p,nb,c0,ncells);cache->tag=tag;}
 u32 offset=(py&7u)<<2,any=0,uniform=cache->uniform,count=uniform?1u:ncells;
 for(u32 k=0;k<count;k++){
  u32 desc=cache->desc[k],row=trow(desc^offset);
  if(desc&0x10000u)row=nib_rev(row);
  p->row[k]=row;any|=row;
 }
 if(uniform&&any)for(u32 k=1;k<ncells;k++)p->row[k]=any;
 p->any=any;
}

static u8 lb[240] HOTDATA __attribute__((aligned(4)));
static PL plw;
/* CP57: a complete opaque high-priority window occludes plane B.
 * Computed from live tile nibbles and all cell priority bits each line. */
static u32 window_cover;
/* window cells of guest line y (no scrolling: cell k covers guest x 8k..8k+7) */
static void window_cells(s32 y,PL*p,u32 ncells){
  u32 base=((u32)(M.reg[3]&0x3E)<<10); u32 ww=(M.reg[12]&1)?64u:32u;
  u32 nb=base+((u32)y>>3)*ww*2u, ty=(u32)y&7u, any=0,nz=0x88888888u,pri=0x8000u;
  for(u32 k=0;k<ncells;k++){
    u32 e=VWA(nb+k*2u);
    u32 t=(e&0x1000u)?7u-ty:ty;
    u32 row=trow(((e&0x7FFu)<<5)+t*4u);
    if(e&0x800u) row=nib_rev(row);
    p->row[k]=row; p->attr[k]=(u8)(((e>>9)&48u)|((e>>8)&128u)); any|=row;
    /* Adding seven to each low three-bit nibble cannot carry between
     * nibbles. Its high bit, OR the source high bit, tests nonzero exactly. */
    nz&=((row&0x77777777u)+0x77777777u)|row;pri&=e;
  }
  p->any=any; p->f=0;window_cover=(nz&0x88888888u)==0x88888888u&&pri;
}
#ifdef MD_HOST
static int slow_window;                       /* host reference: window lines through the per-pixel path */
#else
#define slow_window 0
#endif
/* per-column vertical scroll with every on-screen column of both planes at the same value is the same picture as
 * full-screen scroll (plane_cells reads entries 0/1) */
static u32 vs_columns_differ(u32 width){
  if(!((M.reg[11]>>2)&1u)) return 0;
  u32 nc=width>>4;
  for(u32 c=1;c<nc;c++) if(md_vsram[c*2u]!=md_vsram[0]||md_vsram[c*2u+1u]!=md_vsram[1]) return 1;
  return 0;
}
/* Fixed 320-to-240 sampling for an aligned single plane. Decode once
 * per cell and write three pixel pairs instead of indexing 240 pixels. */
static __attribute__((noinline)) void single320(const PL*p,u32 bd){
 u16 *out=(u16*)lb;
 for(u32 k=0;k<40u;k++){
  u32 r=p->row[k],pal=p->attr[k]&48u;
  if(!r){u16 b=(u16)(bd|(bd<<8));out[0]=b;out[1]=b;out[2]=b;}
  else{
   u32 a=(r>>28)&15u,b=(r>>24)&15u,c=(r>>20)&15u;
   u32 d=(r>>12)&15u,e=(r>>8)&15u,f=(r>>4)&15u;
   a=a?pal|a:bd;b=b?pal|b:bd;c=c?pal|c:bd;
   d=d?pal|d:bd;e=e?pal|e:bd;f=f?pal|f:bd;
   out[0]=(u16)(a|(b<<8));out[1]=(u16)(c|(d<<8));out[2]=(u16)(e|(f<<8));
  }
  out+=3;
 }
}
/* CP28: hot copy only while deciding whether the picture is blank.
 * Once a differing pixel exists, DMA3 copies subsequent lines without
 * repeating the same blank-image scan. Audio owns DMA1/2, not DMA3. */
#ifndef MD_HOST
__attribute__((target("thumb")))
#endif
static HOT __attribute__((noinline)) u32 copy_line_checked(volatile u32*dst,u32 ref){
 const u32*src=(const u32*)lb;u32 diff=0;
 for(u32 i=0;i<60u;i++){u32 word=src[i];dst[i]=word;diff|=word^ref;}
 return diff;
}
static ROMFN __attribute__((noinline)) void copy_line_dma(volatile u32*dst){
#ifdef MD_HOST
 const u32*src=(const u32*)lb;for(u32 i=0;i<60u;i++)dst[i]=src[i];
#else
 volatile u32*regs=(volatile u32*)0x040000D4u;
 regs[2]=0;regs[0]=(u32)lb;regs[1]=(u32)dst;regs[2]=0x8400003Cu;
 /* Immediate DMA stalls the CPU until all sixty aligned words complete. */
#endif
}

static u32 vmode_line HOTDATA;
static void render_line(s32 y,u32 width){
  u32 ncells=(width==320u)?41u:33u;
  u32 bd=M.reg[7]&63u;
  u32 vmode=vmode_line;
  /* window active on this line? */
  u32 wh=M.reg[17], wv=M.reg[18]; s32 hp=(wh&0x1F)*16, vp=(wv&0x1F)*8, right=(wh>>7)&1, down=(wv>>7)&1;
  s32 inv= down? (y>=vp) : (y<vp);
  u32 win= inv || (right? (hp<(s32)width) : (hp>0));
  u32 shas=nspr?spr_line(y):0;
  if(vmode||(win&&slow_window)){
    plane_gen(1,y,lbB,width); plane_gen(0,y,lbA,width); if(win) window_gen(y,lbA);
    for(u32 dx=0;dx<240;dx++){
      u32 b=lbB[dx],a=lbA[dx],s=shas?sbuf[xm[dx]]:0,c=bd;
      if(s&128)c=s;else if(a&128)c=a;else if(b&128)c=b;else if(s)c=s;else if(a)c=a;else if(b)c=b;
      lb[dx]=(u8)(c&63u);
    }
    return;
  }
  /* guest x range [wx0,wx1) shows the window instead of plane A */
  u32 wx0=0,wx1=0;
  if(win){ if(inv){ wx0=0; wx1=0x10000u; } else if(right){ wx0=(u32)hp; wx1=0x10000u; } else { wx0=0; wx1=(u32)hp; }
    window_cells(y,&plw,ncells-1u); }
  if(win&&wx0==0u&&wx1>=width&&window_cover){plb.any=0;plb.f=0;}
  else plane_cells(1,y,&plb,ncells);
  if(!(wx0==0&&wx1>=width)) plane_cells(0,y,&pla,ncells); else pla.any=0;
  if(!win) plw.any=0;
  u32 bf=plb.f, af=pla.f, wn=wx1-wx0;
  if(!plb.any&&!pla.any&&!plw.any&&!shas){ for(u32 dx=0;dx<240;dx+=4) *(u32*)&lb[dx]=bd*0x01010101u; return; }
  /* A single visible plane needs no priority/sprite compositing. This
   * includes the full-width window used for adventure dialogue. */
  const PL *single=0;
  if(!shas){
    if(!plb.any&&(!win||(wx0==0u&&wx1>=width)))single=win?&plw:&pla;
    else if(!win&&!pla.any)single=&plb;
  }
  if(single){
    u32 f=single->f;
    if(width==320u&&!f){single320(single,bd);return;}
    for(u32 dx=0;dx<240u;dx++){
      u32 x=xm[dx]+f,k=x>>3,v=(single->row[k]>>((~x&7u)<<2))&15u;
      lb[dx]=(u8)(v?(single->attr[k]+v)&63u:bd);
    }
    return;
  }
  for(u32 dx=0;dx<240;dx++){
    u32 x=xm[dx],a=0,s=shas?sbuf[x]:0,c;
    if(x-wx0<wn){ if(plw.any){ u32 k=x>>3,v=(plw.row[k]>>((~x&7u)<<2))&15u; if(v) a=plw.attr[k]+v; } }
    else if(pla.any){ u32 u=x+af,k=u>>3,v=(pla.row[k]>>((~u&7u)<<2))&15u; if(v) a=pla.attr[k]+v; }
    /* CP58: a high-priority sprite/A pixel already decides the result.
     * Decode plane B only where its priority can still affect the picture. */
    if(s&128u)c=s;
    else if(a&128u)c=a;
    else {
      u32 b=0;
      if(plb.any){ u32 u=x+bf,k=u>>3,v=(plb.row[k]>>((~u&7u)<<2))&15u; if(v) b=plb.attr[k]+v; }
      if(b&128u)c=b;else if(s)c=s;else if(a)c=a;else if(b)c=b;else c=bd;
    }
    lb[dx]=(u8)(c&63u);
  }
}

static inline u32 gba_color(u32 c){ return (((c>>1)&7u)*4u)|((((c>>5)&7u)*4u)<<5)|((((c>>9)&7u)*4u)<<10); }

ROMFN void mdr_init(void){ tile_index_valid=0; xm_w=0; mdr_vgen=mdr_cgen=0xFFFFFFFFu; }

ROMFN int mdr_image_blank(void){ return blank_flag; }

/* ---- incremental rendering ----
 * md.c marks every 32-byte VRAM block that changes (md_vdirty) and sets M.rfull for register/VSRAM changes.
 * Here those changes are turned into a set of dirty guest lines:
 *   plane A/B name-table entries -> the 8-line band where that cell row is on screen (full-screen vscroll);
 *   window name-table entries    -> their 8-line band;
 *   tile patterns                -> every on-screen cell (planes, window, sprites) that uses the tile;
 *   sprite table changes         -> the lines covered by the old and the new sprite lists.
 * Anything else (per-column vscroll, hscroll table, VSRAM, registers) redraws everything.  Each of the two pages
 * keeps its own pending set, so the back page is brought up to date before it is shown.  The pixels written for a
 * dirty line are exactly what the full render writes (mdhost `incr` checks this on real game runs). */
static u32 pend[2][8], pfull[2]={1,1};
static u32 dl[8];                                     /* guest lines dirtied by this update (240 bits) */
static u32 tdirty[64];                                /* 2048 tile bits */
static Spr ospr[80]; static u32 onspr;
static ROMFN inline void mark_band(s32 y0,s32 n,u32 height){
 s32 y1=y0+n;if(y0<0)y0=0;if(y1>(s32)height)y1=(s32)height;if(y0>=y1)return;
 u32 first=(u32)y0>>5,last=(u32)(y1-1)>>5;
 u32 lo=0xFFFFFFFFu<<((u32)y0&31u),hi=0xFFFFFFFFu>>(31u-((u32)(y1-1)&31u));
 if(first==last){dl[first]|=lo&hi;return;}
 dl[first]|=lo;for(u32 w=first+1;w<last;w++)dl[w]=0xFFFFFFFFu;dl[last]|=hi;
}
ROMFN __attribute__((noinline)) static void mark_sprites(const Spr*l,u32 n,u32 height){ for(u32 i=0;i<n;i++) mark_band(l[i].top,(s32)l[i].h*8,height); }

/* Aligned full-screen vscroll makes an eight-line tile row exactly one
 * guest band. Other vscroll phases retain the original exact line scanner.
 * A name-table update can only add references to this conservative index.
 * Periodic rebuilds remove stale references; full redraws invalidate it. */
ROMFN __attribute__((noinline)) static void index_name_words(u32 base,u32 first,u32 last,u32 shift,u32 vs,u32 ymask,u32 height){
 for(u32 e=first;e<=last;e++){
  u32 y=(((e>>shift)*8u)-vs)&ymask;
  if(y<height){u32 t=VWA(base+e*2u)&2047u;tile_bands[t]|=1u<<(y>>3);}
 }
}
ROMFN __attribute__((noinline)) static void build_tile_index(u32 height){
 for(u32 t=0;t<2048;t++)tile_bands[t]=0;
 u32 s=M.reg[16]&3u,pw=s==0?32u:s==1?64u:128u;
 s=(M.reg[16]>>4)&3u;u32 ph=s==0?32u:s==1?64u:128u,ymask=ph*8u-1u;
 u32 hs=((u32)(M.reg[13]&63u))<<10,width=(M.reg[12]&1u)?320u:256u;
 for(u32 pl=0;pl<2;pl++){
  u32 base=pl?((u32)(M.reg[4]&7u)<<13):((u32)(M.reg[2]&0x38u)<<10);
  u32 vs=md_vsram[pl]&1023u,c0=0,cn=pw;
  if(!(M.reg[11]&3u)){c0=((0u-(VWA(hs+pl*2u)&1023u))&(pw*8u-1u))>>3;cn=(width>>3)+1u;if(cn>pw)cn=pw;}
  for(u32 r=0;r<ph;r++){
   u32 y=(r*8u-vs)&ymask;if(y>=height)continue;u32 mask=1u<<(y>>3);
   for(u32 c=0;c<cn;c++){u32 e=VWA(base+(r*pw+((c0+c)&(pw-1u)))*2u);tile_bands[e&2047u]|=mask;}
  }
 }
 u32 base=((u32)(M.reg[3]&0x3Eu))<<10,ww=(width==320u)?64u:32u;
 for(u32 r=0;r*8u<height;r++)for(u32 c=0;c<(width>>3);c++)tile_bands[VWA(base+(r*ww+c)*2u)&2047u]|=1u<<r;
 tile_index_valid=1;tile_index_age=0;
}
/* returns 1 if a full redraw is needed */
#ifdef MD_HOST
#define COLD_THUMB
#else
#define COLD_THUMB __attribute__((target("thumb")))
#endif
static ROMFN COLD_THUMB __attribute__((noinline)) u32 collect_dirty(u32 height){
  for(u32 k=0;k<8;k++) dl[k]=0;
  if(vmode_line) return 1;
  /* CP53: no guest VRAM changes means no newly dirty guest lines.
   * Previous per-page dirty bits are still consumed by mdr_render().
   * Changed VDP registers and VSRAM force the full path before this call. */
  u32 dirty_or=0;
  for(u32 w=0;w<64;w++) dirty_or |= md_vdirty[w];
  if(!dirty_or) return 0;                           /* per-column vertical scroll: no simple line mapping */
  u32 bA=((u32)(M.reg[2]&0x38)<<10), bB=((u32)(M.reg[4]&7)<<13), bW=((u32)(M.reg[3]&0x3E)<<10);
  u32 s=M.reg[16]&3u; u32 pw= s==0?32u:s==1?64u:128u; s=(M.reg[16]>>4)&3u; u32 ph= s==0?32u:s==1?64u:128u;
  u32 pwshift=(M.reg[16]&3u)==0?5u:(M.reg[16]&3u)==1?6u:7u;
  u32 wshift=(M.reg[12]&1u)?6u:5u;
  u32 psz=pw*ph*2u, ww=(M.reg[12]&1u)?64u:32u, wsz=ww*32u*2u;
  u32 sat=((u32)(M.reg[5]&0x7Fu))<<9, hs=((u32)(M.reg[13]&0x3F))<<10;
  u32 vsA=md_vsram[0]&0x3FFu, vsB=md_vsram[1]&0x3FFu, pmask=ph*8u-1u;
  u32 anyt=0, sprchg=0;
  { u32 nb=0; for(u32 w=0;w<64;w++){ u32 b=md_vdirty[w]; while(b){ b&=b-1u; nb++; } }
    if(nb>96u) return 1; }                           /* big VRAM change (scene load): a full redraw is cheaper */
  for(u32 k=0;k<64;k++) tdirty[k]=0;
  for(u32 w=0;w<64;w++){
    u32 bits=md_vdirty[w]; if(!bits) continue; md_vdirty[w]=0;
    while(bits){
      u32 b=(u32)__builtin_ctz(bits); bits&=bits-1u;
      u32 blk=w*32u+b, a=blk*32u;
      tdirty[blk>>5]|=1u<<(blk&31u); anyt=1;          /* a block is exactly one tile pattern */
      if(a-hs<height*4u&&(M.reg[11]&3u)!=0) return 1;    /* hscroll table (line/cell modes) */
      if(a-hs<4u) return 1;                          /* full-screen hscroll value */
      if(a-sat<640u) sprchg=1;
      for(u32 pl=0;pl<2;pl++){
        u32 base=pl?bB:bA, vs=pl?vsB:vsA;
        u32 offset=(a-base)&65535u;
        if(offset<psz){
          u32 e0=offset>>1, e1=e0+15u; if(e1>=pw*ph) e1=pw*ph-1u;
          if(tile_index_valid)index_name_words(base,e0,e1,pwshift,vs,pmask,height);
          for(u32 r=e0>>pwshift;r<=e1>>pwshift;r++){ s32 y=(s32)(((r*8u)-vs)&pmask); if(y>=(s32)(pmask+1u)-7) y-=(s32)(pmask+1u); mark_band(y,8,height); }
        }
      }
      u32 woffset=(a-bW)&65535u;
      if(woffset<wsz){ u32 e0=woffset>>1; u32 r0=e0>>wshift, r1=(e0+15u)>>wshift; mark_band((s32)(r0*8u),(s32)((r1-r0+1u)*8u),height); if(tile_index_valid)index_name_words(bW,e0,e0+15u,wshift,0u,255u,height); }
    }
  }
  /* CP53: the parsed sprites depend on the SAT bytes and VDP mode.
   * Mode/register changes are full-redraw cases; a VRAM block write touching
   * the non-wrapping SAT sets sprchg above. If none touched, reuse the
   * already-parsed sprite list. Wrapped SAT tables keep original behavior. */
  if(sprchg || sat > 0x10000u-640u) {
    spr_prepare();
    if(nspr!=onspr) sprchg=1; else { const u8*p=(const u8*)spr,*q=(const u8*)ospr; for(u32 i=0;i<nspr*sizeof(Spr);i++) if(p[i]!=q[i]){ sprchg=1; break; } }
    if(sprchg){ mark_sprites(ospr,onspr,height); mark_sprites(spr,nspr,height); for(u32 i=0;i<nspr;i++) ospr[i]=spr[i]; onspr=nspr; }
  }
  if(anyt){
    if(!((vsA|vsB)&7u)){
      if(!tile_index_valid||tile_index_age>=64u)build_tile_index(height);
      tile_index_age++;
      u32 bands=0;
      for(u32 w=0;w<64;w++){
        u32 bits=tdirty[w];while(bits){u32 t=w*32u+(u32)__builtin_ctz(bits);bits&=bits-1u;bands|=tile_bands[t];}
      }
      while(bands){u32 band=(u32)__builtin_ctz(bands);bands&=bands-1u;dl[band>>2]|=255u<<((band&3u)*8u);}
    }else{
      tile_index_valid=0;
    /* cells of the planes and the window that show a changed tile */
    for(u32 pl=0;pl<2;pl++){
      u32 base=pl?bB:bA, vs=pl?vsB:vsA;
      u32 c0=0,cn=pw;
      /* Only full-screen horizontal scroll has the same visible cells on
       * every line. Other modes retain the complete row scan. */
      if(!(M.reg[11]&3u)){
        u32 hsc=VWA(hs+pl*2u)&0x3FFu;
        c0=((0u-hsc)&(pw*8u-1u))>>3;
        cn=((M.reg[12]&1u)?40u:32u)+1u;if(cn>pw)cn=pw;
      }
      for(u32 r=0;r<ph;r++){
        s32 y=(s32)(((r*8u)-vs)&pmask); if(y>=(s32)(pmask+1u)-7) y-=(s32)(pmask+1u);
        if(y>=(s32)height||y<=-8) continue;
        for(u32 c=0;c<cn;c++){ u32 cc=(c0+c)&(pw-1u);u32 t=md_vram[((base+(r*pw+cc)*2u)&0xFFFFu)>>1]&0x7FFu; if(tdirty[t>>5]&(1u<<(t&31u))){ mark_band(y,8,height); break; } }
      }
    }
    for(u32 r=0;r<32u&&r*8u<height;r++)
      for(u32 c=0;c<((M.reg[12]&1u)?40u:32u);c++){ u32 t=md_vram[((bW+(r*ww+c)*2u)&0xFFFFu)>>1]&0x7FFu; if(tdirty[t>>5]&(1u<<(t&31u))){ mark_band((s32)(r*8u),8,height); break; } }
    }
    for(u32 i=0;i<nspr;i++){
      u32 n=(u32)spr[i].w*spr[i].h, t0=spr[i].attr&0x7FFu;
      for(u32 k=0;k<n;k++){ u32 t=(t0+k)&0x7FFu; if(tdirty[t>>5]&(1u<<(t&31u))){ mark_band(spr[i].top,(s32)spr[i].h*8,height); break; } }
    }
  }
  return 0;
}

static u32 last_w, last_h, last_on;
ROMFN void mdr_render(unsigned page){
  volatile u16*pal=MD_PAL;
  if(M.cgen!=mdr_cgen){ for(u32 i=0;i<64;i++) pal[i]=(u16)gba_color(md_cram[i]); }
  u32 width=(M.reg[12]&1u)?320u:256u, height=(M.reg[1]&8u)?240u:224u;
  if(xm_w!=width) build_xm(width);
  u32 on=(M.reg[1]&64u)!=0;
  vmode_line=vs_columns_differ(width);
  u32 full=M.rfull||mdr_vgen==0xFFFFFFFFu||width!=last_w||height!=last_h||on!=last_on; M.rfull=0;
  last_w=width; last_h=height; last_on=on;
  volatile u32*o=(volatile u32*)MD_PAGE(page);
  if(!on){
    for(u32 w=0;w<64;w++) md_vdirty[w]=0;
#ifdef MD_HOST
    for(u32 i=0;i<(240u*160u)/4u;i++) o[i]=0;
#else
    /* CP52: replace 9,600 synchronous CPU->VRAM word stores with one
       native GBA DMA3 fixed-source fill.  The stack word lives in IWRAM,
       the DMA completes before the CPU resumes, and DMA1/2 (audio) are
       untouched.  Always refresh both page choices when display is off. */
    volatile u32 zero=0;
    volatile u32 *regs=(volatile u32*)0x040000D4u;
    regs[2]=0u;
    regs[0]=(u32)&zero;
    regs[1]=(u32)o;
    regs[2]=0x84800000u|9600u;
#endif
    blank_flag=1; pfull[0]=pfull[1]=1; onspr=0xFFFFFFFFu;
    mdr_vgen=M.vgen; mdr_cgen=M.cgen; return;
  }
  if(!full) full=collect_dirty(height);
  if(full){ tile_index_valid=0; for(u32 w=0;w<64;w++) md_vdirty[w]=0; pfull[0]=pfull[1]=1; spr_prepare(); for(u32 i=0;i<nspr;i++) ospr[i]=spr[i]; onspr=nspr; }
  else for(u32 k=0;k<8;k++){ pend[0][k]|=dl[k]; pend[1][k]|=dl[k]; }
  cells_begin();
  if(pfull[page]){
    u32 ref=0,diff=0;
    for(u32 dy=0;dy<160u;dy++){
      s32 y=(s32)((dy*height)/160u);
      render_line(y,width);
      if(dy==0) ref=lb[0]*0x01010101u;
      if(diff)copy_line_dma(o);else diff=copy_line_checked(o,ref);o+=60u;
    }
    blank_flag=(diff==0); pfull[page]=0; for(u32 k=0;k<8;k++) pend[page][k]=0;
  } else {
    u32*pp=pend[page]; u32 any=0; for(u32 k=0;k<8;k++) any|=pp[k];
    if(any){
      u32 diff=0, ref=*(volatile u32*)o;
      for(u32 dy=0;dy<160u;dy++){
        u32 y=(dy*height)/160u;
        if(!(pp[y>>5]&(1u<<(y&31u)))) continue;
        render_line((s32)y,width);
        volatile u32*q=o+dy*60u;
        if(diff)copy_line_dma(q);else diff=copy_line_checked(q,ref);
      }
      if(diff) blank_flag=0;
      for(u32 k=0;k<8;k++) pp[k]=0;
    }
  }
  mdr_vgen=M.vgen; mdr_cgen=M.cgen;
}

#ifdef MD_HOST
/* host check: full render of the current state into out (240x160), leaving the incremental state alone */
void mdr_full_reference(u8*out){
  u32 width=(M.reg[12]&1u)?320u:256u, height=(M.reg[1]&8u)?240u:224u;
  if(xm_w!=width) build_xm(width);
  if(!(M.reg[1]&64u)){ for(u32 i=0;i<240u*160u;i++) out[i]=0; return; }
  Spr save[80]; u32 ns=nspr; for(u32 i=0;i<ns;i++) save[i]=spr[i];
  cells_begin();
  spr_prepare(); slow_window=1; u32 vml=vmode_line; vmode_line=(M.reg[11]>>2)&1u;
  for(u32 dy=0;dy<160u;dy++){ render_line((s32)((dy*height)/160u),width); for(u32 dx=0;dx<240u;dx++) out[dy*240u+dx]=lb[dx]; }
  slow_window=0; vmode_line=vml;
  for(u32 i=0;i<ns;i++) spr[i]=save[i]; nspr=ns;
}
#endif
