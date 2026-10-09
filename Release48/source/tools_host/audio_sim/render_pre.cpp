#include <cstdio>
#include <cstdint>
#include <cstdlib>
extern "C" int ps4_audio_fallback_play(uint8_t); extern "C" void ps4_audio_fallback_render_224(int8_t*); extern "C" int prof_pre[224];
int main(int c,char**v){ unsigned id=strtoul(v[1],0,16), n=atoi(v[2]); ps4_audio_fallback_play(id); FILE*f=fopen(v[3],"wb"); int8_t b[224];
  for(unsigned i=0;i<n;i++){ ps4_audio_fallback_render_224(b); fwrite(prof_pre,4,224,f);} fclose(f); }
