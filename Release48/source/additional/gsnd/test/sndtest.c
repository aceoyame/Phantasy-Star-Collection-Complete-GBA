/* sndtest: run a Genesis adventure through the MD runtime on the PC with the real sound driver + mdsnd synthesis.
 *   sndtest ROM FRAMES OUT.wav [-inj frame:addr:val,...] [-ev LOG] [-nopsg] [-keys]
 * Writes a 13,379 Hz stereo WAV and (optionally) a gsim-format log of every YM2612/PSG write for the reference renderer. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "md.h"
#include "mdsnd.h"
uint16_t md_h_pal[256]; uint8_t md_h_fb[2*0xA000] __attribute__((aligned(4))); uint16_t md_h_dispcnt; uint32_t md_h_keys; uint32_t md_h_ticks; uint32_t md_h_hb[32]; int md_h_reset_requested;
unsigned __aeabi_uidiv(unsigned n,unsigned d){ return d?n/d:0; }
MDCfg md_cfg; u32 md_page; u32 md_frame_ticks, md_render_ticks, md_last_frame_ticks, md_frames_off;
void md_present_game(void){ unsigned back=md_page^1u; mdr_render(back); md_page=back; }
void md_eeprom_store(u32 a,u32 v){ (void)a; (void)v; }
void md_eeprom_load(void){ for(int i=0;i<128;i++) md_ee.mem[i]=0xFF; }
void md_wait_pace(void){}
void md_heartbeat(void){}
void md_soft_reset(void){ printf("soft reset requested\n"); exit(0); }
u32 aot_miss[1<<24];
extern int snd_nopsg;
extern unsigned snd_dbg(unsigned);
static int dbgevery; static const char*zdump;
extern void (*snd_ev_hook)(u32 part,u32 reg,u32 val,u32 tstate);
static FILE *lf; static u32 frame; static int nopsg;
static void hook(u32 part,u32 reg,u32 val,u32 t){
  if(lf){ if(part==2) fprintf(lf,"F%u T%u PSG %02x\n",frame,t,val); else fprintf(lf,"F%u T%u YM%u %02x %02x\n",frame,t,part,reg,val); }
}
static u8*load(const char*path,u32*size){
  FILE*f=fopen(path,"rb"); if(!f){ perror(path); exit(2); }
  fseek(f,0,SEEK_END); long n=ftell(f); fseek(f,0,SEEK_SET);
  u8*b=malloc((size_t)n+4096); memset(b,0,(size_t)n+4096); if(fread(b,1,(size_t)n,f)!=(size_t)n) exit(2); fclose(f); *size=(u32)n; return b;
}
static void wr32(FILE*f,u32 v){ fwrite(&v,4,1,f); } static void wr16(FILE*f,unsigned v){ fwrite(&v,2,1,f); }
extern int snd_noaot, snd_noskip, snd_slice; extern FILE *snd_trace;
extern unsigned long long snd_cnt[4];
int main(int argc,char**argv){
  if(argc<4){ fprintf(stderr,"usage: sndtest ROM FRAMES OUT.wav [-inj f:a:v,...] [-ev LOG] [-nopsg] [-keys]\n"); return 2; }
  u32 size; u8*r=load(argv[1],&size); u32 frames=atoi(argv[2]); FILE*wf=fopen(argv[3],"wb");
  struct { u32 f,a,v; } inj[512]; int ninj=0; int keys_on=0;
  for(int i=4;i<argc;i++){
    if(!strcmp(argv[i],"-inj")&&i+1<argc){ char*s=strdup(argv[++i]); for(char*t=strtok(s,",");t;t=strtok(0,",")){ u32 f,a,v; if(sscanf(t,"%u:%x:%x",&f,&a,&v)==3&&ninj<512){ inj[ninj].f=f; inj[ninj].a=a; inj[ninj].v=v; ninj++; } } }
    else if(!strcmp(argv[i],"-ev")&&i+1<argc) lf=fopen(argv[++i],"w");
    else if(!strcmp(argv[i],"-nopsg")) nopsg=1;
    else if(!strcmp(argv[i],"-noaot")) snd_noaot=1;
    else if(!strcmp(argv[i],"-trace")&&i+1<argc) snd_trace=fopen(argv[++i],"w");
    else if(!strcmp(argv[i],"-noskip")) snd_noskip=1;
    else if(!strcmp(argv[i],"-slice")&&i+1<argc) snd_slice=atoi(argv[++i]);
    else if(!strcmp(argv[i],"-keys")) keys_on=1;
    else if(!strcmp(argv[i],"-dbg")&&i+1<argc) dbgevery=atoi(argv[++i]);
    else if(!strcmp(argv[i],"-zdump")&&i+1<argc) zdump=argv[++i];
  }
  snd_nopsg=nopsg;
  for(u32 i=0;i<128;i++) m68k_romp[i]=(i<(size>>12))?r+i*4096u:0;
  m68k_romsize=size; m68k_ram=md_ram; m68k_init_tables(); md_reset_game(); md_cfg.size=size; md_cfg.index=2;
  mdsnd_init(); snd_ev_hook=hook;
  /* WAV header (16-bit stereo) */
  u32 nsamp=frames*SND_N;
  fwrite("RIFF",1,4,wf); wr32(wf,36+nsamp*4); fwrite("WAVEfmt ",1,8,wf); wr32(wf,16); wr16(wf,1); wr16(wf,2); wr32(wf,13379); wr32(wf,13379*4); wr16(wf,4); wr16(wf,16);
  fwrite("data",1,4,wf); wr32(wf,nsamp*4);
  s8 l[SND_N], rr[SND_N]; int16_t pcm[SND_N*2];
  for(frame=0;frame<frames;frame++){
    u32 keys=keys_on?(((frame%211u)<4u)?K_START:((frame%137u)<3u)?K_A:((frame%173u)<2u)?K_DOWN:((frame%251u)<3u)?K_B:0u):0u;
    for(int i=0;i<ninj;i++) if(inj[i].f==frame) md_zram[inj[i].a&0x1FFF]=(u8)inj[i].v;
    md_begin_frame_input(keys); md_run(0);
    mdsnd_frame(l,rr);
    if(dbgevery&&(frame%dbgevery)==0) printf("f%u pc %04x skipok %u run %u busreq %u rst %u ymst %u intp %u iff %u | 1c07 %02x 1c0a %02x 1c0b %02x 1c0c %02x | slices %u skips %u\n",frame,snd_dbg(0),snd_dbg(1),snd_dbg(2),snd_dbg(3),snd_dbg(4),snd_dbg(5),snd_dbg(6),snd_dbg(7),md_zram[0x1C07],md_zram[0x1C0A],md_zram[0x1C0B],md_zram[0x1C0C],snd_stats.z80_slices,snd_stats.z80_skips);
    for(u32 i=0;i<SND_N;i++){ pcm[2*i]=(int16_t)(l[i]*256); pcm[2*i+1]=(int16_t)(rr[i]*256); }
    fwrite(pcm,2,SND_N*2,wf);
  }
  fclose(wf); if(lf) fclose(lf);
  if(zdump){ FILE*z=fopen(zdump,"wb"); fwrite(md_zram,1,0x2000,z); fclose(z); }
  printf("frames %u  keyons %u misses %u  z80 slices %u skips %u  events %u (max/frame %u, dropped %u)\n",frames,snd_stats.keyons,snd_stats.misses,snd_stats.z80_slices,snd_stats.z80_skips,snd_stats.events,snd_stats.max_events,snd_stats.dropped);
  fprintf(stderr,"mixcount mix1 %llu mix2 %llu mixn %llu\n",snd_cnt[0],snd_cnt[1],snd_cnt[2]);
  return 0;
}
