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
void fastopll_reset(FastOPLL *o){uint8_t *p=(uint8_t*)o;for(unsigned i=0;i<sizeof(*o);i++)p[i]=0;o->lfsr=0x1ace1u;}
void fastopll_addr(FastOPLL *o,uint8_t a){o->adr=a&63;}
void fastopll_data(FastOPLL *o,uint8_t v){o->reg[o->adr]=v;}
int fastopll_active(const FastOPLL *o){
 for(unsigned ch=0;ch<9;ch++)if((o->reg[0x20+ch]&0x10)||o->env[ch])return 1;
 return (o->reg[0x0e]&0x3f)!=0;
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
 if(rhythm){
  uint8_t r=o->reg[0x0e];uint32_t l=o->lfsr;l=(l>>1)^(0u-(l&1u)&0x80200003u);int n=(l&1)?28:-28;int s=0;if(r&1)s+=n;if(r&8)s+=n*2;if(r&2)s+=(o->pc[7]&0x8000000)?22:-22;
  if(r&0x10){uint8_t q=o->reg[0x26];uint16_t f=(uint16_t)o->reg[0x16]|((q&1)<<8);o->pc[6]+=((uint32_t)f<<((q>>1)&7))*22430u;s+=wave(o->pc[6],0)>>1;}
  if(r&0x04){uint8_t q=o->reg[0x28];uint16_t f=(uint16_t)o->reg[0x18]|((q&1)<<8);o->pc[8]+=((uint32_t)f<<((q>>1)&7))*22430u;s+=wave(o->pc[8],0)>>2;}
  o->lfsr=l;mix+=s;
 }
 mix>>=3;if(mix>127)mix=127;else if(mix<-128)mix=-128;return mix;
}

__attribute__((section(".hotdata.fmtab"))) const int8_t fastopll_halfsine256[256] = {0,3,6,9,12,16,19,22,25,28,31,34,37,40,43,46,49,51,54,57,60,63,65,68,71,73,76,78,81,83,85,88,90,92,94,96,98,100,102,104,106,107,109,111,112,113,115,116,117,118,120,121,122,122,123,124,125,125,126,126,126,127,127,127,127,127,127,127,126,126,126,125,125,124,123,122,122,121,120,118,117,116,115,113,112,111,109,107,106,104,102,100,98,96,94,92,90,88,85,83,81,78,76,73,71,68,65,63,60,57,54,51,49,46,43,40,37,34,31,28,25,22,19,16,12,9,6,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
