#include "ps4gba/chip_audio.h"
#include "ps4gba/ps4_chip_audio_data.h"
#include "ps4gba/ps4_fm_multisample_data.h"
#include <cstdio>
#include <cstdint>
#include <cstdlib>

int main() {
    using namespace ps4gba;
    if(fm_multisample_data::wave_points()!=256)return 2;
    const unsigned ids[]={0x84,0x8A,0x95,0x9F};
    chip_audio::set_fm_backend(chip_audio::fm_backend::multisample);
    for(unsigned id:ids){
        if(!chip_audio::play(id,224,true))return 3;
        int8_t frame[224];long long energy=0;unsigned peak=0;
        for(unsigned f=0;f<240;++f){
            chip_audio::mix_psc_mono(frame,224);
            for(int x:frame){energy+=x*x;unsigned a=unsigned(std::abs(x));if(a>peak)peak=a;}
        }
        if(!energy)return 4;
        std::printf("PASS source music $%02X: 240 frames, energy=%lld peak=%u\n",id,energy,peak);
    }
    // Compare all FM increments with an independent, integer source-clock
    // expression. The data generator uses floating-point rounding.
    const unsigned fnum[]={606,644,683,723,766,813,860,911,965,1023,1084,1148};
    for(unsigned n=0;n<128;n++){
        uint64_t numerator=uint64_t(fnum[n%12])*(1u<<((n/12)&7))*7670454u*2048u;
        uint64_t denominator=144u*13379u;
        unsigned expected=unsigned((numerator+denominator/2)/denominator);
        if(chip_audio_data::fm_phase_increment(n)!=expected)return 5;
    }
    std::puts("PASS all 128 FM source-clock increments; 256-point bank renders safely");
}
