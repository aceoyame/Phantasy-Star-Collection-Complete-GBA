#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "ps4_audio_runtime.h"
volatile uint16_t hw_reg16[0x200]; volatile uint32_t hw_reg32[0x100];
void ps4_audio_irq_entry(void){} void ps4_irq_cpu_enable(void){}
void ps4_audio_irq_service(unsigned);
const int8_t* host_mix_buffer(void); unsigned host_mix_buffer_size(void); const int8_t* host_silence(void);
typedef struct { uint32_t vblanks, productions, underruns, late_commits, waitvb_calls, commands; uint16_t max_offset_samples, max_produce_ticks, last_produce_ticks, pad; } diag_t;
extern diag_t ps4_audio_diag;
#define SPF 528u
#define MAXF 8000
static int8_t rlog[MAXF][224]; static unsigned rn;
void __real_ps4_audio_fallback_render_224(int8_t*);
void __wrap_ps4_audio_fallback_render_224(int8_t* o){__real_ps4_audio_fallback_render_224(o);if(rn<MAXF)memcpy(rlog[rn],o,224);rn++;}
static uint64_t t; static const int8_t* src; static unsigned long off; static long garbage; static int8_t* out; static unsigned long on;
static void settm3(void){hw_reg16[0x10C>>1]=(uint16_t)((t*532u)/64u);hw_reg16[6>>1]=(uint16_t)((160u+(unsigned)((t%SPF)*228u/SPF))%228u);}
static void play_until(uint64_t e){const int8_t* mb=host_mix_buffer();unsigned mbs=host_mix_buffer_size();const int8_t* sl=host_silence();
  while(t<e){int8_t v=0;if(src){const int8_t* p=src+off;if((p>=mb&&p<mb+mbs)||(p>=sl&&p<sl+SPF*3))v=*p;else{garbage++;v=0x55;}off++;}out[on++]=v;t++;}}
static void irq(unsigned m){settm3();hw_reg16[0xC6>>1]=0xFFFF;ps4_audio_irq_service(m);
  if(hw_reg16[0xC6>>1]==0xB640u){src=(const int8_t*)(uintptr_t)hw_reg32[0xBC>>2];off=0;}}
static void resamp(const int8_t* f,int8_t* o){unsigned si=0,acc=0;for(unsigned i=0;i<SPF;i++){o[i]=f[si];acc+=224;if(acc>=SPF){acc-=SPF;if(si<223)++si;}}}
int main(int argc,char** argv){
  unsigned music=argc>1?(unsigned)strtoul(argv[1],0,16):0x95u; double jit=argc>2?atof(argv[2]):0.0; int sfx=argc>3?atoi(argv[3]):0; unsigned frames=argc>4?atoi(argv[4]):2000;
  out=malloc(SPF*(MAXF+10)); srand(99);
  ps4_audio_init(); ps4_audio_play((uint8_t)music);
  uint64_t start=0;
  for(unsigned f=0;f<frames;f++){
    uint64_t edge=(uint64_t)(f+1)*SPF; uint64_t lat=jit<0?(uint64_t)(-jit*SPF):(uint64_t)(jit*SPF*((double)rand()/RAND_MAX)); if(getenv("DBG")&&f>5&&f<9){fprintf(stderr,"f=%u lat=%llu\n",f,(unsigned long long)lat);}
    uint64_t line0=edge+(SPF*68u)/228u;
    /* main loop activity: commands at random points of the frame (SFX storm if sfx) */
    if(sfx&&(rand()%6==0)){play_until(edge-SPF/2); ps4_audio_sound_index((uint8_t)(0xB5+rand()%60));}
    if(f==5)start=edge;
    play_until(edge+lat); irq(1u); if(getenv("DBG")&&f>5&&f<9)fprintf(stderr,"  commit at t=%llu src_off_from_mb=%ld\n",(unsigned long long)t,(long)(src-host_mix_buffer()));            /* VBlank commit (possibly delayed by DMA3) */
    play_until(line0+lat); irq(4u);           /* production at VCount line 0 */
    ps4_audio_prepare_frame(); ps4_audio_vblank_commit(); ps4_audio_late_vblank(); /* must be no-ops */
  }
  /* continuity vs ideal concatenated music (music-only runs) */
  long mism=0,slips=0; if(!sfx){ int8_t* ideal=malloc(SPF*(rn+2)); for(unsigned k=0;k<rn&&k<MAXF;k++)resamp(rlog[k],ideal+k*SPF);
    unsigned long j=0,i=start; /* find alignment */ for(j=0;j<SPF*4;j++){if(!memcmp(out+i,ideal+j,64))break;}
    for(;i<on&&j<(unsigned long)rn*SPF;i++,j++){ if(out[i]!=ideal[j]){ int ok=0; for(int d=-6;d<=6&&!ok;d++){ if(d&&(long)j+d>=0&&!memcmp(out+i,ideal+j+d,16)){j+=d;slips++;ok=1;} } if(!ok)mism++; } } }
  double tempo=(double)rn/(double)frames;
  printf("IRQ music %02X jitter<=%.2f sfx=%d: vblanks=%u productions=%u underruns=%u late=%u max_off=%u garbage=%ld tempo=%.3f slips=%ld mismatched_samples=%ld\n",
    music,jit,sfx,ps4_audio_diag.vblanks,ps4_audio_diag.productions,ps4_audio_diag.underruns,ps4_audio_diag.late_commits,ps4_audio_diag.max_offset_samples,garbage,tempo,slips,mism);
  return 0;}
