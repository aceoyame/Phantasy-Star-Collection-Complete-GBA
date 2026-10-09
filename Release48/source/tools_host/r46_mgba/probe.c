/* Headless mGBA 0.10.5 bridge. Test host only; never linked into the ROM. */
#include <mgba/flags.h>
#include <mgba/core/core.h>
#include <mgba/debugger/debugger.h>
#include <mgba/internal/arm/arm.h>
#include <mgba/internal/gba/gba.h>
#include <mgba/internal/gba/audio.h>
#include <mgba-util/vfs.h>
#include <mgba/core/log.h>
#include <mgba/core/blip_buf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static struct mCore* core;
static struct mDebugger debugger;
static uint32_t pixels[240*160];
static ssize_t breakpoint=-1;
static int hit;
static int profile;
static uint64_t hist[0x100000/16+0x8000/16];
static FILE* pcm;
static struct mAVStream stream;
static void quiet_log(struct mLogger* l,int c,enum mLogLevel lev,const char* f,va_list a){(void)l;(void)c;(void)lev;(void)f;(void)a;}
static struct mLogger logger={.log=quiet_log};
static void audio_frame(struct mAVStream* s,int16_t l,int16_t r){
 (void)s;if(pcm){int16_t p[2]={l,r};fwrite(p,2,2,pcm);}
}
int mg_load(const char* path,uint32_t entry){
 mLogSetDefaultLogger(&logger);
 if(core){core->deinit(core);core=NULL;}
 core=mCoreCreate(mPLATFORM_GBA);if(!core||!core->init(core))return -1;
 mCoreInitConfig(core,NULL);mCoreConfigSetDefaultIntValue(&core->config,"idleOptimization",-1);
 /* Bare mCore opts are zeroed; frontends normally supply their volume.
  * Set it explicitly or postAudioFrame silently captures all-zero PCM. */
 mCoreConfigSetDefaultIntValue(&core->config,"volume",GBA_AUDIO_VOLUME_MAX);
 mCoreConfigSetDefaultIntValue(&core->config,"mute",0);
 core->opts.volume=GBA_AUDIO_VOLUME_MAX;core->opts.mute=false;
 core->loadConfig(core,&core->config);if(!mCoreLoadFile(core,path))return -2;
 core->setVideoBuffer(core,pixels,240);core->setAudioBufferSize(core,4096);
 memset(&stream,0,sizeof(stream));stream.postAudioFrame=audio_frame;core->setAVStream(core,&stream);
 core->reset(core);GBASkipBIOS(core->board);
 memset(&debugger,0,sizeof(debugger));debugger.type=DEBUGGER_CUSTOM;
 mDebuggerAttach(&debugger,core);breakpoint=-1;hit=0;
 if(entry)core->writeRegister(core,"pc",&entry);return 0;
}
void mg_break(uint32_t a){
 if(breakpoint>=0)debugger.platform->clearBreakpoint(debugger.platform,breakpoint);
 breakpoint=-1;hit=0;debugger.state=DEBUGGER_RUNNING;
 if(a!=0xffffffffu){struct mBreakpoint bp={.address=a&~1u,.segment=-1,.type=BREAKPOINT_HARDWARE};breakpoint=debugger.platform->setBreakpoint(debugger.platform,&bp);}
}
void mg_run(int frames,uint32_t keys){
 core->setKeys(core,keys);if(hit)return;
 uint32_t end=core->frameCounter(core)+frames;
 while(core->frameCounter(core)<end){
  if(profile){
   struct ARMCore*cpu=core->cpu;uint32_t pc=cpu->gprs[15]-(cpu->cpsr.t?4:8);uint64_t before=mTimingGlobalTime(core->timing);
   mDebuggerRun(&debugger);uint64_t cost=mTimingGlobalTime(core->timing)-before;
   if(pc>=0x08800000u&&pc<0x08900000u)hist[(pc-0x08800000u)/16u]+=cost;
   else if(pc>=0x03000000u&&pc<0x03008000u)hist[0x100000/16+(pc-0x03000000u)/16]+=cost;
  }else if(breakpoint>=0)mDebuggerRun(&debugger);else core->runLoop(core);
  if(debugger.state==DEBUGGER_PAUSED){hit=1;break;}
  /* Drain the unused resampling buffers; PCM above is the native stream. */
  if(blip_samples_avail(core->getAudioChannel(core,0))>2048){int16_t b[4096];blip_read_samples(core->getAudioChannel(core,0),b,4096,0);blip_read_samples(core->getAudioChannel(core,1),b,4096,0);}
 }
}
int mg_hit(void){return hit;}
void mg_prof(int on){profile=on;if(on)memset(hist,0,sizeof hist);}
void* mg_hist(void){return hist;}
uint32_t mg_frame(void){return core->frameCounter(core);}
uint64_t mg_cycles(void){return mTimingGlobalTime(core->timing);}
uint32_t mg_reg(int i){return ((struct ARMCore*)core->cpu)->gprs[i];}
void mg_set_reg(int i,uint32_t v){char n[8];snprintf(n,8,"r%d",i);core->writeRegister(core,n,&v);}
void mg_cpsr(uint32_t v){core->writeRegister(core,"cpsr",&v);}
uint32_t mg_rd8(uint32_t a){return core->busRead8(core,a);}
uint32_t mg_rd16(uint32_t a){return core->busRead16(core,a);}
uint32_t mg_rd32(uint32_t a){return core->busRead32(core,a);}
void mg_wr8(uint32_t a,uint32_t v){core->busWrite8(core,a,v);}
void mg_wr16(uint32_t a,uint32_t v){core->busWrite16(core,a,v);}
void mg_wr32(uint32_t a,uint32_t v){core->busWrite32(core,a,v);}
void mg_mem_read(uint32_t a,uint8_t* dst,uint32_t n){for(uint32_t i=0;i<n;i++)dst[i]=core->busRead8(core,a+i);}
void mg_mem_write(uint32_t a,const uint8_t* src,uint32_t n){for(uint32_t i=0;i<n;i++)core->busWrite8(core,a+i,src[i]);}
void* mg_pixels(void){return pixels;}
int mg_save(const char* path){size_t n=core->stateSize(core);void* b=malloc(n);int ok=core->saveState(core,b);FILE*f=fopen(path,"wb");if(!f)ok=0;else{ok=ok&&fwrite(b,1,n,f)==n;fclose(f);}free(b);return ok;}
int mg_restore(const char* path){size_t n=core->stateSize(core);void*b=malloc(n);FILE*f=fopen(path,"rb");int ok=f&&fread(b,1,n,f)==n;if(f)fclose(f);if(ok)ok=core->loadState(core,b);free(b);return ok;}
int mg_pcm(const char* path){if(pcm)fclose(pcm);pcm=path&&*path?fopen(path,"wb"):NULL;return pcm!=NULL;}

/* Test-only view of the upstream core's FIFO queue and live DMA pointer. */
uint32_t mg_fifo_state(int index) {
 struct GBA* g=core->board;struct GBAAudioFIFO* f=&g->audio.chA;
 switch(index) {
 case 0:return f->fifoRead;case 1:return f->fifoWrite;
 case 2:return f->internalRemaining;case 3:return f->internalSample;
 case 4:return g->memory.dma[1].nextSource;
 case 5:return g->memory.dma[1].source;
 default:return index>=6 && index<14 ? f->fifo[index-6] : 0;
 }
}
