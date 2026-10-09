/* Lightweight YM2413/OPLL renderer for PSC8's ARM7 budget.
 * Register/tone semantics follow the SMS FM unit; synthesis is intentionally
 * lower cost than the bit-exact emu2413 reference retained in vendor/.
 */
#include <stdint.h>
#include "fast_opll.h"
#define HOT __attribute__((section(".hot")))
__attribute__((section(".hotdata.fmtab"))) const int8_t fastopll_sine256[256] = {0,3,6,9,12,16,19,22,25,28,31,34,37,40,43,46,49,51,54,57,60,63,65,68,71,73,76,78,81,83,85,88,90,92,94,96,98,100,102,104,106,107,109,111,112,113,115,116,117,118,120,121,122,122,123,124,125,125,126,126,126,127,127,127,127,127,127,127,126,126,126,125,125,124,123,122,122,121,120,118,117,116,115,113,112,111,109,107,106,104,102,100,98,96,94,92,90,88,85,83,81,78,76,73,71,68,65,63,60,57,54,51,49,46,43,40,37,34,31,28,25,22,19,16,12,9,6,3,0,-3,-6,-9,-12,-16,-19,-22,-25,-28,-31,-34,-37,-40,-43,-46,-49,-51,-54,-57,-60,-63,-65,-68,-71,-73,-76,-78,-81,-83,-85,-88,-90,-92,-94,-96,-98,-100,-102,-104,-106,-107,-109,-111,-112,-113,-115,-116,-117,-118,-120,-121,-122,-122,-123,-124,-125,-125,-126,-126,-126,-127,-127,-127,-127,-127,-127,-127,-126,-126,-126,-125,-125,-124,-123,-122,-122,-121,-120,-118,-117,-116,-115,-113,-112,-111,-109,-107,-106,-104,-102,-100,-98,-96,-94,-92,-90,-88,-85,-83,-81,-78,-76,-73,-71,-68,-65,-63,-60,-57,-54,-51,-49,-46,-43,-40,-37,-34,-31,-28,-25,-22,-19,-16,-12,-9,-6,-3};
__attribute__((section(".hotdata.fmtab"))) const uint8_t fastopll_patches[15][8] = {
 {0x71,0x61,0x1e,0x17,0xd0,0x78,0x00,0x17},{0x13,0x41,0x1a,0x0d,0xd8,0xf7,0x23,0x13},
 {0x13,0x01,0x99,0x00,0xf2,0xc4,0x21,0x23},{0x11,0x61,0x0e,0x07,0x8d,0x64,0x70,0x27},
 {0x32,0x21,0x1e,0x06,0xe1,0x76,0x01,0x28},{0x31,0x22,0x16,0x05,0xe0,0x71,0x00,0x18},
 {0x21,0x61,0x1d,0x07,0x82,0x81,0x11,0x07},{0x33,0x21,0x2d,0x13,0xb0,0x70,0x00,0x07},
 {0x61,0x61,0x1b,0x06,0x64,0x65,0x10,0x17},{0x41,0x61,0x0b,0x18,0x85,0xf0,0x81,0x07},
 {0x33,0x01,0x83,0x11,0xea,0xef,0x10,0x04},{0x17,0xc1,0x24,0x07,0xf8,0xf8,0x22,0x12},
 {0x61,0x50,0x0c,0x05,0xd2,0xf5,0x40,0x42},{0x01,0x01,0x55,0x03,0xe9,0x90,0x03,0x02},
 {0x41,0x41,0x89,0x03,0xf1,0xe4,0xc0,0x13}
};
__attribute__((section(".hotdata.fmtab"))) const uint8_t fastopll_mul2[16]={1,2,4,6,8,10,12,14,16,18,20,20,24,24,30,30};
__attribute__((section(".hotdata.fmtab"))) const uint16_t fastopll_attack_step[16]={0,8,12,16,24,32,48,64,96,128,192,256,384,768,2048,8192};
__attribute__((section(".hotdata.fmtab"))) const uint16_t fastopll_decay_step[16]={0,1,1,2,2,3,4,6,8,12,16,24,32,48,96,192};
static inline const uint8_t *patch(const FastOPLL *o,uint8_t inst){return inst?fastopll_patches[(inst-1)%15]:o->reg;}
static inline int8_t wave(uint32_t ph,uint8_t half){int8_t s=fastopll_sine256[(uint8_t)(ph>>24)];return half&&s<0?0:s;}
static inline uint16_t sustain(uint8_t sl){return (uint16_t)(((uint32_t)(15u-sl)*65535u)/15u);}
static inline uint16_t envelope(uint16_t env,uint8_t *st,uint8_t key,uint8_t ar,uint8_t dr,uint8_t sl,uint8_t rr){
 uint16_t sus=sustain(sl);
 if(key){
  if(*st==0){uint32_t e=(uint32_t)env+fastopll_attack_step[ar];if(ar==15||e>=65535){env=65535;*st=1;}else env=(uint16_t)e;}
  else if(*st==1){uint16_t d=fastopll_decay_step[dr];if(env>sus+d)env-=d;else{env=sus;*st=2;}}
 }else if(env){uint16_t d=fastopll_decay_step[rr?rr:5];env=env>d?env-d:0;}
 return env;
}
void fastopll_reset(FastOPLL *o){uint8_t *p=(uint8_t*)o;for(unsigned i=0;i<sizeof(*o);i++)p[i]=0;o->lfsr=0x1ace1u;o->cdirty=0x1ff;}
void fastopll_addr(FastOPLL *o,uint8_t a){o->adr=a&63;}
void fastopll_data(FastOPLL *o,uint8_t v){
 unsigned a=o->adr;o->reg[a]=v;
 if(a<8)o->cdirty=0x1ff;                                         /* custom patch: every channel that uses instrument 0 */
 else if((a>>4)-1u<3u&&(a&15u)<9u)o->cdirty|=(uint16_t)(1u<<(a&15u));   /* F-number / block+key / instrument+volume of one channel */
}
int fastopll_active(const FastOPLL *o){
 for(unsigned ch=0;ch<9;ch++)if((o->reg[0x20+ch]&0x10)||o->env[ch])return 1;
 if(o->reg[0x0e]&0x20)for(unsigned d=0;d<5;d++)if(o->renv[d])return 1;
 return (o->reg[0x0e]&0x20)&&(o->reg[0x0e]&0x1f)!=(o->rkey&0x1f);
}
int fastopll_calc(FastOPLL *o){
 int mix=0;uint8_t rhythm=o->reg[0x0e]&0x20;unsigned channels=rhythm?6:9;
 for(unsigned ch=0;ch<channels;ch++){
  uint8_t r20=o->reg[0x20+ch];uint16_t f=(uint16_t)o->reg[0x10+ch]|((uint16_t)(r20&1)<<8);uint8_t block=(r20>>1)&7,key=(r20>>4)&1;
  uint8_t iv=o->reg[0x30+ch],inst=iv>>4,vol=iv&15;const uint8_t *p=patch(o,inst);uint16_t env=o->env[ch],menv=o->mod_env[ch];uint8_t st=o->stage[ch],mst=o->mod_stage[ch],old=o->key_prev[ch];
  if(key&&!old){env=menv=0;st=mst=0;} if(!key&&old){st=mst=3;}o->key_prev[ch]=key;
  if(!f||(!key&&!env&&!menv)){o->env[ch]=env;o->mod_env[ch]=menv;o->stage[ch]=st;o->mod_stage[ch]=mst;continue;}
  menv=envelope(menv,&mst,key,p[4]>>4,p[4]&15,p[6]>>4,p[6]&15);
  env=envelope(env,&st,key,p[5]>>4,p[5]&15,p[7]>>4,p[7]&15);
  uint32_t base=((uint32_t)f<<block)*22430u,pm=o->pm[ch]+((base*fastopll_mul2[p[0]&15])>>1),pc=o->pc[ch]+((base*fastopll_mul2[p[1]&15])>>1);int16_t prev=o->prevm[ch];uint8_t fb=p[3]&7;
  int mi=(int)(pm>>24);if(fb)mi+=(prev*(int)fb)>>6;int m=wave((uint32_t)(mi&255)<<24,(p[3]>>3)&1);m=(m*(63-(p[2]&63))*(int)(menv>>8))>>14;prev=(int16_t)m;
  int ci=(int)(pc>>24)+(m>>2);int c=wave((uint32_t)(ci&255)<<24,(p[3]>>4)&1);mix+=(c*(int)(env>>8)*(15-(int)vol))>>12;
  o->pm[ch]=pm;o->pc[ch]=pc;o->env[ch]=env;o->mod_env[ch]=menv;o->stage[ch]=st;o->mod_stage[ch]=mst;o->prevm[ch]=prev;
 }
 if(rhythm){int16_t t=0;fastopll_rhythm(o,&t,1);mix+=t;}
 mix>>=3;if(mix>127)mix=127;else if(mix<-128)mix=-128;return mix;
}

