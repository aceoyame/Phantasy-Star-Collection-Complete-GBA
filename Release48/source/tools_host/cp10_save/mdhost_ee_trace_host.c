/* Host harness for the Genesis runtime (CP08).  Builds the same md.c / mdrender.c /
 * m68k.c / m68k_ref.c / mddebug.c that run on the GBA (with -DMD_HOST) and lets us
 *
 *   run     <rom> <frames> [--ref] [--dump N prefix] [--sig]   run a cartridge, optionally dump presented frames as PPM
 *                                                              or per-scanline CPU signatures (compare fast vs --ref builds)
 *   render  <rom> <frames> [step]                             new renderer vs the original CP07 renderer on real game states
 *   fuzz    <trials> [seed]                                   same, on random VDP states covering every plane/window/sprite mode
 *   ui      <rom> <frames> <prefix>                           write every debug-menu page as a PPM
 *   spin    <rom> <frames>                                    idle-spin fold equivalence: fold on vs off, per-scanline signatures
 *
 * Build (see tools_host/md_host/build_host.sh).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "md.h"

uint16_t md_h_pal[256]; uint8_t md_h_fb[2*0xA000] __attribute__((aligned(4))); uint16_t md_h_dispcnt; uint32_t md_h_keys; uint32_t md_h_ticks; uint32_t md_h_hb[32]; int md_h_reset_requested;
unsigned __aeabi_uidiv(unsigned n,unsigned d){ return d?n/d:0; }

MDCfg md_cfg; u32 md_page; u32 md_frame_ticks, md_render_ticks, md_last_frame_ticks, md_frames_off;
void md_present_game(void){ unsigned back=md_page^1u; mdr_render(back); md_page=back; }
void md_eeprom_store(u32 a,u32 v){ (void)a; (void)v; }
void md_eeprom_load(void){ for(int i=0;i<128;i++) md_ee.mem[i]=0xFF; }
void mdr_full_reference(u8*out);
void md_wait_pace(void){}
void md_heartbeat(void){}
void md_soft_reset(void){ printf("soft reset requested\n"); exit(0); }
u32 aot_miss[1<<24];


#ifdef USE_REF_EXEC
/* Reference-core scheduler step: same contract as the fast core's m68k_exec. */
u32 pc_hist[1<<24]; int pc_hist_on; u32 op_hist[65536];
static u32 fh[1<<24], touched[1<<20], ntouched;
void m68k_exec(u32 limit){
  u32 old_sr;
  do{ old_sr=C.sr; if(pc_hist_on){ u32 p=C.pc&0xFFFFFFu; pc_hist[p]++; op_hist[bus_read16(C.pc)]++; if(!fh[p]++) { if(ntouched<(1u<<20)) touched[ntouched++]=p; } } ref_step(); if(C.stopped||((old_sr^C.sr)&0x2700u)) break; }while((s32)(limit-C.cycles)>0);
}
#endif

static u8*load(const char*path,u32*size){
  FILE*f=fopen(path,"rb"); if(!f){ perror(path); exit(2); }
  fseek(f,0,SEEK_END); long n=ftell(f); fseek(f,0,SEEK_SET);
  u8*b=malloc((size_t)n+4096); memset(b,0,(size_t)n+4096); if(fread(b,1,(size_t)n,f)!=(size_t)n) exit(2); fclose(f); *size=(u32)n; return b;
}
#ifdef AOT_ON
void aot_select_index(u32);
#endif
static void boot(const char*path){
  u32 size; u8*rom=load(path,&size);
  for(u32 i=0;i<128;i++) m68k_romp[i]=(i<(size>>12))?rom+i*4096u:0;
  m68k_romsize=size; m68k_ram=md_ram; m68k_init_tables();
  md_reset_game();
  md_cfg.size=size; md_cfg.index=2;
  { static const char*nm[]={"Anne","Rudger","Kinds","Shilka","Amia","Huey","Eusis","Nei"}; for(int i=0;i<8;i++) if(strstr(path,nm[i])) md_cfg.index=2+i; }
#ifdef AOT_ON
  aot_select_index(md_cfg.index);
#endif const char*b=strrchr(path,'/'); strncpy(md_cfg.title,b?b+1:path,48);
}
static void dump_ppm(const char*name,unsigned page){
  FILE*f=fopen(name,"wb"); fprintf(f,"P6\n240 160\n255\n");
  for(int i=0;i<240*160;i++){
    u32 c=md_h_fb[page*0xA000+i]; u16 p=md_h_pal[c]; u32 r=(p&31)*255/31,g=((p>>5)&31)*255/31,b=((p>>10)&31)*255/31;
    fputc(r,f); fputc(g,f); fputc(b,f);
  }
  fclose(f);
}
static u32 hash_bytes(const void*p,size_t n){ const u8*q=p; u32 h=2166136261u; while(n--){ h^=*q++; h*=16777619u; } return h; }
static u32 cpu_sig(void){
  u32 h=hash_bytes(C.d,sizeof C.d); h=h*31u+hash_bytes(C.a,sizeof C.a); h=h*31u+C.pc; h=h*31u+(getenv("SIGFLAGS")?C.sr:(C.sr&0xA700u)); h=h*31u+C.cycles; h=h*31u+(u32)M.line;
  h=h*31u+M.vint_pending*3u+M.hint_pending; h=h*31u+C.stopped; return h;
}

