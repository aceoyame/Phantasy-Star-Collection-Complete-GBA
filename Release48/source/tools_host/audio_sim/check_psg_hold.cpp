#include "../../gba/audio_p134r/src/chip_audio.cpp"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <fstream>
#include <iterator>
#include <cassert>
#include <cmath>

// Independent translation of original DoPSGNoteOn/VolEnv_Hold ($D0ED0+).
struct oracle {
    std::vector<unsigned char> data;
    unsigned cursor=0, attenuation=0;
    bool keyed=true;
    void step() {
        for(unsigned safety=0;safety<8;++safety) {
            assert(cursor<data.size());
            unsigned value=data[cursor++];
            if(value<0x80) { attenuation=value; return; }
            if(value==0x81) { cursor-=2; return; }
            if(value==0x83) { cursor-=2; keyed=false; return; }
            if(value==0x80) { cursor=0; continue; }
            if(value==0x82) { cursor=data[cursor]; continue; }
            std::abort();
        }
        std::abort();
    }
};
int main(int argc,char**argv) {
    using namespace ps4gba::chip_audio;
    assert(argc==2 || argc==3);
    std::ifstream f(argv[1],std::ios::binary);
    std::vector<unsigned char> rom((std::istreambuf_iterator<char>(f)),{});
    auto be32=[&](unsigned p) { return unsigned(rom.at(p))<<24|unsigned(rom.at(p+1))<<16|unsigned(rom.at(p+2))<<8|rom.at(p+3); };
    unsigned checks=0, mismatches=0, holds=0;
    const unsigned ptrs=be32(0xd1a74);
    for(unsigned env=1;env<=10;++env) {
        unsigned length=0;auto data=ps4gba::chip_audio_data::psg_envelope_data(env,length);
        unsigned start=be32(ptrs+(env-1)*4);
        oracle expected;
        for(unsigned j=0;j<length;++j) { assert(data[j]==rom.at(start+j));expected.data.push_back(rom.at(start+j)); }
        lane_state actual;actual.active=true;actual.kind=ps4gba::chip_audio_data::lane_kind::psg;
        actual.keyed=true;actual.current_note=30;actual.psg_envelope=env;
        for(unsigned frame=0;frame<128;++frame) {
            if(expected.keyed) expected.step();
            if(frame) advance_psg_envelope(actual);
            if(expected.keyed!=actual.keyed || (actual.keyed && unsigned(psg_envelope_delta(actual))!=expected.attenuation)) {
                if(mismatches<5) std::fprintf(stderr,"MISMATCH env=%u frame=%u expected=%u actual=%d keyed=%u/%u\n",env,frame,expected.attenuation,psg_envelope_delta(actual),expected.keyed,actual.keyed);
                ++mismatches;
            }
            if(actual.keyed && data[length-1]==0x81 && actual.psg_envelope_pos>=length-2) ++holds;
            ++checks;
        }
    }
    unsigned song_holds=0, zone_missing=0, max_scale=0;
    // Actual bytecode, instrument bank and production mixer flags for all songs.
    for(unsigned id=0x81;id<=0xb4;++id) {
        assert(play(id,224,true));set_mute_mask(0);
        int8_t out[224];unsigned hits=0,peak=0,limited=0;uint32_t crc=0xffffffffu;double energy=0;
        std::ofstream preview;
        if(argc==3 && (id==0x84 || id==0x8a || id==0x95 || id==0xac)) {
            char name[24];std::snprintf(name,sizeof(name),"music_%02X.s8",id);
            preview.open(std::string(argv[2])+"/"+name,std::ios::binary);
        }
        for(unsigned frame=0;frame<1800;++frame) {
            mix_psc_mono(out,224);
            if(preview.is_open()) preview.write(reinterpret_cast<char*>(out),sizeof(out));
            for(int value:out) { unsigned magnitude=unsigned(std::abs(value));if(magnitude>peak)peak=magnitude;if(magnitude>=125)++limited;energy+=value*value; }
            for(unsigned char byte:out) {
                crc^=byte;for(unsigned bit=0;bit<8;++bit) crc=(crc>>1)^((crc&1)?0xedb88320u:0);
            }
            for(unsigned i=0;i<current_song->lane_count;++i) {
                const auto& lane=lanes[i];
                if(lane.active && lane.keyed && lane.current_note>=0 && lane.kind==ps4gba::chip_audio_data::lane_kind::fm && !lane.fm_zone) ++zone_missing;
                if(lane.fm_prev_scale_q13>int(max_scale)) max_scale=unsigned(lane.fm_prev_scale_q13);
                if(lane.active && lane.keyed && lane.kind==ps4gba::chip_audio_data::lane_kind::psg && (lane.psg_envelope==9 || lane.psg_envelope==10)) {
                    unsigned n=0;auto data=ps4gba::chip_audio_data::psg_envelope_data(lane.psg_envelope,n);
                    if(data[n-1]==0x81 && lane.psg_envelope_pos>=n-2) {
                        ++hits;
                        if(psg_envelope_delta(lane)!=4) ++mismatches;
                    }
                }
            }
        }
        song_holds+=hits;
        std::printf("{\"song\":%u,\"frames\":1800,\"hold_lane_frames\":%u,\"pcm_crc32\":\"%08x\",\"RMS\":%.4f,\"peak\":%u,\"limiter_top_fraction\":%.8f}\n",id,hits,crc^0xffffffffu,std::sqrt(energy/(1800*224)),peak,double(limited)/(1800*224));
    }
    std::printf("{\"envelope_frame_checks\":%u,\"fixture_hold_frames\":%u,\"song_hold_lane_frames\":%u,\"missing_FM_zones\":%u,\"max_FM_scale\":%u,\"mismatches\":%u}\n",checks,holds,song_holds,zone_missing,max_scale,mismatches);
    return mismatches || zone_missing ? 1:0;
}
