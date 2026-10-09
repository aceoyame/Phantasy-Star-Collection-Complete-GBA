/* Direct data-port access versus the original ordered word bus calls. */
#include <stdio.h>
#include <string.h>
#include "../additional/md.h"
uint16_t md_h_pal[256],md_h_dispcnt;uint8_t md_h_fb[2*0xA000];
uint32_t md_h_keys,md_h_ticks,md_h_hb[32];int md_h_reset_requested;
MDCfg md_cfg;u32 md_page,md_frame_ticks,md_render_ticks,md_last_frame_ticks,md_frames_off;
unsigned __aeabi_uidiv(unsigned n,unsigned d){return d?n/d:0;}
void md_eeprom_store(u32 a,u32 v){(void)a;(void)v;}
void md_eeprom_load(void){}
void mdsnd_init(void){}void mdsnd_busreq(u32 v){(void)v;}void mdsnd_reset(u32 v){(void)v;}
void mdsnd_psg_68k(u32 v){(void)v;}void mdsnd_ym_68k(u32 a,u32 v){(void)a;(void)v;}
static u32 seed=0x3917429;
static u32 rnd(void){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
typedef struct{MD m;M68K c;u16 vram[32768],cram[64],vsram[64];u32 dirty[64];} State;
static State initial,fast,slow;
static void take(State *s){s->m=M;s->c=C;memcpy(s->vram,md_vram,sizeof md_vram);memcpy(s->cram,md_cram,sizeof md_cram);memcpy(s->vsram,md_vsram,sizeof md_vsram);memcpy(s->dirty,md_vdirty,sizeof md_vdirty);}
static void restore(const State *s){M=s->m;C=s->c;memcpy(md_vram,s->vram,sizeof md_vram);memcpy(md_cram,s->cram,sizeof md_cram);memcpy(md_vsram,s->vsram,sizeof md_vsram);memcpy(md_vdirty,s->dirty,sizeof md_vdirty);}
int main(void){
  for(u32 i=0;i<32768;i++)md_vram[i]=(u16)rnd();
  for(u32 i=0;i<20000;i++){
    memset(&M,0,sizeof M);memset(&C,0,sizeof C);
    M.code=(u8)rnd();M.addr=rnd();M.reg[15]=(u8)rnd();
    M.reg[19]=1+(rnd()&31);M.fill_pending=(rnd()&7)==0;
    M.vgen=rnd();M.cgen=rnd();M.n_data_wr=rnd();M.n_vram_wr=rnd();
    C.fx=rnd();C.ring_pos=rnd();for(u32 j=0;j<M68K_RING;j++)C.ring[j]=rnd();
    for(u32 j=0;j<EV_COUNT;j++)M.ev[j]=(rnd()&1)?0xffffffffu:rnd();
    u32 v=rnd();take(&initial);
    if(i&1)md_vdp_write32(v);else md_vdp_write16(v);
    take(&fast);restore(&initial);
    if(i&1){bus_write16(0xc00000,(u16)(v>>16));bus_write16(0xc00002,(u16)v);}
    else bus_write16(0xc00000,(u16)v);
    take(&slow);
    if(memcmp(&fast,&slow,sizeof fast)){printf("FAIL VDP case %u code %u addr %x inc %u fill %u\n",i,initial.m.code,initial.m.addr,initial.m.reg[15],initial.m.fill_pending);return 1;}
  }
  puts("20,000 direct VDP accesses exactly match ordered bus writes (full CPU/VDP/VRAM state)");
}