/* ---------------- original CP07 renderer (verbatim semantics, operating on byte arrays) ---------------- */
typedef struct { u8 vram[65536]; u16 cram[64], vsram[40]; u8 reg[24]; } OM;
static OM om;
static u8 olinebuf[3][320];
static int oplane_w(void){ int s=om.reg[16]&3; return s==0?32:s==1?64:128; }
static int oplane_h(void){ int s=(om.reg[16]>>4)&3; return s==0?32:s==1?64:128; }
static void orender_plane(int which,int y,u8* out){
  u32 base= which==0? ((u32)(om.reg[2]&0x38)<<10) : ((u32)(om.reg[4]&7)<<13);
  int pw=oplane_w(), ph=oplane_h();
  u32 hs=((u32)(om.reg[13]&0x3F))<<10; int hmode=om.reg[11]&3; int vmode=(om.reg[11]>>2)&1;
  int hline= hmode==0?0: hmode==2? (y&~7) : y;
  u32 ha=hs+(u32)hline*4+(which==0?0:2); int hsc=(int)(((om.vram[ha&0xFFFF]<<8)|om.vram[(ha+1)&0xFFFF])&0x3FF);
  for(int x=0;x<320;x++){
    int col=x>>4; int vs= vmode? om.vsram[(col*2+(which==0?0:1))%40] : om.vsram[which==0?0:1];
    int py=(y+vs)&(ph*8-1); int px=(x-hsc)&(pw*8-1);
    u32 ea=base+(u32)(((py>>3)*pw+(px>>3))*2); u16 e=(u16)((om.vram[ea&0xFFFF]<<8)|om.vram[(ea+1)&0xFFFF]);
    int tx=px&7, ty=py&7; if(e&0x800) tx=7-tx; if(e&0x1000) ty=7-ty;
    u32 ta=((u32)(e&0x7FF)<<5)+(u32)ty*4+(u32)(tx>>1); u8 b=om.vram[ta&0xFFFF]; u8 ci=(tx&1)?(b&15):(b>>4);
    out[x]= ci? (u8)(((e>>13)&3)*16+ci+((e&0x8000)?0x80:0)) : 0;
  }
}
static void orender_window_line(int y,u8* outA){
  int wh=om.reg[17], wv=om.reg[18]; int right=(wh>>7)&1, hp=(wh&0x1F)*16; int down=(wv>>7)&1, vp=(wv&0x1F)*8;
  int inv= down? (y>=vp) : (y<vp);
  u32 base=((u32)(om.reg[3]&0x3E)<<10); int ww=(om.reg[12]&1)?64:32;
  for(int x=0;x<320;x++){
    int inh= right? (x>=hp) : (x<hp);
    if(!(inv||inh)) continue;
    u32 ea=base+(u32)(((y>>3)*ww+(x>>3))*2); u16 e=(u16)((om.vram[ea&0xFFFF]<<8)|om.vram[(ea+1)&0xFFFF]);
    int tx=x&7, ty=y&7; if(e&0x800) tx=7-tx; if(e&0x1000) ty=7-ty;
    u32 ta=((u32)(e&0x7FF)<<5)+(u32)ty*4+(u32)(tx>>1); u8 b=om.vram[ta&0xFFFF]; u8 ci=(tx&1)?(b&15):(b>>4);
    outA[x]= ci? (u8)(((e>>13)&3)*16+ci+((e&0x8000)?0x80:0)) : 0;
  }
}
static void orender_sprites(int y,u8* out){
  memset(out,0,320);
  u32 sat=((u32)(om.reg[5]&0x7F))<<9; int link=0, n=0; int maxs=(om.reg[12]&1)?80:64;
  int drawn=0;
  do{
    u32 e=sat+(u32)link*8;
    int sy=((om.vram[e&0xFFFF]<<8)|om.vram[(e+1)&0xFFFF])&0x3FF; int sz=om.vram[(e+2)&0xFFFF]; link=om.vram[(e+3)&0xFFFF]&0x7F;
    u16 attr=(u16)((om.vram[(e+4)&0xFFFF]<<8)|om.vram[(e+5)&0xFFFF]); int sx=((om.vram[(e+6)&0xFFFF]<<8)|om.vram[(e+7)&0xFFFF])&0x1FF;
    int w=((sz>>2)&3)+1, h=(sz&3)+1; int top=sy-128;
    if(y>=top && y<top+h*8 && drawn<20){
      drawn++;
      int ly=y-top; if(attr&0x1000) ly=h*8-1-ly;
      for(int cx=0;cx<w*8;cx++){
        int X=sx-128+cx; if(X<0||X>=320) continue;
        int lx= (attr&0x800)? w*8-1-cx : cx;
        int tile=(attr&0x7FF)+(lx>>3)*h+(ly>>3);
        u32 ta=((u32)tile<<5)+(u32)(ly&7)*4+(u32)((lx&7)>>1); u8 b=om.vram[ta&0xFFFF]; u8 ci=(lx&1)?(b&15):(b>>4);
        if(ci && !out[X]) out[X]=(u8)(((attr>>13)&3)*16+ci+((attr&0x8000)?0x80:0));
      }
    }
    n++;
  }while(link && n<maxs);
}
static u8 oref[160][240];
static void old_render(void){
  for(u32 a=0;a<65536;a++) om.vram[a]=((u8*)md_vram)[a^1];
  memcpy(om.cram,md_cram,sizeof om.cram); memcpy(om.vsram,md_vsram,40*2); memcpy(om.reg,M.reg,24);
  unsigned width=(om.reg[12]&1)?320:256,height=(om.reg[1]&8)?240:224;
  for(unsigned dy=0;dy<160;dy++){
    int y=(dy*height)/160; orender_plane(1,y,olinebuf[0]); orender_plane(0,y,olinebuf[1]); orender_window_line(y,olinebuf[1]); orender_sprites(y,olinebuf[2]);
    for(unsigned dx=0;dx<240;dx++){
      unsigned x=(dx*width)/240; u8 b=olinebuf[0][x],a=olinebuf[1][x],s=olinebuf[2][x],c=om.reg[7]&63;
      if(s&128)c=s;else if(a&128)c=a;else if(b&128)c=b;else if(s)c=s;else if(a)c=a;else if(b)c=b;
      if(!(om.reg[1]&64))c=0;
      oref[dy][dx]=c&63;
    }
  }
}
static int compare_render(const char*what){
  old_render();
  mdr_vgen=0xFFFFFFFFu; mdr_cgen=0xFFFFFFFFu; mdr_render(0);
  int bad=0;
  for(int y=0;y<160;y++) for(int x=0;x<240;x++){
    u8 n=md_h_fb[y*240+x];
    if(n!=oref[y][x]){ if(bad<5) printf("  MISMATCH %s at (%d,%d): new %u old %u\n",what,x,y,n,oref[y][x]); bad++; }
  }
  /* palette */
  for(int i=0;i<64;i++){ u32 c=md_cram[i]; u16 e=(u16)((((c>>1)&7)*4)|((((c>>5)&7)*4)<<5)|((((c>>9)&7)*4)<<10)); if(md_h_pal[i]!=e){ printf("  PALETTE MISMATCH %d\n",i); bad++; } }
  return bad;
}

