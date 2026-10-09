/* Genesis runtime entry point, frame loop, pacing and presentation (CP08). */
#include "md.h"
#include "mdsnd.h"

#define R16(a) (*(volatile u16*)(a))
#define R32(a) (*(volatile u32*)(a))
#define FRAME_TICKS 280896u            /* one GBA frame in CPU cycles: 59.7275 Hz */
#define RENDER_MIN_TICKS (FRAME_TICKS*2u)

MDCfg md_cfg;
u32 md_page;
u32 md_frame_ticks, md_render_ticks, md_last_frame_ticks, md_frames_off;
static u32 next_deadline, last_render_tick, force_render;
#define frames_off md_frames_off

/* ---- EEPROM saves in GBA SRAM ----
 * 0x0E007000 + slot*128 (slot = Additional Content entry index - 2, 0..7): the 128-byte EEPROM image of each
 * text adventure.  0x0E007F00: "MDEE" + version, written the first time any game saves (until then nothing in
 * SRAM is touched and every EEPROM reads as erased, 0xFF).  The rest of the GBA SRAM is not touched. */
#define EE_SRAM_BASE 0x0E007000u
#define EE_SRAM_MAGIC 0x0E007F00u
static u32 ee_slot(void){ return md_cfg.index>=2u&&md_cfg.index<10u?md_cfg.index-2u:7u; }
/* GamePak SRAM shares the cartridge bus with the ROM, so any access made by code that is executing from ROM reads or
 * writes garbage on real hardware (emulators usually do not model this).  Every SRAM byte therefore goes through this one
 * tiny routine, which lives in IWRAM (.hot): d and s may each point into SRAM or into ordinary RAM/ROM. */