__attribute__((section(".hotdata.fmtab"))) const int8_t fastopll_halfsine256[256] = {0,3,6,9,12,16,19,22,25,28,31,34,37,40,43,46,49,51,54,57,60,63,65,68,71,73,76,78,81,83,85,88,90,92,94,96,98,100,102,104,106,107,109,111,112,113,115,116,117,118,120,121,122,122,123,124,125,125,126,126,126,127,127,127,127,127,127,127,126,126,126,125,125,124,123,122,122,121,120,118,117,116,115,113,112,111,109,107,106,104,102,100,98,96,94,92,90,88,85,83,81,78,76,73,71,68,65,63,60,57,54,51,49,46,43,40,37,34,31,28,25,22,19,16,12,9,6,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

/* Rhythm section (reg 0x0E bit5). Each drum is key-on edge triggered with an instant attack and a percussive
 * exponential decay (faster after key-off), as the OPLL rhythm patches are percussive. Volumes come from
 * regs 0x36 (BD), 0x37 (HH<<4|SD), 0x38 (TOM<<4|CYM). Frequencies: BD ch6, HH/SD ch7, TOM/CYM ch8.
 * Waveforms are cheap approximations: BD/TOM sines (TOM at the x5 modulator multiple), SD noise+tone,
 * HH/CYM metallic phase-bit squares (HH mixed with noise).
 * Envelopes step every 8 output samples (counted globally in rcnt, so block size never changes the result);
 * between steps each drum has a constant gain and is rendered drum-major. Shared by calc and render. */
#ifndef RDECAY
#define RDECAY 7,6,7,8,5
#endif
static const uint8_t rdecay[5]={RDECAY};
HOT void fastopll_rhythm(FastOPLL *o, int16_t *acc, unsigned n){
 uint8_t r=o->reg[0x0e]&0x1f, prev=o->rkey; o->rkey=r;
 uint8_t on=r&~prev;
 if(on&0x10){o->renv[0]=65535;o->pc[6]=0;}
 if(on&0x08){o->renv[1]=65535;o->pc[7]=0;}
 if(on&0x04){o->renv[2]=65535;o->pm[8]=0;}
 if(on&0x02){o->renv[3]=65535;}
 if(on&0x01){o->renv[4]=65535;}
 if(!(o->renv[0]|o->renv[1]|o->renv[2]|o->renv[3]|o->renv[4])) return;
 uint8_t q6=o->reg[0x26],q7=o->reg[0x27],q8=o->reg[0x28];
 uint32_t inc6=(((uint32_t)o->reg[0x16]|((q6&1u)<<8))<<((q6>>1)&7))*22430u;
 uint32_t inc7=(((uint32_t)o->reg[0x17]|((q7&1u)<<8))<<((q7>>1)&7))*22430u;
 uint32_t inc8=(((uint32_t)o->reg[0x18]|((q8&1u)<<8))<<((q8>>1)&7))*22430u, inc8m=inc8*5u;
 int vol[5]={15-(o->reg[0x36]&15),15-(o->reg[0x37]&15),15-(o->reg[0x38]>>4),15-(o->reg[0x38]&15),15-(o->reg[0x37]>>4)};
 const int8_t *sn=fastopll_sine256;
 unsigned i=0;
 while(i<n){
  unsigned seg=8u-o->rcnt; if(seg>n-i) seg=n-i;
  int16_t *a=acc+i;
  int g0=(o->renv[0]>>8)*vol[0], g1=(o->renv[1]>>8)*vol[1], g2=(o->renv[2]>>8)*vol[2], g3=(o->renv[3]>>8)*vol[3], g4=(o->renv[4]>>8)*vol[4];
  int8_t nzb[8];
  if(g1|g4){ uint32_t l=o->lfsr; for(unsigned j=0;j<seg;j++){ l=(l>>1)^(0u-(l&1u)&0x80200003u); nzb[j]=(l&1)?63:-63; } o->lfsr=l; }
  if(g0){ uint32_t p=o->pc[6]; for(unsigned j=0;j<seg;j++){ p+=inc6; a[j]=(int16_t)(a[j]+((sn[p>>24]*g0)>>10)); } }
  if(g1){ uint32_t p=o->pc[7]; for(unsigned j=0;j<seg;j++){ p+=inc7; a[j]=(int16_t)(a[j]+((((sn[p>>24]>>1)+nzb[j])*g1)>>12)); } }
  if(g2){ uint32_t p=o->pm[8]; for(unsigned j=0;j<seg;j++){ p+=inc8m; a[j]=(int16_t)(a[j]+((sn[p>>24]*g2)>>12)); } }
  if(g3|g4){ uint32_t p7=o->pm[7],p8=o->pc[8]; int c3=(64*g3)>>12, c4=(48*g4)>>12;
   for(unsigned j=0;j<seg;j++){ p7+=inc7; p8+=inc8; unsigned mb=((p7>>31)^(p7>>26)^(p8>>29))&1u;
    int s=mb?c3+c4:-(c3+c4); if(g4) s+=(nzb[j]*g4)>>13; a[j]=(int16_t)(a[j]+s); } }
  o->pc[6]+=inc6*seg; o->pc[7]+=inc7*seg; o->pm[7]+=inc7*seg; o->pc[8]+=inc8*seg; o->pm[8]+=inc8m*seg;
  i+=seg; o->rcnt=(uint8_t)(o->rcnt+seg);
  if(o->rcnt>=8){
   o->rcnt=0; unsigned live=0;
   for(unsigned d=0;d<5;d++){ uint32_t e=o->renv[d]; if(e){ unsigned k=rdecay[d]-((r&(0x10>>d))?0:2); uint32_t dec=(e>>k)+8u; e=e>dec?e-dec:0; o->renv[d]=(uint16_t)e; live|=e; } }
   if(!live) break;                                  /* all drums silent: state frozen */
  }
 }
}