/* ---------------- random VDP states ---------------- */
static u64 rs=88172645463325252ull;
static u32 rnd(void){ rs^=rs<<13; rs^=rs>>7; rs^=rs<<17; return (u32)(rs>>11); }
static void random_state(void){
  u8*vb=(u8*)md_vram;
  /* sparse tiles: mostly random, some zero */
  for(u32 i=0;i<65536;i++) vb[i]=(rnd()%5==0)?0:(u8)rnd();
  if(rnd()%3==0){ for(u32 i=0;i<65536;i++) if(rnd()%3) vb[i]=0; }
  for(u32 i=0;i<64;i++) md_cram[i]=(u16)(rnd()&0xEEE);
  for(u32 i=0;i<40;i++) md_vsram[i]=(u16)(rnd()&0x7FF);
  if(rnd()%2) for(u32 i=0;i<40;i++) md_vsram[i]&=0xF;
  if(rnd()%4==0) for(u32 i=2;i<40;i++) md_vsram[i]=md_vsram[i&1u];   /* column scroll with equal columns */
  memset(M.reg,0,sizeof M.reg);
  M.reg[1]=(u8)((rnd()%8?0x40:0)|(rnd()%4==0?8:0)|0x14);
  M.reg[2]=(u8)((rnd()%8)<<3); M.reg[3]=(u8)((rnd()%32)<<1); M.reg[4]=(u8)(rnd()%8); M.reg[5]=(u8)(rnd()%128);
  M.reg[7]=(u8)(rnd()&63); M.reg[11]=(u8)((rnd()%4)|((rnd()%2)<<2)); M.reg[12]=(u8)(rnd()%2?0x81:0x00);
  M.reg[13]=(u8)(rnd()%64); M.reg[16]=(u8)((rnd()%4)|((rnd()%4)<<4));
  M.reg[17]=(u8)(rnd()%3==0?rnd():(rnd()%2?0:0x80)); M.reg[18]=(u8)(rnd()%3==0?rnd():(rnd()%2?0:0x80));
  /* sprite table with a bounded random chain, mostly small */
  u32 sat=((u32)(M.reg[5]&0x7F))<<9; u32 n=rnd()%30;
  for(u32 i=0;i<=n;i++){
    u32 e=sat+i*8;
    u32 sy=128+(rnd()%240)-16, sx=128+(rnd()%360)-24; u32 sz=rnd()&15; u32 link=(i==n)?0:i+1; u32 attr=rnd()&0xFFFF; if(rnd()%2) attr=(attr&0xF800)|(rnd()&0x7FF);
    vb[(e+0)&0xFFFF^1]=(u8)(sy>>8); vb[(e+1)&0xFFFF^1]=(u8)sy; vb[(e+2)&0xFFFF^1]=(u8)sz; vb[(e+3)&0xFFFF^1]=(u8)link;
    vb[(e+4)&0xFFFF^1]=(u8)(attr>>8); vb[(e+5)&0xFFFF^1]=(u8)attr; vb[(e+6)&0xFFFF^1]=(u8)(sx>>8); vb[(e+7)&0xFFFF^1]=(u8)sx;
  }
  /* sometimes make plane tables dense with low tile numbers so images are busy */
  if(rnd()%2){
    u32 bases[3]={((u32)(M.reg[2]&0x38))<<10,((u32)(M.reg[4]&7))<<13,((u32)(M.reg[3]&0x3E))<<10};
    for(int k=0;k<3;k++) for(u32 i=0;i<0x2000;i+=2){ u32 e=(rnd()&0xE000)|(rnd()%64); vb[(bases[k]+i)&0xFFFF^1]=(u8)(e>>8); vb[(bases[k]+i+1)&0xFFFF^1]=(u8)e; }
  }
  M.vgen++; M.cgen++;
}

