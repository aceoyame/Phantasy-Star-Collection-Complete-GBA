#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <vector>
#include "ps4gba/chip_audio.h"
int main(int argc,char**argv){
 if(argc!=5){std::fprintf(stderr,"usage: %s IDHEX FRAMES RATE OUT.raw\n",argv[0]);return 2;}
 unsigned id=std::strtoul(argv[1],nullptr,16); unsigned frames=std::strtoul(argv[2],nullptr,10); unsigned rate=std::strtoul(argv[3],nullptr,10);
 ps4gba::chip_audio::set_fm_backend(ps4gba::chip_audio::fm_backend::four_operator_reference);
 if(!ps4gba::chip_audio::play((uint8_t)id,256,id>=0xF8)){std::fprintf(stderr,"play failed %02X\n",id);return 3;}
 FILE*f=std::fopen(argv[4],"wb"); if(!f)return 4;
 uint64_t prior=0;
 for(unsigned fr=0;fr<frames;fr++){
   uint64_t next=((uint64_t)(fr+1)*rate+30)/60; unsigned n=(unsigned)(next-prior); prior=next;
   std::vector<int8_t>L(n),R(n); ps4gba::chip_audio::mix_into_output(L.data(),R.data(),(int)n);
   for(unsigned i=0;i<n;i++){int v=(int(L[i])+int(R[i]))/2; int8_t m=(int8_t)v; std::fwrite(&m,1,1,f);} 
 }
 std::fclose(f); return 0;
}
