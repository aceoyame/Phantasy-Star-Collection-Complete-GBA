#include <cstdio>
#include <cstdint>
#include <cstdlib>
extern "C" int ps4_audio_fallback_play(uint8_t); extern "C" void ps4_audio_fallback_render_224(int8_t*);
extern "C" unsigned prof_fm, prof_psg, prof_noise, prof_dac;
int main(int c,char**v){ unsigned id=strtoul(v[1],0,16), n=atoi(v[2]); ps4_audio_fallback_play(id);
  FILE*f=fopen(v[3],"wb"); FILE*g=fopen(v[4],"wb"); int8_t b[224];
  for(unsigned i=0;i<n;i++){unsigned a=prof_fm,p=prof_psg,q=prof_noise,d=prof_dac; ps4_audio_fallback_render_224(b); fwrite(b,1,224,f);
    uint8_t L[4]={(uint8_t)(prof_fm-a),(uint8_t)(prof_psg-p),(uint8_t)(prof_noise-q),(uint8_t)(prof_dac-d)}; fwrite(L,1,4,g);} fclose(f); fclose(g); }