static int sprite_oddities=0;
int main(int argc,char**argv){
  if(argc<2){ fprintf(stderr,"usage: mdhost run|render|fuzz|ui|spin ...\n"); return 2; }
  const char*cmd=argv[1];
  if(!strcmp(cmd,"fuzz")){
    u32 trials=argc>2?(u32)atoi(argv[2]):1000; if(argc>3) rs^=(u64)atoll(argv[3])*0x9E3779B97F4A7C15ull;
    u32 bad_trials=0; m68k_ram=md_ram; mdr_init();
    for(u32 t=0;t<trials;t++){ random_state(); int bad=compare_render("fuzz"); if(bad){ bad_trials++; printf("trial %u: %d mismatches (reg1=%02x reg11=%02x reg12=%02x reg16=%02x reg17=%02x reg18=%02x)\n",t,bad,M.reg[1],M.reg[11],M.reg[12],M.reg[16],M.reg[17],M.reg[18]); if(bad_trials>5) break; } }
    printf("%s: %u random VDP states, %u differing\n",bad_trials?"FAIL":"PASS",trials,bad_trials); return bad_trials?1:0;
  }
  if(argc<4){ fprintf(stderr,"need rom and frame count\n"); return 2; }
  boot(argv[2]); u32 frames=(u32)atoi(argv[3]);
  if(!strcmp(cmd,"run")){
    int ref=0,dump=0,sig=0; const char*prefix="frame";
    for(int i=4;i<argc;i++){ if(!strcmp(argv[i],"--ref")) ref=1; else if(!strcmp(argv[i],"--sig")) sig=1; else if(!strcmp(argv[i],"--dump")){ dump=atoi(argv[++i]); prefix=argv[++i]; } }
    (void)ref;
    for(u32 f=0;f<frames;f++){
      md_begin_frame_input(0);
      if(sig){ for(;;){ int fr=md_run(2); printf("%u %u %08x pc %06x cyc %u d %08x %08x %08x %08x sr %04x\n",M.frame-(fr?1:0),(u32)M.line,cpu_sig(),C.pc,C.cycles,C.d[0],C.d[1],C.d[3],C.d[5],C.sr); if(fr) break; } }
      else md_run(0);
      if(dump&&f%dump==0){ md_present_game(); char n[256]; snprintf(n,sizeof n,"%s_%05u.ppm",prefix,f); dump_ppm(n,md_page); }
    }
#ifdef AOT_ON
    { extern u32 aot_ncalls,aot_ninterp; printf("aot calls %u interp %u\n",aot_ncalls,aot_ninterp); }
#endif
    printf("frame %u pc %06x sr %04x cyc %u exc %u ill %u spin %u/%u irq %u ctrl %u vram %u cram %u dma %u disp %02x vint-ev %u\n",M.frame,C.pc,C.sr,C.cycles,C.exc_count,C.illegal_count,C.spin_skips,C.spin_cycles,C.irq_taken,M.n_ctrl_wr,M.n_vram_wr,M.n_cram_wr,M.n_dma,M.reg[1],M.n_vint);
    printf("ev:"); for(int i=0;i<EV_COUNT;i++) printf(" %d",(int)M.ev[i]); printf("\n");
    printf("hashes ram %08x vram %08x cram %08x\n",hash_bytes(md_ram,sizeof md_ram),hash_bytes(md_vram,sizeof md_vram),hash_bytes(md_cram,sizeof md_cram));
    return 0;
  }

  if(!strcmp(cmd,"eetest")){
    /* drive the game's own EEPROM routines: 4A7A load, 4AAE write-back; shadow at $FF F960 (64 words), dirty flags $FFF9E0, $FFFA20, $FFFFE8 */
    for(u32 f=0;f<frames;f++){ md_begin_frame_input(0); md_run(0); }
    #define CALL(pc_) do{ C.sr=0x2700; C.a[7]=0xFFFE00; C.pc=(pc_); u32 n=0; bus_write32(C.a[7]-4,0x400); C.a[7]-=4; while(C.pc!=0x400&&n<4000000){ m68k_step(); n++; } printf("call %06x: %u steps, d0=%08x\n",(pc_),n,C.d[0]); }while(0)
    for(int i=0;i<128;i++) md_ee.mem[i]=0xFF;
    CALL(0x4a7a);
    printf("shadow after load of erased EEPROM:"); for(int i=0;i<8;i++) printf(" %04x",bus_read16(0xFFF960+2*i)); printf("\n");
    /* now preload EEPROM with a pattern and reload */
    for(int i=0;i<128;i++) md_ee.mem[i]=(u8)(i*3+1);
    CALL(0x4a7a);
    int bad=0; for(int i=0;i<64;i++){ u16 w=bus_read16(0xFFF960+2*i), e=(u16)((md_ee.mem[2*i]<<8)|md_ee.mem[2*i+1]); if(w!=e){ if(bad<6) printf("  word %d shadow %04x eeprom %04x\n",i,w,e); bad++; } }
    printf("load mismatches: %d\n",bad);
    /* write: change shadow words 5 and 40, flag them, run write-back until idle */
    bus_write16(0xFFF960+2*5,0xBEEF); bus_write16(0xFFF960+2*40,0xCAFE);
    bus_write8(0xFFF9E0+5,1); bus_write8(0xFFF9E0+40,1); bus_write16(0xFFFA20,1); bus_write16(0xFFFFE8,0x1234);
    for(int k=0;k<8;k++){ CALL(0x4aae); }
    printf("eeprom after write-back: [10..11]=%02x%02x [80..81]=%02x%02x  flags5=%02x flags40=%02x fa20=%04x\n",md_ee.mem[10],md_ee.mem[11],md_ee.mem[80],md_ee.mem[81],bus_read8(0xFFF9E0+5),bus_read8(0xFFF9E0+40),bus_read16(0xFFFA20));
    return 0;
  }
  if(!strcmp(cmd,"stat")){
    /* per-50-frame averages of executed (non-folded) 68000 cycles, IRQs and VDP traffic */
    u32 c0=0,s0=0,i0=0,v0=0,d0=0,k0=0,w0=0;
    for(u32 f=0;f<frames;f++){
      u32 keys=((f%211u)<4u)?K_START:((f%137u)<3u)?K_A:((f%173u)<2u)?K_DOWN:((f%251u)<3u)?K_B:0u;   /* same input script as `spin` */
      md_begin_frame_input(keys); md_run(0);
      if(f%50==49){
        u32 ex=(C.cycles-c0)-(C.spin_cycles-s0);
        printf("f%5u exec68k/frame %7u  folded/frame %7u  irq/frame %.1f  vdpdata/frame %6u  ctrl/frame %5u  dma/frame %.1f  vgen+%u\n",f+1,ex/50,(C.spin_cycles-s0)/50,(C.irq_taken-i0)/50.0,(M.n_data_wr-d0)/50,(M.n_ctrl_wr-k0)/50,(M.n_dma-w0)/50.0,M.vgen-v0);
        c0=C.cycles; s0=C.spin_cycles; i0=C.irq_taken; v0=M.vgen; d0=M.n_data_wr; k0=M.n_ctrl_wr; w0=M.n_dma;
      }
    }
    return 0;
  }
#ifdef USE_REF_EXEC
  if(!strcmp(cmd,"hist")){
    /* instruction histogram of N frames after the warm-up (reference core, no folding): prints the hottest PCs */
    u32 n=argc>4?(u32)atoi(argv[4]):5; C.spin_enable=0;
    for(u32 f=0;f<frames;f++){ md_begin_frame_input(0); md_run(0); }
    pc_hist_on=1; for(u32 f=0;f<n;f++){ md_begin_frame_input(0); md_run(0); } pc_hist_on=0;
    u32 tot=0; for(u32 i=0;i<(1u<<24);i++) tot+=pc_hist[i]; printf("%u instructions in %u frames (%u/frame)\n",tot,n,tot/n);
    { u32 cls[64]={0}; for(u32 o=0;o<65536;o++) cls[o>>10]+=op_hist[o];
      printf("opcode class (top 6 bits) shares:"); for(u32 k=0;k<64;k++) if(cls[k]*100u>=tot) printf(" %02x:%u%%",k,cls[k]*100u/tot); printf("\n");
      printf("hottest opcode words:"); for(int k=0;k<24;k++){ u32 best=0,bi=0; for(u32 o=0;o<65536;o++) if(op_hist[o]>best){ best=op_hist[o]; bi=o; } if(!best) break; printf(" %04x:%u%%",bi,best*100u/tot); op_hist[bi]=0; } printf("\n"); }
    for(int k=0;k<12;k++){ u32 best=0,bi=0; for(u32 i=0;i<(1u<<24);i++) if(pc_hist[i]>best){ best=pc_hist[i]; bi=i; } if(!best) break;
      printf("%06x  %04x %04x %04x  x%u/frame\n",bi,(u32)bus_read16(bi),(u32)bus_read16(bi+2),(u32)bus_read16(bi+4),best/n); pc_hist[bi]=0; }
    return 0;
  }
#endif
#ifdef USE_REF_EXEC
  if(!strcmp(cmd,"cover")){
    /* executed-PC coverage of a long pseudo-random input run (reference core, no folding); writes "pc total busy" to
     * argv[4]; busy = executions in frames that ran more than 3000 instructions outside the idle-wait page */
    C.spin_enable=0; u32 seed=argc>5?(u32)atoi(argv[5]):1;
    static u32 busy[1<<24];
    for(u32 f=0;f<frames;f++){
      seed=seed*1103515245u+12345u; u32 r=(seed>>16)&255u;
      static u32 hold=0,key=0; if(!hold){ hold=4+(r&15u); u32 k=r>>4; key= k<6?K_A: k<8?K_B: k<9?K_START: k<11?K_DOWN: k<13?K_UP: k<14?K_LEFT: k<15?K_RIGHT:0; if((seed>>24)&1) key=0; }
      hold--;
      ntouched=0; pc_hist_on=1; md_begin_frame_input(key); md_run(0); pc_hist_on=0;
      u32 work=0; for(u32 k=0;k<ntouched;k++){ u32 p=touched[k]; if((p>>8)!=0x34) work+=fh[p]; }
      for(u32 k=0;k<ntouched;k++){ u32 p=touched[k]; if(work>3000) busy[p]+=fh[p]; fh[p]=0; }
    }
    FILE*o=fopen(argv[4],"w"); u32 n=0;
    for(u32 i=0;i<(1u<<24);i++) if(pc_hist[i]){ fprintf(o,"%06x %u %u\n",i,pc_hist[i],busy[i]); n++; }
    fclose(o);
    printf("frame %u pcs %u exc %u ill %u pc %06x\n",M.frame,n,C.exc_count,C.illegal_count,C.pc); return 0;
  }
#endif
  if(!strcmp(cmd,"ramdump")){
    for(u32 f=0;f<frames;f++){ u32 keys=((f%211u)<4u)?K_START:((f%137u)<3u)?K_A:((f%173u)<2u)?K_DOWN:((f%251u)<3u)?K_B:0u; md_begin_frame_input(keys); md_run(0); }
    printf("pc %06x sr %04x sp %08x\n",C.pc,C.sr,C.a[7]);
    FILE*o=fopen(argv[4],"wb"); for(u32 i=0;i<65536;i++){ u8 b=((u8*)md_ram)[i^1u]; fputc(b,o); } fclose(o); return 0;
  }
#ifdef USE_REF_EXEC
  if(!strcmp(cmd,"busy")){
    /* per-frame instruction counts outside the idle-wait page (0x3400-0x34FF), and where they ran */
    C.spin_enable=0; static u32 reg[256]; u32 prevtot=0;
    for(u32 f=0;f<frames;f++){
      u32 keys=((f%211u)<4u)?K_START:((f%137u)<3u)?K_A:((f%173u)<2u)?K_DOWN:((f%251u)<3u)?K_B:0u;
      pc_hist_on=1; md_begin_frame_input(keys); md_run(0); pc_hist_on=0;
      u32 tot=0; for(u32 i=0;i<0x40000;i++){ if((i>>8)!=0x34) tot+=pc_hist[i]; } for(u32 i=0xFF0000;i<0x1000000;i++) tot+=pc_hist[i];
      u32 d=tot-prevtot; prevtot=tot;
      if(d>3000){ printf("frame %u busy instr %u\n",f,d); }
    }
    for(u32 i=0;i<0x1000000;i++) if(pc_hist[i]&&((i>>8)!=0x34)) reg[(i>=0xFF0000?128u:0u)+((i>>12)&127u)]+=pc_hist[i];
    for(u32 k=0;k<256;k++) if(reg[k]) printf("%s %05x: %u\n",k>=128?"RAM":"ROM",(k&127u)<<12,reg[k]);
    return 0;
  }
#endif
  if(!strcmp(cmd,"render")){
    u32 step=argc>4?(u32)atoi(argv[4]):50; u32 bad=0,n=0;
    for(u32 f=0;f<frames;f++){ md_begin_frame_input(0); md_run(0); if(f%step==step-1){ int b=compare_render("game"); n++; if(b){ bad++; printf("frame %u: %d mismatches\n",f,b); } } }
    printf("%s: %u real-game states compared, %u differing\n",bad?"FAIL":"PASS",n,bad); return bad?1:0;
  }
  if(!strcmp(cmd,"incr")){
    /* incremental renderer vs a full render of the same state, after every frame of a scripted run */
    static u8 ref[240*160]; u32 bad=0,n=0,pages=0;
    for(u32 f=0;f<frames;f++){
      u32 keys=((f%211u)<4u)?K_START:((f%137u)<3u)?K_A:((f%173u)<2u)?K_DOWN:((f%251u)<3u)?K_B:0u;
      md_begin_frame_input(keys); md_run(0);
      if(M.vgen!=mdr_vgen||M.cgen!=mdr_cgen){ md_present_game(); pages++;
        mdr_full_reference(ref); n++;
        if(memcmp(ref,md_h_fb+md_page*0xA000,240*160)){ bad++; if(bad<6){ int c=0; for(int i=0;i<240*160;i++) if(ref[i]!=md_h_fb[md_page*0xA000+i]) c++; printf("frame %u: %d pixels differ (first line %d)\n",f,c,0); } } }
    }
    printf("%s: %u presented frames compared, %u differing\n",bad?"FAIL":"PASS",n,bad); return bad?1:0;
  }
#ifdef AOT_ON
  if(!strcmp(cmd,"miss")){
    /* cycles run by the interpreter (instead of translated code), by entry pc, over the `spin` input script */
    extern int aot_has(u32);
    u32 heavy=0; static u8 kf[1<<20]; const char*kp=getenv("KEYS");
    if(kp){ FILE*kfp=fopen(kp,"r"); u32 a,b; while(fscanf(kfp,"%u %u",&a,&b)==2) if(a<(1u<<20)) kf[a]=(u8)b; fclose(kfp); }
    for(u32 f=0;f<frames;f++){
      u32 keys=kp?kf[M.frame+1u]:((f%211u)<4u)?K_START:((f%137u)<3u)?K_A:((f%173u)<2u)?K_DOWN:((f%251u)<3u)?K_B:0u;
      u32 c0=C.cycles; static u32 prev_m; u32 m0=0; for(u32 i=0;i<0x40000;i++) m0+=aot_miss[i]; for(u32 i=0xFF0000;i<0x1000000;i++) m0+=aot_miss[i];
      md_begin_frame_input(keys); md_run(0);
      u32 m1=0; for(u32 i=0;i<0x40000;i++) m1+=aot_miss[i]; for(u32 i=0xFF0000;i<0x1000000;i++) m1+=aot_miss[i];
      if(m1-m0>60000u){ heavy++; if(heavy<60) printf("frame %u interp %u cyc %u\n",M.frame,m1-m0,C.cycles-c0); }
      (void)c0; (void)prev_m;
    }
    { u64 tot=0; for(u32 i=0;i<(1u<<24);i++) tot+=aot_miss[i]; printf("TOTAL interp guest cycles %llu over %u frames (%.1f%% of %llu)\n",(unsigned long long)tot,frames,100.0*tot/((double)frames*127800.0),(unsigned long long)frames*127800ull); }
    for(int k=0;k<(getenv("TOP")?atoi(getenv("TOP")):40);k++){ u32 best=0,bi=0; for(u32 i=0;i<(1u<<24);i++) if(aot_miss[i]>best){ best=aot_miss[i]; bi=i; } if(!best) break;
      printf("%06x %10u has %d  %04x %04x\n",bi,best,aot_has(bi),(u32)bus_read16(bi),(u32)bus_read16(bi+2)); aot_miss[bi]=0; }
    return 0;
  }
#endif
  if(!strcmp(cmd,"spin")){
    /* Frame-end state signatures of a scripted run.  mode "line": every frame is run scanline by scanline with
     * md_run(2) (never skips whole lines); mode "frame" (default): md_run(0) (idle chunk skipping active);
     * "off": md_run(0) with the loop fast-forward disabled.  The three outputs must be identical. */
    const char*mode=argc>4?argv[4]:"frame";
    if(!strcmp(mode,"off")) C.spin_enable=0;
    for(u32 f=0;f<frames;f++){
      u32 keys=((f%211u)<4u)?K_START:((f%137u)<3u)?K_A:((f%173u)<2u)?K_DOWN:((f%251u)<3u)?K_B:0u;   /* deterministic input script */
      md_begin_frame_input(keys);
      if(!strcmp(mode,"line")){ for(;;){ if(md_run(2)) break; } }
      else if(!strcmp(mode,"step")&&f>=(u32)atoi(getenv("STEPF"))){ C.spin_enable=0;
        for(;;){ printf("I %06x c%u sr%04x d%08x %08x %08x %08x %08x %08x %08x %08x a%08x %08x %08x %08x %08x %08x %08x %08x\n",C.pc,C.cycles,C.sr,C.d[0],C.d[1],C.d[2],C.d[3],C.d[4],C.d[5],C.d[6],C.d[7],C.a[0],C.a[1],C.a[2],C.a[3],C.a[4],C.a[5],C.a[6],C.a[7]);
          if(md_run(1)) break; } }
      else { if(!strcmp(mode,"step")) C.spin_enable=0; md_run(0); }
      u32 h=cpu_sig(); h=h*31u+hash_bytes(md_ram,sizeof md_ram); h=h*31u+hash_bytes(md_vram,sizeof md_vram); h=h*31u+hash_bytes(md_cram,sizeof md_cram); h=h*31u+hash_bytes(md_vsram,sizeof md_vsram);
      h=h*31u+(u32)M.hc*7u+(u32)M.next_line+M.line_end+M.n_vint*13u+M.n_hint*17u+M.n_ctrl_wr*19u+M.n_vram_wr*23u+C.irq_taken;
      { u32 vh=hash_bytes(md_vram,sizeof md_vram)*31u+hash_bytes(md_cram,sizeof md_cram)*7u+hash_bytes(md_vsram,sizeof md_vsram); printf("%u pc %06x cyc %u sig %08x vis %08x vint %u hint %u\n",M.frame,C.pc,C.cycles,h,vh,M.n_vint,M.n_hint); }
    }
    fprintf(stderr,"mode %s: spin skips %u cycles %u\n",mode,C.spin_skips,C.spin_cycles);
    return 0;
  }
  
  return 2;
}
