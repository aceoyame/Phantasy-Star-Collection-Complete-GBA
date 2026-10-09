#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "ps4_audio_runtime.h"
void ps4_audio_prepare_guard_frame(void);
volatile uint16_t hw_reg16[0x200]; volatile uint32_t hw_reg32[0x100];
#define SPF 528u
static int8_t rlog[4000][224]; static unsigned rn;
void __real_ps4_audio_fallback_render_224(int8_t*);
void __wrap_ps4_audio_fallback_render_224(int8_t* o){__real_ps4_audio_fallback_render_224(o);memcpy(rlog[rn++],o,224);}
static uint32_t tick; static const int8_t* src; static unsigned off; static long overrun;
static int8_t out[SPF*4000]; static unsigned on;
static void st(void){hw_reg16[0x10C>>1]=(uint16_t)tick;}
static void play_frame(void){tick+=4389u;st();for(unsigned i=0;i<SPF;i++){int8_t v=0;if(src){if(off<SPF*3u)v=src[off];else overrun++;off++;}out[on++]=v;}}
static void commit(void){hw_reg32[0xBC>>2]=0;hw_reg16[0xC6>>1]=0;ps4_audio_vblank_commit();if(hw_reg16[0xC6>>1]==0xB640u){src=(const int8_t*)(uintptr_t)hw_reg32[0xBC>>2];off=0;}}
static void waitvb(int blank){st();ps4_audio_prepare_frame();if(blank)ps4_audio_late_vblank();play_frame();commit();}
static void resamp(const int8_t* f,int8_t* o){unsigned si=0,acc=0;for(unsigned i=0;i<SPF;i++){o[i]=f[si];acc+=224;if(acc>=SPF){acc-=SPF;if(si<223)++si;}}}
/* every played frame in [from,to) must equal the next consecutive rendered frame */
static void exact(const char* tag,unsigned from,unsigned to,unsigned r0,unsigned r1){
  int8_t e[SPF]; long j=-1; unsigned ok=0,bad=0,rep=0,skip=0;
  for(unsigned f=from;f<to;f+=SPF){
    long hit=-1; if(j>=0&&(unsigned)j+1<r1){resamp(rlog[j+1],e);if(!memcmp(out+f,e,SPF))hit=j+1;}
    if(hit<0)for(unsigned k=(j<0?r0:(unsigned)j);k<r1&&k<(j<0?r0+6:(unsigned)j+4);k++){resamp(rlog[k],e);if(!memcmp(out+f,e,SPF)){hit=k;break;}}
    if(hit<0){bad++;continue;}
    if(j>=0){ if(hit==j) rep++; else if(hit>j+1) skip++; }
    if(hit==j+1||j<0) ok++; j=hit;
  }
  printf("%-40s frames=%u in-order=%u repeats=%u skips=%u unmatched=%u\n",tag,(to-from)/SPF,ok,rep,skip,bad);
}
int main(void){
  st(); ps4_audio_init();
  ps4_audio_play(0x84); unsigned r0=rn,a=on; for(int f=0;f<300;f++) waitvb(0);
  exact("A steady $84",a+SPF,on,r0,rn);
  unsigned b=on,rb=rn>2?rn-2:0;
  for(int r=0;r<40;r++){ st(); ps4_audio_prepare_guard_frame(); play_frame(); waitvb(1); waitvb(0); }
  exact("B guard look-ahead + missed VBlank",b,on,rb,rn);
  unsigned c=on,rc=rn,played=0;
  for(int r=0;r<60;r++){ st(); ps4_audio_prepare_frame(); if(r%3==0) ps4_audio_play(0xF2); waitvb(0); played++; }
  printf("%-40s played=%u sequencer_advances=%u (look-ahead cache holds <=2)\n","C SFX invalidations",played,rn-rc);
  unsigned e=on,re=rn; for(int f=0;f<30;f++) waitvb(0); exact("C' steady after SFX",e+SPF,on,re>2?re-2:0,rn);
  play_frame(); ps4_audio_play(0x87); unsigned rd=rn,d=on; for(int f=0;f<200;f++) waitvb(0);
  exact("D shop $87 after switch",d+SPF,on,rd,rn);
  /* E: one-shot jingle ($8B victory) ends, reports no music, and can be replayed */
  ps4_audio_play(0x8B); int end_f=-1; for(int f=0;f<240;f++){ waitvb(0); if(end_f<0&&ps4_audio_current_music()==0) end_f=f; }
  ps4_audio_play(0x8B); int replay=ps4_audio_current_music()==0x8B;
  printf("E one-shot $8B ended_at_frame=%d replay_ok=%d\n",end_f,replay);
  printf("overrun samples past guards: %ld\n",overrun); return 0; }
