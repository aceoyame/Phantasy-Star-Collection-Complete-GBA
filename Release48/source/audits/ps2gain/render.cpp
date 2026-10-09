// Render one YM2612 channel (ch1) with a 26-byte UE-order PSII patch.
// usage: render <hex26> <fnum> <block> <samples_out_native> <keyoff_sample> > out.s16
#include "ymfm_opn.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
struct iface : ymfm::ymfm_interface {};
static const uint8_t REGS[26]={0xB0,0xB4,0x40,0x48,0x44,0x4C,0x30,0x38,0x34,0x3C,0x50,0x58,0x54,0x5C,0x60,0x68,0x64,0x6C,0x70,0x78,0x74,0x7C,0x80,0x88,0x84,0x8C};
int main(int argc,char**argv){
  if(argc<6) return 2;
  uint8_t p[26]; for(int i=0;i<26;i++){unsigned v; sscanf(argv[1]+2*i,"%2x",&v); p[i]=v;}
  int fnum=atoi(argv[2]), blk=atoi(argv[3]); long n=atol(argv[4]), koff=atol(argv[5]);
  iface itf; ymfm::ym2612 chip(itf); chip.reset();
  auto w=[&](uint8_t a,uint8_t d){ chip.write_address(a); chip.write_data(d); ymfm::ym2612::output_data o; chip.generate(&o,1); };
  w(0x22,0); w(0x27,0); w(0x2B,0); w(0x28,0);
  for(int i=0;i<26;i++){ uint8_t v=p[i]; if(REGS[i]==0xB4) v=(v&0x3F)|0xC0; w(REGS[i],v); }
  w(0xA4,(blk<<3)|(fnum>>8)); w(0xA0,fnum&255);
  w(0x28,0xF0);
  ymfm::ym2612::output_data o;
  for(long i=0;i<n;i++){ if(i==koff) w(0x28,0x00); chip.generate(&o,1); int16_t s=(int16_t)o.data[0]; fwrite(&s,2,1,stdout); }
  return 0;
}
