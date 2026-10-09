#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <vector>
#include <fstream>
#include "ps4gba/chip_audio.h"

static std::vector<uint8_t> data;
extern "C" void ps4_vgm_ym_write(unsigned port,unsigned reg,unsigned value){
    data.push_back(port ? 0x53 : 0x52); data.push_back(uint8_t(reg)); data.push_back(uint8_t(value));
}
extern "C" void ps4_vgm_psg_write(unsigned value){ data.push_back(0x50); data.push_back(uint8_t(value)); }
static void put32(std::vector<uint8_t>&h,size_t p,uint32_t v){for(int i=0;i<4;i++)h[p+i]=uint8_t(v>>(8*i));}
int main(int argc,char**argv){
    if(argc!=4){std::fprintf(stderr,"usage: %s IDHEX FRAMES OUT.vgm\n",argv[0]);return 2;}
    unsigned id=std::strtoul(argv[1],nullptr,16), frames=std::strtoul(argv[2],nullptr,10);
    ps4gba::chip_audio::set_fm_backend(ps4gba::chip_audio::fm_backend::four_operator_reference);
    if(!ps4gba::chip_audio::play(uint8_t(id),256,id>=0xF8)){std::fprintf(stderr,"play failed %02X\n",id);return 3;}
    int8_t l=0,r=0;
    for(unsigned f=0; f<frames; ++f){
        ps4gba::chip_audio::mix_into_output(&l,&r,1);
        data.push_back(0x62); // 735 samples at 44.1 kHz = one NTSC 60 Hz source frame
    }
    data.push_back(0x66);
    std::vector<uint8_t> h(0x40,0);
    h[0]='V';h[1]='g';h[2]='m';h[3]=' ';
    put32(h,0x08,0x00000150); put32(h,0x0C,3579545u); put32(h,0x18,frames*735u);
    put32(h,0x2C,7670454u); put32(h,0x34,0x0Cu);
    uint32_t total=uint32_t(h.size()+data.size()); put32(h,0x04,total-4u);
    std::ofstream out(argv[3],std::ios::binary); out.write((char*)h.data(),h.size()); out.write((char*)data.data(),data.size());
    return out ? 0 : 4;
}
