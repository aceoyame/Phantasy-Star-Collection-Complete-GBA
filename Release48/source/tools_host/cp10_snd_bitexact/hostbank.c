/* host: the sound bank lives in the ROM image at its GBA address; snd_data0 is the base so that (snd_data0 + 0x08xxxxxx) hits the ROM file */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
const unsigned char *snd_data0;
__attribute__((constructor)) static void init(void){
  const char*p=getenv("SND_ROM"); if(!p){ fprintf(stderr,"set SND_ROM\n"); exit(2); }
  FILE*f=fopen(p,"rb"); if(!f){ perror(p); exit(2); }
  unsigned char*b=malloc(0x2000000); if(fread(b,1,0x2000000,f)!=0x2000000) exit(2); fclose(f);
  snd_data0=(const unsigned char*)((uintptr_t)b-0x08000000u);
}
