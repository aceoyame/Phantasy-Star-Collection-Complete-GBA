#include "TRACE_CHIP_PATH"
#include <cstdio>
#include <cstdlib>
#include <fstream>

unsigned trace_frame = 0, trace_song = 0;
void trace_note(unsigned lane, unsigned pc, unsigned kind, unsigned event,
                unsigned duration, int note, unsigned keyed, unsigned held,
                unsigned instrument, unsigned wait) {
    std::printf("%u\t%u\t%u\t%u\t%u\t%u\t%u\t%d\t%u\t%u\t%u\t%u\n",
        trace_song, trace_frame, lane, pc, kind, event, duration, note,
        keyed, held, instrument, wait);
}
int main(int argc, char** argv) {
    using namespace ps4gba::chip_audio;
    if(argc != 4) return 2;
    unsigned first = std::strtoul(argv[1], nullptr, 0);
    unsigned last = first ? first : 0xb4;
    if(!first) first = 0x81;
    unsigned frames = std::strtoul(argv[2], nullptr, 0);
    for(trace_song = first; trace_song <= last; ++trace_song) {
        if(!play(trace_song,224,true)) return 3;
        set_mute_mask(0);
        std::ofstream pcm;
        if(trace_song == 0xac && *argv[3]) pcm.open(argv[3],std::ios::binary);
        for(trace_frame=0; trace_frame<frames; ++trace_frame) {
            int8_t out[224]; mix_psc_mono(out,224);
            if(pcm.is_open()) pcm.write(reinterpret_cast<char*>(out),sizeof out);
        }
        if(std::fflush(stdout) || std::ferror(stdout)) return 4;
        std::fprintf(stderr,"Traced $%02X, %u source frames\n",trace_song,frames);
    }
}
