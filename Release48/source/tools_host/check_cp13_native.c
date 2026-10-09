/* Full state comparison of the native shared transfer routine with CP12. */
#include <stdio.h>
#include <string.h>
#include "../additional/aot/aotrt.h"
uint16_t md_h_pal[256],md_h_dispcnt;uint8_t md_h_fb[2*0xA000];
uint32_t md_h_keys,md_h_ticks,md_h_hb[32];int md_h_reset_requested;
MDCfg md_cfg;u32 md_page,md_frame_ticks,md_render_ticks,md_last_frame_ticks,md_frames_off;
u32 aot_lim,aot_rgen[128];
unsigned __aeabi_uidiv(unsigned n,unsigned d){return d?n/d:0;}
void md_eeprom_store(u32 a,u32 v){(void)a;(void)v;}void md_eeprom_load(void){}
void mdsnd_init(void){}void mdsnd_busreq(u32 v){(void)v;}void mdsnd_reset(u32 v){(void)v;}
void mdsnd_psg_68k(u32 v){(void)v;}void mdsnd_ym_68k(u32 a,u32 v){(void)a;(void)v;}
u32 native_fn(u32),reference_fn(u32);
static u32 seed=0x11479021;
static u32 rnd(void){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
typedef struct{MD m;M68K c;u16 vram[32768],cram[64],vsram[64];u32 dirty[64];} State;
static State initial,fast,slow;
static void take(State *s){s->m=M;s->c=C;memcpy(s->vram,md_vram,sizeof md_vram);memcpy(s->cram,md_cram,sizeof md_cram);memcpy(s->vsram,md_vsram,sizeof md_vsram);memcpy(s->dirty,md_vdirty,sizeof md_vdirty);}
static void restore(const State *s){M=s->m;C=s->c;memcpy(md_vram,s->vram,sizeof md_vram);memcpy(md_cram,s->cram,sizeof md_cram);memcpy(md_vsram,s->vsram,sizeof md_vsram);memcpy(md_vdirty,s->dirty,sizeof md_vdirty);}
int main(void){
  static const u8 rom[4096]={0};for(u32 i=0;i<128;i++)m68k_romp[i]=rom;
  m68k_romsize=0x40000;m68k_ram=md_ram;
  for(u32 i=0;i<32768;i++){md_vram[i]=(u16)rnd();md_ram[i]=(u16)rnd();}
  for(u32 i=0;i<100000;i++){
    memset(&M,0,sizeof M);memset(&C,0,sizeof C);
    M.code=(i%4)?1:(u8)rnd();M.addr=rnd();M.reg[15]=(i%5)?2:(u8)rnd();
    if(i%3)M.addr&=~3u;
    M.reg[19]=1+(rnd()&31);M.fill_pending=(rnd()&63)==0;
    M.vgen=rnd();M.cgen=rnd();M.n_data_wr=rnd();M.n_vram_wr=rnd();
    C.fx=rnd();C.sr=(u16)rnd();C.cycles=rnd();C.ring_pos=rnd();
    for(u32 j=0;j<M68K_RING;j++)C.ring[j]=rnd();
    for(u32 j=0;j<8;j++){C.a[j]=rnd();C.d[j]=rnd();}
    C.a[0]=0xffff0000u|(rnd()&0xffffu);if(i%4)C.a[0]&=~3u;
    if(i%17==0)C.a[0]=rnd()&0x1ffffu;
    C.a[5]=(i%19)?0xc00000u:0xc00002u;C.a[7]=0xfffff000u;
    C.pc=0x3218u;aot_lim=C.cycles+(rnd()%2048u)+1u;
    for(u32 j=0;j<EV_COUNT;j++)M.ev[j]=(rnd()&1)?0xffffffffu:rnd();
    u32 entry=32+(rnd()%9u);take(&initial);
    u32 r1=native_fn(entry);take(&fast);restore(&initial);
    u32 r2=reference_fn(entry);take(&slow);
    if(r1!=r2||memcmp(&fast,&slow,sizeof fast)){
      printf("FAIL case %u entry %u pc %x/%x cyc %u/%u sr %x/%x addr %x/%x fx %u/%u\n",i,entry,fast.c.pc,slow.c.pc,fast.c.cycles,slow.c.cycles,fast.c.sr,slow.c.sr,fast.m.addr,slow.m.addr,fast.c.fx,slow.c.fx);return 1;
    }
  }
  puts("100,000 native routine states exactly match CP12 across cycle splits, bus fallbacks and VDP modes");
}
