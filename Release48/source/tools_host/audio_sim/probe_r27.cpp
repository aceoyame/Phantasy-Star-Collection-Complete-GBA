// Include the mixer so the focused regression can exercise note and morph
// state directly without shipping a diagnostic API in the cartridge.
#include "../../gba/audio_p134r/src/chip_audio.cpp"
#include <cstdio>
#include <cstdlib>

int main()
{
    using namespace ps4gba::chip_audio;
    if(!play(0x95,224,true)) return 2;
    int8_t frame[224];
    mix_psc_mono(frame,224);
    lane_state* test = nullptr;
    for(auto& lane : lanes)
        if(lane.kind == ps4gba::chip_audio_data::lane_kind::fm && lane.fm_zone && lane.current_note >= 0) { test=&lane; break; }
    if(!test) return 3;
    test->fm_prev_scale_q13 = 1234;
    test->fm_prev_note = static_cast<signed char>(test->current_note);
    key_on_fm(*test);
    if(test->fm_prev_scale_q13 != -1 || !test->fm_edge_pending) return 4;
    for(unsigned stage=0;stage<5;++stage)
        for(unsigned b=0;b<65535;b+=97)
        {
            const unsigned expected=(65535-b+fm_stage_step[stage]-1)/fm_stage_step[stage];
            if(fm_stage_segment(stage,b,224)!=(expected<224 ? expected:224)) return 5;
        }
    // Exercise an actual morph crossing eight samples into a source frame.
    test->wait=200; test->note_stop_counter=0; test->modulation_enabled=false;
    test->fm_timbre_stage=0; test->fm_timbre_blend=65535-8*140;
    mix_psc_mono(frame,224);
    if(test->fm_timbre_stage!=1 || test->fm_timbre_blend!=216*89) return 6;
    std::puts("PASS same-pitch attack reset and sample-exact snapshot boundary");
    const unsigned ids[]={0x84,0x8F,0x95};
    for(unsigned id:ids)
    {
        if(!play(id,224,true)) return 7;
        long long energy=0; unsigned peak=0, live=0, missing=0;
        for(unsigned f=0;f<720;++f)
        {
            mix_psc_mono(frame,224);
            for(int v:frame) { energy+=v*v; unsigned a=unsigned(std::abs(v)); if(a>peak) peak=a; }
            for(auto& lane:lanes)
                if(lane.active && lane.keyed && lane.kind==ps4gba::chip_audio_data::lane_kind::fm)
                { ++live; if(!lane.fm_zone) ++missing; }
        }
        if(!energy || missing) return 8;
        std::printf("PASS $%02X 720 frames energy=%lld peak=%u live_FM=%u missing_zones=%u\n",id,energy,peak,live,missing);
    }
}