__attribute__((noinline,section(".hot"),target("thumb"))) static void sram_xfer(volatile u8*d,const volatile u8*s,u32 n){ while(n--) *d++=*s++; }
static const u8 ee_ff[128]={0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
static const u8 ee_magic[8]={'M','D','E','E',1,0,0,0};
static int ee_formatted(void){ u8 m[8]; sram_xfer(m,(const volatile u8*)EE_SRAM_MAGIC,8); for(u32 i=0;i<5u;i++) if(m[i]!=ee_magic[i]) return 0; return 1; }
void md_eeprom_load(void){
  if(ee_formatted()) sram_xfer(md_ee.mem,(const volatile u8*)(EE_SRAM_BASE+ee_slot()*128u),128);
  else for(u32 i=0;i<128u;i++) md_ee.mem[i]=0xFFu;
}
void md_eeprom_store(u32 addr,u32 v){
  if(!ee_formatted()){                                    /* first save ever: erase all eight images, then stamp the magic */
    for(u32 k=0;k<8u;k++) sram_xfer((volatile u8*)(EE_SRAM_BASE+k*128u),ee_ff,128);
    sram_xfer((volatile u8*)(EE_SRAM_BASE+ee_slot()*128u),md_ee.mem,128);
    sram_xfer((volatile u8*)EE_SRAM_MAGIC,ee_magic,8);
    return;
  }
  u8 b=(u8)v; sram_xfer((volatile u8*)(EE_SRAM_BASE+ee_slot()*128u+(addr&127u)),&b,1);
}

/* ---- sound output (CP09) ----
 * The sound CPU + synthesiser (mdsnd.c) renders one frame of SND_N stereo samples per emulated frame into a ring of NBUF
 * buffers; Direct Sound A/B play them through DMA1/2 clocked by Timer0 (13,379 Hz), and Timer1 (cascaded, one overflow per
 * buffer) interrupts to chain to the next ready buffer.  Same scheme as PSC8 (psc8.c): each buffer carries 32 extra samples
 * (the start of the next one) because the FIFO has already fetched them when the DMA is re-armed with a 32 sample skip.
 * The ring is also the emulator's clock: md_wait_pace() waits until a buffer is free. */
#define NBUF 4
static s8 pcm[NBUF][2][SND_N+32] __attribute__((aligned(4)));
static volatile u8 ready[NBUF];
static volatile u8 playbuf;
static u8 prod;
static volatile u32 audio_frames, audio_late;
static u8 audio_on;
extern u32 snd_tz, snd_tz_max, snd_tr, snd_tr_max;
u32 md_snd_ticks, md_snd_max;                                   /* CPU cycles the last / the slowest audio_produce took */
static const u32 silence[4]={0,0,0,0};

static void rearm(u32 b,u32 skip){
  R16(0x040000c6)=0; R16(0x040000d2)=0;
  R32(0x040000bc)=(u32)&pcm[b][0][skip]; R32(0x040000c0)=0x040000a0; R32(0x040000c4)=0xb6400004;
  R32(0x040000c8)=(u32)&pcm[b][1][skip]; R32(0x040000cc)=0x040000a4; R32(0x040000d0)=0xb6400004;
}
static void audio_irq(void){
  if(R16(0x04000202)&0x10){
    u32 next=(playbuf+1u)&(NBUF-1);
    if(ready[next]){ playbuf=(u8)next; ready[next]=0; rearm(playbuf,32); }
    else { audio_late++; R16(0x040000c6)=0; R16(0x040000d2)=0; R32(0x040000bc)=R32(0x040000c8)=(u32)silence; R32(0x040000c4)=R32(0x040000d0)=0xb7400004; }
    audio_frames++;
  }
  R16(0x04000202)=0x10;
}
static void audio_stop(void){
  R16(0x04000208)=0; R16(0x04000200)=0;
  R32(0x04000100)=0; R32(0x04000104)=0;                       /* timers 0/1 off */
  R16(0x040000c6)=0; R16(0x040000d2)=0; R16(0x040000ba)=0;    /* DMA 1/2 (and 0) off */
  R16(0x04000084)=0; R16(0x04000082)=0;
  R16(0x04000202)=0xffff;
}
static void audio_start(void){
  R16(0x04000084)=0x80; R16(0x04000082)=0x9a0c; R16(0x04000080)=0; R16(0x04000088)=0x200;
  R32(0x03007ffc)=(u32)audio_irq; R16(0x04000200)=0x10; R16(0x04000202)=0xffff;
  rearm(0,0);
  R32(0x04000104)=0x00c40000u|(0x10000u-SND_N); R32(0x04000100)=0x00800000u|(0x10000u-1254u);
  R16(0x04000208)=1;
  __asm__ volatile("mrs r0,cpsr\n bic r0,r0,#0x80\n msr cpsr_c,r0":::"r0","memory");
  audio_on=1;
}
/* render one frame of sound into the next free ring buffer; returns 0 if the ring is full */
static u32 audio_produce(void){
  u32 b=prod;
  if(b==playbuf||ready[b]) return 0;
  u32 t0=md_ticks();
  mdsnd_frame(pcm[b][0],pcm[b][1]);
  md_snd_ticks=md_ticks()-t0; if(md_snd_ticks>md_snd_max) md_snd_max=md_snd_ticks;
  u32 prev=(b-1u)&(NBUF-1);
  for(u32 i=0;i<32u;i++){ pcm[prev][0][SND_N+i]=pcm[b][0][i]; pcm[prev][1][SND_N+i]=pcm[b][1][i]; }
  ready[b]=1; prod=(u8)((b+1u)&(NBUF-1));
  return 1;
}

void md_soft_reset(void){
  audio_stop();
  __asm__ volatile("mov r0,#0\n swi 0x10000\n swi 0":::"r0","memory");
  for(;;){}
}

void md_wait_pace(void){
  if(audio_on){ while(prod==playbuf||ready[prod]){} return; }       /* the audio ring is the clock (59.73 Hz) */
  u32 now=md_ticks();
  if((s32)(next_deadline-now)>0){ while((s32)(next_deadline-md_ticks())>0){} }
  else if((s32)(now-next_deadline)>(s32)(FRAME_TICKS*6u)) next_deadline=now;   /* far behind: resync, never "catch up" */
  next_deadline+=FRAME_TICKS;
}

void md_heartbeat(void){
  volatile u32*d=MD_HB;
  d[0]=0x4d444558u; d[1]=M.frame; d[2]=C.pc; d[3]=M.reg[1];
  d[4]=C.sr; d[5]=M.vgen; d[6]=M.cgen; d[7]=mdr_vgen;
  d[8]=C.exc_count; d[9]=C.illegal_count; d[10]=C.irq_taken; d[11]=C.spin_skips;
  d[12]=M.n_dma; d[13]=md_last_frame_ticks; d[14]=md_render_ticks; d[15]=!mdr_image_blank();
  d[16]=M.ev[EV_DISPON]; d[17]=M.ev[EV_VISIBLE]; d[18]=M.n_vram_wr; d[19]=M.n_cram_wr;
  d[20]=M.n_ctrl_wr; d[21]=C.last_exc_pc; d[22]=C.last_exc_vec; d[23]=C.stopped;
  d[24]=md_snd_ticks; d[25]=md_snd_max; d[26]=audio_late; d[27]=audio_frames; d[28]=snd_tz; d[29]=snd_tz_max; d[30]=snd_tr; d[31]=snd_tr_max;
}

/* Render the current guest image (or the boot/status panel) into the back page and flip. */
void md_present_game(void){
  u32 t0=md_ticks(), back=md_page^1u;
  mdr_render(back);
  if(!mdr_image_blank()) MD_EVENT(EV_VISIBLE);
  md_set_dispcnt((u16)(0x0404u|(back<<4)));
  md_page=back; last_render_tick=md_ticks(); md_render_ticks=last_render_tick-t0; force_render=0;
}

static void frame_epilogue(void){
  if(C.spin_skips&&M.ev[EV_SPIN]==0xFFFFFFFFu) MD_EVENT(EV_SPIN);
  if((C.illegal_count|C.linea_count|C.linef_count|C.priv_count)&&M.ev[EV_EXC]==0xFFFFFFFFu) MD_EVENT(EV_EXC);
  u32 dispon=(M.reg[1]&0x40u)!=0;
  if(dispon){
    frames_off=0;
    if((M.vgen!=mdr_vgen||M.cgen!=mdr_cgen||force_render)&&(u32)(md_ticks()-last_render_tick)>=RENDER_MIN_TICKS) md_present_game();
  } else {
    frames_off++;
    /* display off: the screen shows black; present once when it goes off, then nothing until it comes back */
    if(frames_off==1u) md_present_game();
  }
  md_heartbeat();
}

static void hw_init(void){
  R16(0x04000000)=0x80;                         /* forced blank while we set up */
  R16(0x04000084)=0;                            /* sound off */
  R16(0x04000020)=R16(0x04000026)=0x100; R16(0x04000022)=R16(0x04000024)=0; R32(0x04000028)=R32(0x0400002c)=0;
  R16(0x0400000c)=0; R16(0x04000050)=0; R16(0x0400004c)=0; R16(0x04000040)=R16(0x04000042)=R16(0x04000044)=R16(0x04000046)=0;
  R16(0x04000108)=0; R16(0x0400010a)=0x0080;    /* Timer2: CPU clock */
  R16(0x0400010c)=0; R16(0x0400010e)=0x0084;    /* Timer3: cascade -> 32-bit free-running counter */
  R16(0x04000208)=0;                            /* IME off: polled runtime */
  R16(0x04000204)=0x4317;                       /* ROM 3/1 wait states + prefetch (the recompiled code runs from ROM) */
}

void md_main(void){
  const u32*cfg=(const u32*)0x0203ff00;
  md_cfg.table=cfg[0]; md_cfg.size=cfg[1]; md_cfg.flags=cfg[2]; md_cfg.index=cfg[3];
  hw_init();
  mdr_init();
  md_init_machine((const u32*)md_cfg.table,md_cfg.size);
#ifdef AOT_ON
  { void aot_select_index(u32); aot_select_index(md_cfg.index); }
#endif
  { /* title: the Additional Content menu passes its entry name (cfg[4], a ROM string); else the cartridge header */
    char*t=md_cfg.title; u32 n=0,last=0; u32 sp=cfg[4];
    if((sp>>24)==0x08u){ const char*q=(const char*)sp; while(q[n]&&n<48u){ t[n]=q[n]; n++; } last=n; }
    else {
      u32 off=0x150u;
      for(u32 k=0;k<48u;k++){ u32 c=bus_read8(off+k); if(c<32u||c>126u) c=' '; t[n]=(char)c; n++; if(c!=' ') last=n; }
      if(!last){ n=0; for(u32 k=0;k<48u;k++){ u32 c=bus_read8(0x120u+k); if(c<32u||c>126u) c=' '; t[n]=(char)c; n++; if(c!=' ') last=n; } }
    }
    t[last]=0;
  }
  mdsnd_init();
  mdsnd_frame(pcm[0][0],pcm[0][1]); mdsnd_frame(pcm[1][0],pcm[1][1]);          /* two silent frames to start the ring */
  for(u32 i=0;i<32u;i++){ pcm[0][0][SND_N+i]=pcm[1][0][i]; pcm[0][1][SND_N+i]=pcm[1][1][i]; }
  ready[1]=1; playbuf=0; prod=2; audio_start();
  md_page=1; next_deadline=md_ticks()+FRAME_TICKS; last_render_tick=md_ticks();
  u32 prev=md_keys_raw();
  for(;;){
    u32 keys=md_keys_raw(); (void)prev;
    if((keys&(K_A|K_B|K_SELECT|K_START))==(K_A|K_B|K_SELECT|K_START)) md_soft_reset();
    md_begin_frame_input(keys);
    u32 t0=md_ticks();
    md_run(0);
    audio_produce();
    md_last_frame_ticks=md_ticks()-t0;
    frame_epilogue();
    md_wait_pace();
  }
}
