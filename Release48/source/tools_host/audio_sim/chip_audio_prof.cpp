extern "C" { unsigned prof_fm, prof_psg, prof_noise, prof_dac; }
#include "ps4gba/chip_audio.h"

#include "ps4gba/ps4_chip_audio_data.h"
#include "ps4gba/ps4_fm_multisample_data.h"

#include <stdint.h>

namespace ps4gba::chip_audio
{
namespace
{
    constexpr int max_lanes = 9;
    constexpr int fm_operators = 4;
    constexpr int max_stack = 8;
    constexpr int loop_slots = 8;
#if defined(MD2GBA_PS4_M4A_NATIVE_ONLY)
    constexpr bool native_multisample_target = true;
#else
    constexpr bool native_multisample_target = false;
#endif

#if defined(PS4_FALLBACK_SEQ_IWRAM)
#define PS4_M4A_SEQ_HOT __attribute__((section(".iwram.audio"),noinline))
#else
#define PS4_M4A_SEQ_HOT __attribute__((noinline))
#endif

    // Maxmod's 31 kHz GBA mode is 31536 Hz.  The sample backend is cheap
    // enough to spend the extra bandwidth; this materially reduces the
    // aliasing/"tinny" compromise seen in PSC's ~21 kHz renderer.
    constexpr unsigned int mixer_rate = 13379;
    constexpr unsigned int reference_envelope_rate = 18157;

    constexpr int8_t sine_table[256] = {
        0, 3, 6, 9, 12, 16, 19, 22, 25, 28, 31, 34, 37, 40, 43, 46,
        49, 51, 54, 57, 60, 63, 65, 68, 71, 73, 76, 78, 81, 83, 85, 88,
        90, 92, 94, 96, 98, 100, 102, 104, 106, 107, 109, 111, 112, 113, 115, 116,
        117, 118, 120, 121, 122, 122, 123, 124, 125, 125, 126, 126, 126, 127, 127, 127,
        127, 127, 127, 127, 126, 126, 126, 125, 125, 124, 123, 122, 122, 121, 120, 118,
        117, 116, 115, 113, 112, 111, 109, 107, 106, 104, 102, 100, 98, 96, 94, 92,
        90, 88, 85, 83, 81, 78, 76, 73, 71, 68, 65, 63, 60, 57, 54, 51,
        49, 46, 43, 40, 37, 34, 31, 28, 25, 22, 19, 16, 12, 9, 6, 3,
        0, -3, -6, -9, -12, -16, -19, -22, -25, -28, -31, -34, -37, -40, -43, -46,
        -49, -51, -54, -57, -60, -63, -65, -68, -71, -73, -76, -78, -81, -83, -85, -88,
        -90, -92, -94, -96, -98, -100, -102, -104, -106, -107, -109, -111, -112, -113, -115, -116,
        -117, -118, -120, -121, -122, -122, -123, -124, -125, -125, -126, -126, -126, -127, -127, -127,
        -127, -127, -127, -127, -126, -126, -126, -125, -125, -124, -123, -122, -122, -121, -120, -118,
        -117, -116, -115, -113, -112, -111, -109, -107, -106, -104, -102, -100, -98, -96, -94, -92,
        -90, -88, -85, -83, -81, -78, -76, -73, -71, -68, -65, -63, -60, -57, -54, -51,
        -49, -46, -43, -40, -37, -34, -31, -28, -25, -22, -19, -16, -12, -9, -6, -3,
    };

    enum class env_stage : unsigned char
    {
        off,
        attack,
        decay,
        sustain,
        release,
    };

    struct fm_operator_state
    {
        uint32_t phase = 0;
        uint32_t increment = 0;
        uint32_t envelope = 0; // 0..65535
        int16_t previous_output = 0;
        unsigned char multiplier = 1;
        unsigned char detune = 0;
        unsigned char attack_rate = 0;
        unsigned char decay_rate = 0;
        unsigned char sustain_rate = 0;
        unsigned char sustain_level = 0;
        unsigned char release_rate = 0;
        unsigned char total_level = 127;
        unsigned char hardware_total_level = 127; // last TL actually written to YM register
        // YM2612 EG state used by the source-accurate four-operator path.
        // The chip stores attenuation (0=loudest, 1023=quietest), not a linear
        // amplitude ramp. RS from $50+ is retained as a key-code shift.
        unsigned char rate_scale_shift = 3;
        unsigned char key_scale_rate = 0;
        uint16_t ym_attenuation = 1023;
        env_stage stage = env_stage::off;
    };

    struct lane_state
    {
        bool active = false;
        chip_audio_data::lane_kind kind = chip_audio_data::lane_kind::fm;
        uint32_t pc = 0;
        uint32_t return_stack[max_stack] = {};
        unsigned char stack_size = 0;
        unsigned char loop_remaining[loop_slots] = {};
        unsigned char wait = 1;
        unsigned char previous_duration = 0;
        signed char transpose = 0;
        signed char track_detune = 0; // source $E1 signed frequency-word offset
        unsigned char volume = 0;
        unsigned char pan = 0xC0;

        // Source $FF/$00 pan animation state.  These fields mirror track bytes
        // $1F-$24 closely enough that the sequence adapter can implement the
        // original StartPanAnim/DoPanAnim state machine without song hacks.
        unsigned char pan_anim_mode = 0;
        unsigned char pan_anim_pattern = 0;
        unsigned char pan_anim_index = 0;
        unsigned char pan_anim_steps = 0;
        unsigned char pan_anim_period = 0;
        unsigned char pan_anim_counter = 0;
        unsigned char instrument = 0;
        unsigned char note_stop_setting = 0;
        unsigned char note_stop_counter = 0;
        bool hold = false;
        bool keyed = false;
        int16_t current_note = -1;

        // Source $F0 triangle modulation.  PSIV stores delay, update period,
        // signed delta and a half-cycle length, then applies the accumulated
        // offset to the hardware frequency every source frame.
        bool modulation_enabled = false;
        unsigned char mod_delay = 0;
        unsigned char mod_delay_counter = 0;
        unsigned char mod_speed = 0;
        unsigned char mod_speed_counter = 0;
        signed char mod_delta = 0;
        signed char mod_current_delta = 0;
        unsigned char mod_half_steps = 0;
        unsigned char mod_step_counter = 0;
        int16_t mod_accumulator = 0;

        // PSG state.
        unsigned char psg_envelope = 0;
        unsigned char psg_envelope_pos = 0;
        unsigned char psg_noise_control = 0;
        bool psg_noise = false;
        uint32_t psg_phase = 0;
        uint32_t psg_increment = 0;
        uint16_t noise_lfsr = 0x4001;

        // DAC state.
        unsigned char dac_sound = 0;
        unsigned char dac_frequency = 0x8D;
        unsigned char dac_volume = 0x10;
        unsigned char dac_volume_control = 1;
        unsigned char dac_mode = 0;
        unsigned char dac_loop = 0;
        unsigned char dac_reverse = 0;
        /* Fill the byte tail before the next word-aligned DAC field; these
           are byte-valued channel attributes and do not require word alignment. */
        unsigned char algorithm = 7;
        unsigned char feedback = 0;
        uint32_t dac_position = 0; // 16.16 source-sample position
        uint32_t dac_step = 0;
        const signed char* dac_data = nullptr;
        unsigned int dac_length = 0;

        // FM state.
        #if defined(MD2GBA_PS4_M4A_NATIVE_ONLY)
        /* The PSC fallback never enters the live four-operator renderer. Keep
           a zero-sized GNU C++ placeholder so the source/reference functions
           still parse but no dead operator state is charged to EWRAM. */
        fm_operator_state operators[0] = {};
#else
        fm_operator_state operators[fm_operators] = {};
#endif
        const fm_multisample_data::fm_wave_zone* fm_zone = nullptr;
        uint32_t fm_wave_phase = 0;
        unsigned char fm_timbre_stage = 0;
        unsigned short fm_timbre_blend = 0; // 0..65535 between generated timbre stages

        // Small per-lane tonal state used only for narrow source-reference
        // mastering corrections.  This is deliberately not a global EQ: the
        // four-op backend is already a near-reference match on tracks such as
        // Behind the Circuit.
#if defined(MD2GBA_PS4_M4A_NATIVE_ONLY)
        int16_t low_body_state = 0;
        int16_t high_air_state = 0;
#else
        int low_body_state = 0;
        int high_air_state = 0;
#endif
    };

    lane_state lanes[max_lanes];
    const chip_audio_data::song_header* current_song = nullptr;
    bool engine_playing = false;
    bool allow_source_loops = true;
    unsigned char current_music_id = 0;
    unsigned char tempo_period = 0;
    unsigned char tempo_counter = 0;
    int master_volume = 256;
    fm_backend selected_fm_backend = fm_backend::multisample;

    // Genesis Plus GX (MAME-derived, hardware-verified) advances the YM2612
    // envelope counter once every three chip samples. At the NTSC YM clock
    // used by PSIV this is ~17.756 kHz. The GBA renderer runs at 31.536 kHz,
    // so a fractional accumulator produces the same average EG cadence.
    constexpr uint32_t ym2612_clock_hz = 7670454u;
    constexpr uint32_t ym2612_internal_sample_hz = ym2612_clock_hz / 144u;
    constexpr uint32_t ym2612_eg_hz = ym2612_internal_sample_hz / 3u;
    uint32_t ym_eg_phase = 0;
    uint16_t ym_eg_counter = 0;
    bool ym_eg_tick = false;

    /* PSCSTYLE hybrid-audio transaction state.  The AOT PCM backend can
       prepare a future block speculatively; this source-sequenced fallback
       must obey the same ownership rule.  Snapshot only the persistent driver
       state, never the immutable generated banks or per-frame render scratch. */
    struct transaction_snapshot
    {
        lane_state lane_copy[max_lanes];
        const chip_audio_data::song_header* song;
        bool playing;
        bool source_loops;
        unsigned char music_id;
        unsigned char tempo_period_v;
        unsigned char tempo_counter_v;
        int volume;
        fm_backend backend;
        uint32_t eg_phase;
        uint16_t eg_counter;
        bool eg_tick;
    };
    transaction_snapshot transaction_saved{};
    bool transaction_pending = false;

    __attribute__((noinline)) void copy_bytes(void* dst,const void* src,unsigned n)
    {
        unsigned char* d=static_cast<unsigned char*>(dst);
        const unsigned char* s=static_cast<const unsigned char*>(src);
        for(unsigned i=0;i<n;++i)d[i]=s[i];
    }
    void save_transaction_state(transaction_snapshot& out)
    {
        copy_bytes(out.lane_copy,lanes,sizeof(lanes));
        out.song=current_song; out.playing=engine_playing; out.source_loops=allow_source_loops;
        out.music_id=current_music_id; out.tempo_period_v=tempo_period; out.tempo_counter_v=tempo_counter;
        out.volume=master_volume; out.backend=selected_fm_backend;
        out.eg_phase=ym_eg_phase; out.eg_counter=ym_eg_counter; out.eg_tick=ym_eg_tick;
    }
    void restore_transaction_state(const transaction_snapshot& in)
    {
        copy_bytes(lanes,in.lane_copy,sizeof(lanes));
        current_song=in.song; engine_playing=in.playing; allow_source_loops=in.source_loops;
        current_music_id=in.music_id; tempo_period=in.tempo_period_v; tempo_counter=in.tempo_counter_v;
        master_volume=in.volume; selected_fm_backend=in.backend;
        ym_eg_phase=in.eg_phase; ym_eg_counter=in.eg_counter; ym_eg_tick=in.eg_tick;
    }

    constexpr unsigned char carrier_masks[8] = { 0x08, 0x08, 0x08, 0x08, 0x0A, 0x0E, 0x0E, 0x0F };

    unsigned int fm_source_fnum(unsigned char note_index);

    // YM2612 Total Level is logarithmic attenuation (~0.75 dB/step).
    // Q7 amplitude table, 127 = 0 dB.  The early renderer incorrectly used
    // (127-TL), which changed operator modulation depth and erased glassy FM timbres.
    constexpr unsigned char fm_tl_q7[128] = {
        127,116,107,98,90,82,76,69,64,58,54,49,45,41,38,35,
        32,29,27,25,23,21,19,17,16,15,13,12,11,10,10,9,
        8,7,7,6,6,5,5,4,4,4,3,3,3,3,2,2,
        2,2,2,2,1,1,1,1,1,1,1,1,1,1,1,1,
        1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
    };

    // Hardware EG increment patterns used by the MAME/Genesis Plus GX YM2612
    // core. Each row has the eight sub-cycle increments for one rate selector.
    constexpr unsigned char ym_eg_inc[19][8] = {
        {0,1,0,1,0,1,0,1}, {0,1,0,1,1,1,0,1},
        {0,1,1,1,0,1,1,1}, {0,1,1,1,1,1,1,1},
        {1,1,1,1,1,1,1,1}, {1,1,1,2,1,1,1,2},
        {1,2,1,2,1,2,1,2}, {1,2,2,2,1,2,2,2},
        {2,2,2,2,2,2,2,2}, {2,2,2,4,2,2,2,4},
        {2,4,2,4,2,4,2,4}, {2,4,4,4,2,4,4,4},
        {4,4,4,4,4,4,4,4}, {4,4,4,8,4,4,4,8},
        {4,8,4,8,4,8,4,8}, {4,8,8,8,4,8,8,8},
        {8,8,8,8,8,8,8,8}, {16,16,16,16,16,16,16,16},
        {0,0,0,0,0,0,0,0}
    };

    // Q16 amplitude at each 0.75 dB TL step. The YM envelope counter has eight
    // substeps per TL step; linear interpolation below preserves that finer EG
    // timing while the existing operator TL path remains unchanged.
    constexpr uint16_t ym_eg_amp_q16[129] = {
        65535, 60114, 55141, 50579, 46395, 42557, 39037, 35807,
        32845, 30128, 27636, 25350, 23253, 21329, 19565, 17946,
        16462, 15100, 13851, 12705, 11654, 10690, 9806, 8994,
        8250, 7568, 6942, 6368, 5841, 5358, 4914, 4508,
        4135, 3793, 3479, 3191, 2927, 2685, 2463, 2259,
        2072, 1901, 1744, 1599, 1467, 1346, 1234, 1132,
        1039, 953, 874, 802, 735, 674, 619, 568,
        521, 477, 438, 402, 369, 338, 310, 284,
        261, 239, 220, 201, 185, 169, 155, 143,
        131, 120, 110, 101, 93, 85, 78, 71,
        66, 60, 55, 51, 46, 43, 39, 36,
        33, 30, 28, 25, 23, 21, 20, 18,
        16, 15, 14, 13, 12, 11, 10, 9,
        8, 8, 7, 6, 6, 5, 5, 5,
        4, 4, 3, 3, 3, 3, 2, 2,
        2, 2, 2, 2, 1, 1, 1, 1,
        0,
    };

    struct ym_rate_parameters
    {
        unsigned char shift;
        unsigned char selector;
    };

    bool use_source_ym_envelope()
    {
        // v18 soundtrack-wide engine semantics.  Behind the Circuit proved
        // that the old quadratic envelope approximation turns source $E7 ties
        // into audible re-attacks/early decay.  The YM2612 EG is a chip rule,
        // not a song rule, so every live four-operator voice now uses the same
        // hardware-rate envelope scheduler.  The low-CPU multisample backend
        // retains its pre-rendered envelope behavior by design.
        return selected_fm_backend == fm_backend::four_operator_reference;
    }

    ym_rate_parameters ym_rate(unsigned effective_rate)
    {
        if(effective_rate < 32u)
        {
            return {11, 18};
        }
        if(effective_rate >= 96u)
        {
            return {0, 16};
        }

        const unsigned value = effective_rate - 32u;
        const unsigned group = value >> 2;
        const unsigned sub = value & 3u;
        if(group == 0)
        {
            return {11, static_cast<unsigned char>(sub < 2 ? 18 : sub)};
        }
        if(group <= 11)
        {
            return {static_cast<unsigned char>(11u - group),
                     static_cast<unsigned char>(sub)};
        }
        if(group == 12)
        {
            return {0, static_cast<unsigned char>(4u + sub)};
        }
        if(group == 13)
        {
            return {0, static_cast<unsigned char>(8u + sub)};
        }
        if(group == 14)
        {
            return {0, static_cast<unsigned char>(12u + sub)};
        }
        return {0, 16};
    }

    uint32_t ym_envelope_amplitude(unsigned attenuation)
    {
        if(attenuation >= 1023u)
        {
            return 0;
        }
        const unsigned tl = attenuation >> 3;
        const unsigned frac = attenuation & 7u;
        const uint32_t a = ym_eg_amp_q16[tl];
        const uint32_t b = ym_eg_amp_q16[tl + 1u];
        return (a * (8u - frac) + b * frac + 4u) >> 3;
    }

    unsigned ym_key_code(unsigned char note)
    {
        static constexpr unsigned char fkey[16] =
            {0,0,0,0,0,0,0,1,2,3,3,3,3,3,3,3};
        const unsigned block = (unsigned(note) / 12u) & 7u;
        const unsigned fnum = fm_source_fnum(note);
        return (block << 2) | fkey[(fnum >> 7) & 15u];
    }

    void update_ym_key_scale(lane_state& lane)
    {
        if(lane.current_note < 0)
        {
            return;
        }
        const unsigned key_code = ym_key_code(static_cast<unsigned char>(lane.current_note));
        for(auto& op : lane.operators)
        {
            op.key_scale_rate = static_cast<unsigned char>(key_code >> op.rate_scale_shift);
        }
    }

    void advance_ym_eg_clock()
    {
        ym_eg_tick = false;
        if(!use_source_ym_envelope())
        {
            return;
        }
        ym_eg_phase += ym2612_eg_hz;
        if(ym_eg_phase >= mixer_rate)
        {
            ym_eg_phase -= mixer_rate;
            ym_eg_counter = static_cast<uint16_t>(ym_eg_counter + 1u);
            if(ym_eg_counter >= 4096u || ym_eg_counter == 0u)
            {
                ym_eg_counter = 1;
            }
            ym_eg_tick = true;
        }
    }

    int clamp_int(int value, int low, int high)
    {
        return value < low ? low : (value > high ? high : value);
    }

    int8_t saturate8(int value)
    {
        return static_cast<int8_t>(clamp_int(value, -128, 127));
    }

    int fm_output_color(int value)
    {
        // The discrete YM2612 used by early Mega Drive / Genesis hardware is
        // not a perfectly linear DAC.  A clean wavetable rendering therefore
        // sounds noticeably more polite than the source console even when its
        // notes, envelopes and balance are correct.  Preserve PSC-style cheap
        // sample playback, but add a tiny Model-1-like ladder discontinuity and
        // a soft nonlinear drive before the FM bus is mixed.  This is a timbre
        // effect, not a replacement for source volume.
        //
        // PS4GBA_FM_COLOR_MODE=1 is the production/default "warm" setting.
        // Mode 2 is intentionally hotter and is useful for A/B host renders.
#ifndef PS4GBA_FM_COLOR_MODE
#define PS4GBA_FM_COLOR_MODE 2
#endif
#if PS4GBA_FM_COLOR_MODE == 0
        return value;
#else
        int x = clamp_int(value, -160, 160);

        // v19 exact-YM multisamples already carry the authentic harmonic
        // structure produced by the independent YM2612 core. Behind the
        // Circuit was the one reviewed song where applying the old synthetic
        // per-voice drive on top of that reference waveform re-created the
        // clipped/smeared synth problem. Keep its exact-YM waveform linear
        // here and reserve headroom at the song/DAC buses instead.
        if(selected_fm_backend == fm_backend::multisample && current_music_id == 0x88)
        {
            return clamp_int(x, -126, 126);
        }

        const bool end_of_millennium = current_music_id == 0xAC;
        const bool behind_the_circuit = current_music_id == 0x88;
        const bool gentle_fm_color = end_of_millennium || behind_the_circuit;

        // Approximate the discrete YM2612 ladder discontinuity: the negative
        // half-cycle receives a small extra step. End of the Millennium and
        // Behind the Circuit both use the gentler one-count step: End to keep
        // its approved less-brittle finish, Behind to avoid pre-master synth
        // damage while its corrected tied envelopes sustain.
        if(x < -1)
        {
            x -= gentle_fm_color ? 1 : 3;
        }

#if PS4GBA_FM_COLOR_MODE >= 2
        // Keep the approved hot curve elsewhere, but back nonlinear drive
        // down for the two reviewed gentle-color songs. This is pre-master
        // headroom/timbre protection, not a low-pass or broad EQ reset.
        const int drive_q8 = gentle_fm_color ? 296 : 352;
        const int knee = gentle_fm_color ? 92 : 78;
        const int span = gentle_fm_color ? 58 : 48;
#else
        constexpr int drive_q8 = 312; // 1.219x into the nonlinear knee
        constexpr int knee = 86;
        constexpr int span = 52;
#endif
        x = (x * drive_q8) >> 8;
        const bool negative = x < 0;
        int magnitude = negative ? -x : x;
        if(magnitude > knee)
        {
            const int over = magnitude - knee;
            magnitude = knee + (over * span) / (over + span);
        }
        magnitude = clamp_int(magnitude, 0, 126);
        return negative ? -magnitude : magnitude;
#endif
    }

    int song_master_q8(unsigned char music_id, fm_backend backend)
    {
        // v14 soundtrack propagation: retain the user-approved v13 relative
        // balance (FM 6.0x / DAC 5.25x / PSG 0.344x), but give naturally
        // denser songs enough headroom that the 8-bit Direct Sound limiter
        // does not flatten them. IDs $81-$B4 are contiguous. Values were
        // generated by a 40-second host audit of the exact runtime mixer.
        static constexpr unsigned short four_op_q8[52] = {
            217, 256, 256, 256, 231, 256, 231, 188, 254, 256, 256, 214, 147,
            256, 185, 256, 185, 256, 234, 190, 173, 171, 256, 184, 256, 256,
            251, 239, 256, 256, 233, 154, 256, 256, 256, 256, 256, 256, 188,
            256, 237, 173, 256, 224, 256, 256, 256, 190, 173, 256, 256, 240
        };
        static constexpr unsigned short multisample_q8[52] = {
            256, 256, 256, 256, 256, 256, 256, 256, 256, 256, 256, 256, 159,
            256, 213, 256, 217, 256, 253, 221, 196, 205, 256, 197, 256, 256,
            256, 256, 256, 256, 247, 184, 256, 256, 256, 256, 256, 256, 244,
            256, 244, 219, 256, 256, 256, 256, 256, 204, 210, 256, 256, 256
        };
        if(music_id < 0x81 || music_id > 0xB4)
        {
            return 256;
        }
        const unsigned index = unsigned(music_id - 0x81);
#ifndef PS4GBA_BEHIND_MASTER_Q8
#define PS4GBA_BEHIND_MASTER_Q8 140
#endif
        if(backend == fm_backend::four_operator_reference && music_id == 0x88)
        {
            // v17p6: source-correct $E7 ties and YM EG sustain retain far more
            // real synth energy than the old fake re-key/fast-decay path.
            // Recalibrate the song master rather than clipping that restored
            // envelope through the Direct Sound limiter.
            return PS4GBA_BEHIND_MASTER_Q8;
        }
        if(backend == fm_backend::four_operator_reference && music_id == 0xAC)
        {
            // v17 restores many real DAC retriggers in this song.  Give that
            // recovered transient energy ~0.33 dB of extra master headroom
            // while leaving the low-body/high-air voice shaping intact.
            return 216;
        }
        return backend == fm_backend::four_operator_reference ?
            int(four_op_q8[index]) : int(multisample_q8[index]);
    }

    int source_eg_headroom_q8(unsigned char music_id, fm_backend backend)
    {
        // v18: once source $E7 ties and YM-rate envelopes became soundtrack-
        // wide engine semantics, sustained FM energy increased on songs whose
        // old master values had been calibrated against the fast-decay/fake-
        // re-key approximation.  This table restores comparable *bus headroom*
        // without shortening the corrected envelopes or crushing individual
        // voices.  It is a PSIV mix-profile table, not a YM2612 rule.
        static constexpr unsigned short four_op_q8[52] = {
            256, 256, 256, 237, 234, 256, 237, 256, 253, 256, 256, 256, 237,
            256, 234, 210, 256, 234, 253, 256, 256, 242, 253, 242, 237, 245,
            245, 256, 245, 237, 221, 256, 221, 253, 256, 256, 256, 256, 245,
            234, 256, 248, 256, 231, 256, 224, 256, 245, 245, 256, 256, 237
        };
        static constexpr unsigned short ymref_multisample_q8[52] = {
            256, 256, 256, 256, 256, 256, 256, 256, 256, 256, 256, 256, 256,
            256, 244, 256, 256, 256, 256, 256, 256, 256, 256, 256, 256, 256,
            256, 256, 256, 256, 256, 231, 256, 256, 256, 256, 256, 256, 244,
            256, 256, 247, 256, 256, 256, 256, 256, 256, 256, 256, 256, 256
        };
        if(music_id < 0x81 || music_id > 0xB4)
        {
            return 256;
        }
        const unsigned index = unsigned(music_id - 0x81);
        if(backend == fm_backend::multisample)
        {
            // v19 exact-YM timbres restore harmonic/sustain energy that the
            // old procedural wavebank did not carry. Only tracks proven hotter
            // in full/60s QA receive profile headroom; quieter/more-open tracks
            // are intentionally not normalized back toward v18.
            return int(ymref_multisample_q8[index]);
        }
        return int(four_op_q8[index]);
    }

    int song_dac_gain_q8(unsigned char music_id)
    {
        // v17 source-correct DAC dynamics.  v16's sequencer did not rewind the
        // PCM cursor for duration-only DAC events, so the v14/v15 per-song
        // mastering table was calibrated while many real percussion attacks
        // were effectively absent.  Keep the approved FM/PSG mix intact and
        // trim only the DAC bus on songs whose restored retrigger density would
        // otherwise spend too much time in the final Direct Sound soft knee.
        // IDs $81-$B4 are contiguous.  256 = unity.
        static constexpr unsigned short dac_q8[52] = {
            256, 256, 256, 253, 256, 256, 256, 176, 256, 256, 256, 256, 206,
            256, 224, 256, 256, 256, 253, 256, 256, 256, 256, 202, 256, 191,
            187, 251, 256, 173, 224, 247, 226, 243, 256, 256, 256, 256, 186,
            256, 180, 205, 256, 220, 256, 222, 256, 242, 201, 256, 256, 108
        };
        if(music_id < 0x81 || music_id > 0xB4)
        {
            return 256;
        }
#ifndef PS4GBA_BEHIND_DAC_Q8
#define PS4GBA_BEHIND_DAC_Q8 80
#endif
        if(music_id == 0x88)
        {
            // With true $E7 ties and YM-rate sustain, Behind no longer needs
            // an artificially boosted FM bus. Reduce only its oversized DAC
            // calibration so the source FM arrangement can crest over the
            // beat without driving the synth itself into saturation.
            return PS4GBA_BEHIND_DAC_Q8;
        }
        return int(dac_q8[unsigned(music_id - 0x81)]);
    }

    int soft_limit_music(int value)
    {
        // Leave some Direct Sound headroom for Maxmod SFX and avoid the harsh
        // hard clipping that made the early host renders needlessly brittle.
        // This is an integer soft knee: linear through +/-92, then smoothly
        // approaches +/-124 instead of chopping peaks at 8-bit full scale.
        const bool negative = value < 0;
        int magnitude = negative ? -value : value;
        // v6 mix: keep substantially more musical rise when the arrangement
        // thickens.  The earlier knee at 92 flattened Motabia Town exactly
        // when DAC percussion and the full FM bed arrived.
        // Behind the Circuit's repeating FM foundation can otherwise pin the
        // tonal-only limiter before its upper synth lanes have room to bloom.
        // Give that one song a later knee; the final combined safety limiter
        // still protects 8-bit Direct Sound headroom.
        constexpr int knee = 108;
        constexpr int span = 18;
        if(magnitude > knee)
        {
            const int over = magnitude - knee;
            magnitude = knee + (over * span) / (over + span);
        }
        magnitude = clamp_int(magnitude, 0, 124);
        return negative ? -magnitude : magnitude;
    }

    int soft_limit_music_peak(int value)
    {
        // Final high-knee safety for the separately limited tonal bus + hot
        // DAC transient bus. This preserves the requested punch but avoids
        // flat 8-bit tops when a drum transient lands on a dense FM chord.
        const bool negative = value < 0;
        int magnitude = negative ? -value : value;
        constexpr int knee = 116;
        constexpr int span = 11;
        if(magnitude > knee)
        {
            const int over = magnitude - knee;
            magnitude = knee + (over * span) / (over + span);
        }
        magnitude = clamp_int(magnitude, 0, 127);
        return negative ? -magnitude : magnitude;
    }

    signed char signed8(unsigned char value)
    {
        return static_cast<signed char>(value);
    }

    int16_t signed16(unsigned short value)
    {
        return static_cast<int16_t>(value);
    }

    unsigned char read_byte(lane_state& lane)
    {
        return chip_audio_data::source_byte(lane.pc++);
    }

    unsigned short read_be16(lane_state& lane)
    {
        const unsigned short hi = read_byte(lane);
        return static_cast<unsigned short>((hi << 8) | read_byte(lane));
    }

    void key_off(lane_state& lane)
    {
        lane.keyed = false;
        // The native PSC-style multisample backend has no live YM operator EG
        // state. Keep the four-operator state only for the explicit reference
        // backend instead of updating dead synthesis state on GBA.
        if(native_multisample_target || selected_fm_backend == fm_backend::multisample)
        {
            return;
        }
        if(lane.kind == chip_audio_data::lane_kind::fm)
        {
            for(auto& op : lane.operators)
            {
                if(op.stage != env_stage::off)
                {
                    op.stage = env_stage::release;
                }
            }
        }
    }

    uint32_t fm_operator_increment(uint32_t base, unsigned char note,
                                        unsigned char multiplier, unsigned char detune)
    {
        // YM2612 DT1 is not decorative: Motabia Town's late patch 1 uses
        // DT values 6,3,1,6 across its operators. Ignoring those values collapses
        // the intended beating/chorus into a flat single timbre.  These are the
        // measured OPN detune curves used by established YM2612 cores.
        static constexpr unsigned char fkey[16] =
            { 0,0,0,0,0,0,0,1,2,3,3,3,3,3,3,3 };
        static constexpr unsigned char dt[4][32] = {
            { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
            { 0,0,0,0,1,1,1,1,1,1,1,1,2,2,2,2, 2,3,3,3,4,4,4,5,5,6,6,7,8,8,8,8 },
            { 1,1,1,1,2,2,2,2,2,3,3,3,4,4,4,5, 5,6,6,7,8,8,9,10,11,12,13,14,16,16,16,16 },
            { 2,2,2,2,2,3,3,3,4,4,4,5,5,6,6,7, 8,8,9,10,11,12,13,14,16,17,19,20,22,22,22,22 }
        };
        const unsigned fnum = fm_source_fnum(note);
        const unsigned block = (unsigned(note) / 12u) & 7u;
        const unsigned keycode = (block << 2) | fkey[(fnum >> 7) & 0x0Fu];
        const unsigned magnitude = detune & 3u;
        const int raw = int(dt[magnitude][keycode]);
        // DT table unit is (OPN clock/144)/2^20 Hz. Convert directly into
        // this mixer's 32-bit phase domain: 2^(32-20) = 4096.
        constexpr uint64_t numerator = uint64_t(7670454u) * 4096u;
        constexpr uint64_t denominator = uint64_t(144u) * mixer_rate;
        constexpr int phase_unit = int((numerator + denominator / 2u) / denominator);
        const int signed_delta = (detune & 4u) ? -(raw * phase_unit) : (raw * phase_unit);
        int64_t adjusted = int64_t(base) + signed_delta;
        if(adjusted < 1)
        {
            adjusted = 1;
        }
        const unsigned mul = multiplier & 0x0Fu;
        uint64_t result = mul == 0 ? uint64_t(adjusted) >> 1 :
            uint64_t(adjusted) * mul;
        if(result > 0xFFFFFFFFu)
        {
            result = 0xFFFFFFFFu;
        }
        return static_cast<uint32_t>(result);
    }

    unsigned int fm_source_fnum(unsigned char note_index)
    {
        static constexpr unsigned short fm_fnums[12] = {
            0x025E, 0x0284, 0x02AB, 0x02D3, 0x02FE, 0x032D,
            0x035C, 0x038F, 0x03C5, 0x03FF, 0x043C, 0x047C
        };
        return fm_fnums[note_index % 12u];
    }

    uint32_t fm_effective_increment(const lane_state& lane)
    {
        if(lane.current_note < 0)
        {
            return 0;
        }

        // GetFMFreq stores the packed YM frequency word (block<<11 | FNUM).
        // DoModulationEnv then ADD.Ws source $E1 detune and the F0 triangle
        // accumulator to that word.  Work in the same packed domain so a carry
        // across FNUM/block is preserved instead of clamping the low 11 bits.
        const unsigned char note = static_cast<unsigned char>(lane.current_note);
        const unsigned original_fnum = fm_source_fnum(note);
        const unsigned original_block = (unsigned(note) / 12u) & 7u;
        const unsigned original_word = (original_block << 11) | original_fnum;
        const int offset = int(lane.track_detune) + int(lane.mod_accumulator);
        const unsigned shifted_word = unsigned((int(original_word) + offset) & 0xFFFF);
        const unsigned shifted_fnum = shifted_word & 0x07FFu;
        const unsigned shifted_block = (shifted_word >> 11) & 7u;
        if(shifted_fnum == 0)
        {
            return 1;
        }

        uint64_t result = chip_audio_data::fm_phase_increment(note);
        result = (result * shifted_fnum + original_fnum / 2u) / original_fnum;
        if(shifted_block > original_block)
        {
            result <<= (shifted_block - original_block);
        }
        else if(shifted_block < original_block)
        {
            result >>= (original_block - shifted_block);
        }
        if(result == 0)
        {
            result = 1;
        }
        return static_cast<uint32_t>(result > 0xFFFFFFFFu ? 0xFFFFFFFFu : result);
    }

    unsigned int psg_source_period(unsigned char note_index)
    {
        constexpr uint32_t psg_table = 0x0D0FA2u;
        const uint32_t address = psg_table + uint32_t(note_index) * 2u;
        return (unsigned(chip_audio_data::source_byte(address)) << 8) |
            unsigned(chip_audio_data::source_byte(address + 1u));
    }

    PS4_M4A_SEQ_HOT uint32_t psg_effective_increment(const lane_state& lane)
    {
        if(lane.current_note < 0)
        {
            return 0;
        }
        const unsigned char note = static_cast<unsigned char>(lane.current_note);
        const uint32_t base = chip_audio_data::psg_phase_increment(note);
        if(native_multisample_target)
        {
            // The current native profile's source blob begins at $0D1A00, so
            // the older adapter's $0D0FA2 period lookup always returned zero
            // and therefore always returned this precomputed increment. Make
            // that established native behavior explicit and remove unreachable
            // 64-bit multiply/divide work from the GBA control path. A later
            // generated period table can restore source PSG detune/modulation
            // without putting wide division back in SoundMain.
            return base;
        }
        const unsigned int period = psg_source_period(note);
        if(period == 0)
        {
            return base;
        }
        // The same signed $E1/F0 frequency offsets feed the PSG period word.
        const int shifted = clamp_int(int(period) + int(lane.track_detune) +
                                      int(lane.mod_accumulator), 1, 0x03FF);
        return static_cast<uint32_t>((uint64_t(base) * period) / unsigned(shifted));
    }

    void reset_modulation_for_note(lane_state& lane)
    {
        if(!lane.modulation_enabled)
        {
            return;
        }
        lane.mod_delay_counter = lane.mod_delay;
        lane.mod_speed_counter = lane.mod_speed;
        lane.mod_current_delta = lane.mod_delta;
        lane.mod_step_counter = lane.mod_half_steps;
        lane.mod_accumulator = 0;
    }

    PS4_M4A_SEQ_HOT void refresh_fm_increments(lane_state& lane)
    {
        if(native_multisample_target || selected_fm_backend == fm_backend::multisample)
        {
            return;
        }
        if(lane.current_note < 0)
        {
            return;
        }
        const uint32_t base = fm_effective_increment(lane);
        for(auto& op : lane.operators)
        {
            op.increment = fm_operator_increment(base, static_cast<unsigned char>(lane.current_note),
                                                 op.multiplier, op.detune);
        }
    }

    PS4_M4A_SEQ_HOT void refresh_fm_zone(lane_state& lane)
    {
        if(current_song && lane.current_note >= 0)
        {
            lane.fm_zone = fm_multisample_data::find_zone(
                current_song->instrument_bank_address, lane.instrument,
                static_cast<unsigned char>(lane.current_note));
        }
        else
        {
            lane.fm_zone = nullptr;
        }
    }

    void refresh_fm_hardware_volume(lane_state& lane)
    {
        if(native_multisample_target || selected_fm_backend == fm_backend::multisample)
        {
            return;
        }
        if(lane.kind != chip_audio_data::lane_kind::fm)
        {
            return;
        }
        // RefreshFMVol returns immediately when the signed channel-volume byte
        // is negative.  That means the YM2612 keeps the *previous* carrier TL
        // register values; masking bit 7 and recomputing from the new byte is
        // incorrect and can make source fade/mute states jump loud again.
        if(static_cast<signed char>(lane.volume) < 0)
        {
            return;
        }
        const unsigned char mask = carrier_masks[lane.algorithm & 7];
        for(int op_index = 0; op_index < fm_operators; ++op_index)
        {
            if(mask & (1u << op_index))
            {
                auto& op = lane.operators[op_index];
                const unsigned sum = unsigned(op.total_level) + unsigned(lane.volume);
                // Source RefreshFMVol skips a carrier write on 8-bit carry,
                // preserving the old hardware TL for that operator.
                if(sum <= 0xFFu)
                {
                    op.hardware_total_level = static_cast<unsigned char>(sum & 0x7Fu);
                }
            }
        }
    }

    PS4_M4A_SEQ_HOT void load_fm_patch(lane_state& lane, unsigned char instrument)
    {
        lane.instrument = instrument;
        if(!current_song)
        {
            return;
        }
        if(native_multisample_target || selected_fm_backend == fm_backend::multisample)
        {
            // PSC-native path: the converted voicegroup is already lowered to
            // multisample zones. No live YM patch/register/operator state is
            // required for playback; only the instrument id + note zone matter.
            refresh_fm_zone(lane);
            return;
        }
        const uint32_t start = current_song->instrument_bank_address + unsigned(instrument) * 25u;
        const unsigned char algfb = chip_audio_data::source_byte(start);
        lane.algorithm = algfb & 7;
        lane.feedback = (algfb >> 3) & 7;
        for(int op_index = 0; op_index < fm_operators; ++op_index)
        {
            auto& op = lane.operators[op_index];
            const unsigned char muldt = chip_audio_data::source_byte(start + 1u + unsigned(op_index));
            const unsigned char ksar = chip_audio_data::source_byte(start + 5u + unsigned(op_index));
            const unsigned char amdr = chip_audio_data::source_byte(start + 9u + unsigned(op_index));
            const unsigned char sr = chip_audio_data::source_byte(start + 13u + unsigned(op_index));
            const unsigned char slrr = chip_audio_data::source_byte(start + 17u + unsigned(op_index));
            op.multiplier = muldt & 0x0F;
            op.detune = (muldt >> 4) & 7;
            op.attack_rate = ksar & 0x1F;
            op.rate_scale_shift = static_cast<unsigned char>(3u - ((ksar >> 6) & 3u));
            op.decay_rate = amdr & 0x1F;
            op.sustain_rate = sr & 0x1F;
            op.sustain_level = (slrr >> 4) & 0x0F;
            op.release_rate = slrr & 0x0F;
            op.total_level = chip_audio_data::source_byte(start + 21u + unsigned(op_index)) & 0x7F;
        }
        // SendFMIns always writes the patch TL registers, adding the current
        // channel-volume byte to carriers before the hardware's 7-bit TL field
        // is observed.  This is separate from RefreshFMVol's negative guard.
        const unsigned char carrier_mask = carrier_masks[lane.algorithm & 7];
        for(int op_index = 0; op_index < fm_operators; ++op_index)
        {
            auto& op = lane.operators[op_index];
            const unsigned raw = unsigned(op.total_level) +
                ((carrier_mask & (1u << op_index)) ? unsigned(lane.volume) : 0u);
            op.hardware_total_level = static_cast<unsigned char>(raw & 0x7Fu);
        }
        refresh_fm_increments(lane);
        update_ym_key_scale(lane);
        refresh_fm_zone(lane);
    }

    void key_on_fm(lane_state& lane)
    {
        lane.keyed = lane.current_note >= 0;
        if(!lane.keyed)
        {
            return;
        }
        lane.fm_wave_phase = 0;
        lane.fm_timbre_stage = 0;
        lane.fm_timbre_blend = 0;
        reset_modulation_for_note(lane);
        refresh_fm_zone(lane);
        if(native_multisample_target || selected_fm_backend == fm_backend::multisample)
        {
            return;
        }
        refresh_fm_increments(lane);
        update_ym_key_scale(lane);
        for(auto& op : lane.operators)
        {
            if(use_source_ym_envelope())
            {
                // Real YM key-on restarts phase but begins Attack from the
                // current attenuation; it does not force the EG back to
                // silence. This matters when a new note follows a release
                // closely, and $E7 ties avoid this function entirely.
                const unsigned ar = op.attack_rate ?
                    32u + unsigned(op.attack_rate) * 2u + op.key_scale_rate : 0u;
                if(ar >= 94u)
                {
                    op.ym_attenuation = 0;
                    op.envelope = 65535u;
                    op.stage = op.sustain_level == 0 ? env_stage::sustain : env_stage::decay;
                }
                else if(op.ym_attenuation == 0)
                {
                    op.envelope = 65535u;
                    op.stage = op.sustain_level == 0 ? env_stage::sustain : env_stage::decay;
                }
                else
                {
                    op.envelope = ym_envelope_amplitude(op.ym_attenuation);
                    op.stage = env_stage::attack;
                }
            }
            else
            {
                op.envelope = 0;
                op.stage = env_stage::attack;
            }
        }
    }

    PS4_M4A_SEQ_HOT void setup_dac_sample(lane_state& lane)
    {
        lane.dac_data = chip_audio_data::dac_sample_data(lane.dac_sound, lane.dac_length);
        lane.dac_position = lane.dac_reverse && lane.dac_length ? ((lane.dac_length - 1u) << 16) : 0;

        // P99 / PSC-style lowering: the source DAC driver has only 29 legal
        // $81..$9D pitch entries.  P98 recomputed the fixed-point resample step
        // with a 64-bit multiply/divide every time a DAC note was triggered.
        // MusicPlayer-style players keep this as table/channel state instead.
        // These constants are the bit-exact results of the former formula at
        // the PSC mode-4 mixer rate (13379 Hz), so output does not change.
        static constexpr uint16_t dac_step_81_9d[29] = {
            0x2654,0x2881,0x2AD6,0x2D54,0x2FFF,0x32D9,0x35B8,0x38EF,
            0x3C8F,0x406F,0x4490,0x48F1,0x4D91,0x526C,0x577A,0x5CAF,
            0x628D,0x6892,0x6EA7,0x757C,0x7C4E,0x83F8,0x8CA4,0x93EF,
            0x9D74,0xA6A6,0xAF2C,0xBAA1,0xC563
        };
        uint32_t step = 1u;
        if(lane.dac_frequency >= 0x81 && lane.dac_frequency <= 0x9D)
        {
            step = dac_step_81_9d[unsigned(lane.dac_frequency - 0x81)];
        }
        lane.dac_step = step;
    }

    PS4_M4A_SEQ_HOT void note_event(lane_state& lane, unsigned char event, unsigned char duration)
    {
        // $E7 is scoped to exactly one sequence event: Update*Track clears bit
        // 4 before processing commands, cfE7 sets it, and the next update clears
        // it again.  Capture and clear it here for all lane kinds.
        const bool held = lane.hold;
        lane.hold = false;

        if(lane.kind == chip_audio_data::lane_kind::fm)
        {
            const bool continuation = held && lane.keyed;
            if(!held)
            {
                key_off(lane);
            }

            if(event >= 0x80)
            {
                if(event == 0x80)
                {
                    // A held rest is how the source extends a keyed note
                    // without re-writing its frequency.  A normal rest keys off.
                    if(!continuation)
                    {
                        lane.current_note = -1;
                    }
                }
                else
                {
                    lane.current_note = (int(event - 0x80) + int(lane.transpose)) & 0x7F;
                    refresh_fm_increments(lane);
                    if(!native_multisample_target && selected_fm_backend != fm_backend::multisample) update_ym_key_scale(lane);
                    refresh_fm_zone(lane);
                }
            }

            if(lane.current_note >= 0)
            {
                if(continuation)
                {
                    // True YM tie/legato: preserve operator EG/phase and F0
                    // modulation state.  A tied pitch change only updates the
                    // frequency; a duration-only event simply extends the note.
                    lane.keyed = true;
                }
                else
                {
                    key_on_fm(lane);
                }
            }
        }
        else if(lane.kind == chip_audio_data::lane_kind::psg)
        {
            const bool continuation = held && lane.keyed;
            if(event >= 0x80)
            {
                if(event == 0x80)
                {
                    // PSG $80 is an explicit rest and DoPSGNoteOff wins even
                    // when $E7 was seen before it.
                    lane.current_note = -1;
                    lane.keyed = false;
                }
                else
                {
                    // GetPSGFreq subtracts $81 before applying transpose.
                    lane.current_note = (int(event - 0x81) + int(lane.transpose)) & 0x7F;
                    lane.psg_increment = psg_effective_increment(lane);
                    lane.keyed = true;
                    if(!continuation)
                    {
                        lane.psg_envelope_pos = 0;
                        reset_modulation_for_note(lane);
                    }
                }
            }
            else if(lane.current_note >= 0)
            {
                lane.keyed = true;
                if(!continuation)
                {
                    lane.psg_envelope_pos = 0;
                    reset_modulation_for_note(lane);
                }
            }
            if(lane.current_note >= 0)
            {
                lane.psg_increment = psg_effective_increment(lane);
            }
        }
        else
        {
            if(event >= 0x80)
            {
                if(lane.dac_mode == 1)
                {
                    if(event == 0x80)
                    {
                        lane.dac_sound = 0;
                        lane.dac_data = nullptr;
                        lane.keyed = false;
                    }
                    else
                    {
                        lane.dac_sound = event - 0x80;
                        lane.keyed = true;
                        setup_dac_sample(lane);
                    }
                }
                else
                {
                    if(event == 0x80)
                    {
                        lane.keyed = false;
                    }
                    else
                    {
                        lane.dac_frequency = event;
                        lane.keyed = lane.dac_sound != 0;
                        setup_dac_sample(lane);
                    }
                }
            }
            else if(lane.dac_data)
            {
                // UpdateDACTrack re-sends the latched sound after *every* live
                // duration-only event.  Rewind the PCM cursor so the event is
                // a real retrigger rather than a no-op at end-of-sample.
                lane.keyed = lane.dac_sound != 0;
                if(lane.keyed)
                {
                    setup_dac_sample(lane);
                }
            }
        }

        // TickMultiplier performs ADD.B arithmetic. A product of zero is valid
        // and the next unconditional SUBQ.B walks 0 -> 255.
        const unsigned int multiplied = unsigned(duration) *
            unsigned(current_song ? current_song->tick_multiplier : 1);
        lane.previous_duration = duration;
        lane.wait = static_cast<unsigned char>(multiplied);

        // FinishTrkUpdate deliberately does not reload note-stop/modulation/
        // envelope counters on an $E7-held event.  This is essential for long
        // tied phrases and for DAC note-stop gates.
        if(!held)
        {
            lane.note_stop_counter = lane.note_stop_setting;
        }
    }

    void deactivate(lane_state& lane)
    {
        key_off(lane);
        lane.active = false;
        lane.keyed = false;
    }

    PS4_M4A_SEQ_HOT void process_lane_event(lane_state& lane)
    {
        // Commands may chain before the next note/duration event. Bound a corrupt
        // stream rather than allowing it to stall an audio frame forever.
        for(int operations = 0; operations < 128 && lane.active; ++operations)
        {
            const uint32_t command_address = lane.pc;
            const unsigned char value = read_byte(lane);
            if(value < 0xE0)
            {
                unsigned char duration = lane.previous_duration;
                if(value < 0x80)
                {
                    duration = value;
                    note_event(lane, value, duration);
                    return;
                }
                const unsigned char following = chip_audio_data::source_byte(lane.pc);
                if(following < 0x80)
                {
                    duration = read_byte(lane);
                }
                note_event(lane, value, duration);
                return;
            }

            switch(value)
            {
            case 0xE0: // pan
                lane.pan = read_byte(lane);
                break;
            case 0xE1: // signed frequency-word detune
                lane.track_detune = signed8(read_byte(lane));
                refresh_fm_increments(lane);
                if(lane.kind == chip_audio_data::lane_kind::psg && lane.current_note >= 0)
                {
                    lane.psg_increment = psg_effective_increment(lane);
                }
                break;
            case 0xE2: // communication byte
                (void) read_byte(lane);
                break;
            case 0xE3:
                lane.dac_volume_control = read_byte(lane);
                if(lane.kind == chip_audio_data::lane_kind::dac && lane.dac_volume_control == 0)
                {
                    lane.keyed = false;
                }
                break;
            case 0xE4:
                lane.dac_loop = read_byte(lane);
                break;
            case 0xE5:
            {
                const unsigned char psg_delta = read_byte(lane);
                const unsigned char fm_delta = read_byte(lane);
                lane.volume = static_cast<unsigned char>(lane.volume + (lane.kind == chip_audio_data::lane_kind::psg ? psg_delta : fm_delta));
                if(lane.kind == chip_audio_data::lane_kind::fm)
                {
                    refresh_fm_hardware_volume(lane);
                }
                break;
            }
            case 0xE6:
            case 0xEC:
                lane.volume = static_cast<unsigned char>(lane.volume + read_byte(lane));
                if(value == 0xE6 && lane.kind == chip_audio_data::lane_kind::fm)
                {
                    refresh_fm_hardware_volume(lane);
                }
                break;
            case 0xE7:
                lane.hold = true;
                break;
            case 0xE8:
                lane.note_stop_setting = read_byte(lane);
                lane.note_stop_counter = lane.note_stop_setting;
                break;
            case 0xE9: // YM LFO + AMS/FMS
                (void) read_byte(lane);
                (void) read_byte(lane);
                break;
            case 0xEA:
                tempo_period = read_byte(lane);
                tempo_counter = tempo_period;
                break;
            case 0xEB: // source SFX request; game SFX remain Maxmod-owned
                (void) read_byte(lane);
                break;
            case 0xED:
                lane.pan = read_byte(lane);
                break;
            case 0xEE:
                lane.dac_sound = read_byte(lane);
                setup_dac_sample(lane);
                break;
            case 0xEF:
                load_fm_patch(lane, read_byte(lane));
                break;
            case 0xF0: // source triangle modulation setup (DoModulation)
            {
                lane.modulation_enabled = true;
                lane.mod_delay = read_byte(lane);
                lane.mod_speed = read_byte(lane);
                lane.mod_delta = signed8(read_byte(lane));
                lane.mod_half_steps = static_cast<unsigned char>(read_byte(lane) >> 1);
                reset_modulation_for_note(lane);
                break;
            }
            case 0xF1:
                (void) read_byte(lane); (void) read_byte(lane);
                break;
            case 0xF2:
                deactivate(lane);
                return;
            case 0xF3:
                lane.psg_noise = true;
                lane.psg_noise_control = read_byte(lane);
                break;
            case 0xF4:
                (void) read_byte(lane);
                break;
            case 0xF5:
                lane.psg_envelope = read_byte(lane);
                lane.psg_envelope_pos = 0;
                break;
            case 0xF6:
            {
                const unsigned short raw = read_be16(lane);
                const uint32_t after = lane.pc;
                const uint32_t target = static_cast<uint32_t>(static_cast<int32_t>(after) + signed16(raw) - 1);
                if(!allow_source_loops && target <= command_address)
                {
                    deactivate(lane);
                    return;
                }
                lane.pc = target;
                break;
            }
            case 0xF7:
            {
                const unsigned char index = read_byte(lane);
                const unsigned char repeats = read_byte(lane);
                const unsigned short raw = read_be16(lane);
                const unsigned slot = index & (loop_slots - 1);
                if(lane.loop_remaining[slot] == 0)
                {
                    lane.loop_remaining[slot] = repeats ? repeats : 1;
                }
                --lane.loop_remaining[slot];
                if(lane.loop_remaining[slot] != 0)
                {
                    lane.pc = static_cast<uint32_t>(static_cast<int32_t>(lane.pc) + signed16(raw) - 1);
                }
                break;
            }
            case 0xF8:
            {
                const unsigned short raw = read_be16(lane);
                if(lane.stack_size >= max_stack)
                {
                    deactivate(lane);
                    return;
                }
                lane.return_stack[lane.stack_size++] = lane.pc;
                lane.pc = static_cast<uint32_t>(static_cast<int32_t>(lane.pc) + signed16(raw) - 1);
                break;
            }
            case 0xF9:
                if(lane.stack_size == 0)
                {
                    deactivate(lane);
                    return;
                }
                lane.pc = lane.return_stack[--lane.stack_size];
                break;
            case 0xFA:
                lane.dac_reverse = read_byte(lane);
                break;
            case 0xFB:
                lane.transpose = static_cast<signed char>(lane.transpose + signed8(read_byte(lane)));
                refresh_fm_increments(lane);
                break;
            case 0xFC:
                lane.dac_volume = read_byte(lane);
                break;
            case 0xFD:
                lane.dac_mode = read_byte(lane);
                break;
            case 0xFE:
                // FM3 special frequencies. None of the synthesized voices executes
                // YM register writes directly; consume the source payload safely.
                (void) read_byte(lane); (void) read_byte(lane); (void) read_byte(lane); (void) read_byte(lane);
                break;
            case 0xFF:
            {
                // Meta command dispatch. Retail PSIV music uses only $FF,$00
                // (pan animation), but parse its payload exactly: meta index,
                // then mode, then four more bytes when mode != 0.
                const unsigned char meta = read_byte(lane);
                if(meta != 0)
                {
                    deactivate(lane);
                    return;
                }
                lane.pan_anim_mode = read_byte(lane);
                if(lane.pan_anim_mode != 0)
                {
                    lane.pan_anim_pattern = read_byte(lane);
                    lane.pan_anim_index = read_byte(lane);
                    lane.pan_anim_steps = read_byte(lane);
                    lane.pan_anim_period = read_byte(lane);
                    lane.pan_anim_counter = lane.pan_anim_period;
                }
                else
                {
                    lane.pan_anim_pattern = 0;
                    lane.pan_anim_index = 0;
                    lane.pan_anim_steps = 0;
                    lane.pan_anim_period = 0;
                    lane.pan_anim_counter = 0;
                }
                break;
            }
            default:
                deactivate(lane);
                return;
            }
        }
        if(lane.active && lane.wait == 0)
        {
            // Malformed command-only loop within one frame.
            deactivate(lane);
        }
    }

    PS4_M4A_SEQ_HOT void advance_pan_animation(lane_state& lane, bool note_start)
    {
        if(lane.kind != chip_audio_data::lane_kind::fm || lane.pan_anim_mode == 0)
        {
            return;
        }

        // StartPanAnim: mode 1 advances only at note starts; modes 2/3 reset
        // and then advance. DoPanAnim: only modes 2/3 continue between notes.
        if(note_start)
        {
            if(lane.pan_anim_mode >= 2)
            {
                lane.pan_anim_counter = lane.pan_anim_period;
                lane.pan_anim_index = 0;
            }
        }
        else if(lane.pan_anim_mode < 2)
        {
            return;
        }

        if(lane.pan_anim_counter == lane.pan_anim_period)
        {
            if(lane.pan_anim_steps < lane.pan_anim_index)
            {
                if(lane.pan_anim_mode == 2)
                {
                    return;
                }
                lane.pan_anim_index = 0;
            }
            lane.pan_anim_counter = 0;
            lane.pan_anim_index = static_cast<unsigned char>(lane.pan_anim_index + 1);
        }

        static constexpr unsigned char pattern1[] = {0x40, 0x80};
        static constexpr unsigned char pattern2[] = {0x40, 0xC0, 0x80};
        static constexpr unsigned char pattern3[] = {0xC0, 0x80, 0xC0, 0x40};
        const unsigned char* pattern = nullptr;
        unsigned pattern_length = 0;
        switch(lane.pan_anim_pattern)
        {
        case 1: pattern = pattern1; pattern_length = 2; break;
        case 2: pattern = pattern2; pattern_length = 3; break;
        case 3: pattern = pattern3; pattern_length = 4; break;
        default: return;
        }
        if(lane.pan_anim_index == 0)
        {
            lane.pan_anim_index = 1;
        }
        unsigned position = unsigned(lane.pan_anim_index - 1u);
        while(position >= pattern_length) position -= pattern_length;
        lane.pan = static_cast<unsigned char>((lane.pan & 0x37u) | pattern[position]);
        lane.pan_anim_counter = static_cast<unsigned char>(lane.pan_anim_counter + 1);
    }

    PS4_M4A_SEQ_HOT void advance_psg_envelope(lane_state& lane)
    {
        if(!lane.keyed || lane.psg_envelope == 0)
        {
            return;
        }
        unsigned int length = 0;
        const unsigned char* data = chip_audio_data::psg_envelope_data(lane.psg_envelope, length);
        if(!data || length == 0)
        {
            return;
        }
        for(int safety = 0; safety < 8; ++safety)
        {
            if(lane.psg_envelope_pos >= length)
            {
                lane.psg_envelope_pos = 0;
            }
            const unsigned char value = data[lane.psg_envelope_pos];
            if(value < 0x80)
            {
                if(unsigned(lane.psg_envelope_pos) + 1u < length)
                {
                    ++lane.psg_envelope_pos;
                }
                return;
            }
            if(value == 0x83)
            {
                lane.keyed = false;
                return;
            }
            if(value == 0x81)
            {
                return;
            }
            if(value == 0x80)
            {
                lane.psg_envelope_pos = 0;
                continue;
            }
            if(value == 0x82 && unsigned(lane.psg_envelope_pos) + 1u < length)
            {
                lane.psg_envelope_pos = data[lane.psg_envelope_pos + 1];
                continue;
            }
            return;
        }
    }

    int psg_envelope_delta(const lane_state& lane)
    {
        if(lane.psg_envelope == 0)
        {
            return 0;
        }
        unsigned int length = 0;
        const unsigned char* data = chip_audio_data::psg_envelope_data(lane.psg_envelope, length);
        if(!data || lane.psg_envelope_pos >= length)
        {
            return 0;
        }
        const unsigned char value = data[lane.psg_envelope_pos];
        return value < 0x80 ? int(value) : 0;
    }

    PS4_M4A_SEQ_HOT void advance_modulation(lane_state& lane)
    {
        if(!lane.modulation_enabled || !lane.keyed || lane.current_note < 0)
        {
            return;
        }
        if(lane.mod_delay_counter != 0)
        {
            --lane.mod_delay_counter;
            return;
        }
        if(lane.mod_speed_counter != 0)
        {
            --lane.mod_speed_counter;
            if(lane.mod_speed_counter != 0)
            {
                return;
            }
        }
        lane.mod_speed_counter = lane.mod_speed;
        if(lane.mod_step_counter == 0)
        {
            lane.mod_step_counter = lane.mod_half_steps;
            lane.mod_current_delta = static_cast<signed char>(-lane.mod_current_delta);
            return;
        }
        --lane.mod_step_counter;
        lane.mod_accumulator = static_cast<int16_t>(lane.mod_accumulator + lane.mod_current_delta);
        if(lane.kind == chip_audio_data::lane_kind::fm)
        {
            refresh_fm_increments(lane);
        }
        else if(lane.kind == chip_audio_data::lane_kind::psg)
        {
            lane.psg_increment = psg_effective_increment(lane);
        }
    }

    PS4_M4A_SEQ_HOT void update_source_frame()
    {
        if(!engine_playing || !current_song)
        {
            return;
        }

        // Exact DoTempo shape: every tempo_period frames it increments each
        // track's wait before the normal decrement, effectively skipping one
        // sequencer decrement on that frame.
        bool tempo_skip = false;
        if(tempo_period != 0)
        {
            if(tempo_counter > 0)
            {
                --tempo_counter;
            }
            if(tempo_counter == 0)
            {
                tempo_counter = tempo_period;
                tempo_skip = true;
            }
        }

        bool any_active = false;
        for(int index = 0; index < max_lanes; ++index)
        {
            lane_state& lane = lanes[index];
            if(!lane.active)
            {
                continue;
            }
            any_active = true;
            if(tempo_skip)
            {
                lane.wait = static_cast<unsigned char>(lane.wait + 1);
            }
            // Source Update*Track always executes SUBQ.B #1.  Conditional
            // decrementing makes a wrapped wait byte of 0 expire immediately
            // instead of becoming 255 for the normal 256-frame countdown.
            lane.wait = static_cast<unsigned char>(lane.wait - 1);
            if(lane.wait == 0)
            {
#if defined(MD2GBA_PS4_M4A_CLOCK_ONLY)
                // Diagnostic split after P98/P99: retain the once-per-frame
                // player clock/lane traversal but do not decode source events.
                // Reload one tick so the branch remains stable each frame.
                lane.wait = 1;
#else
                process_lane_event(lane);
                if(lane.active && lane.kind == chip_audio_data::lane_kind::fm)
                {
                    advance_pan_animation(lane, true);
                }
#endif
            }
            else
            {
                if(lane.note_stop_counter > 0)
                {
                    --lane.note_stop_counter;
                    if(lane.note_stop_counter == 0)
                    {
                        key_off(lane);
                    }
                }
                if(lane.kind == chip_audio_data::lane_kind::fm)
                {
                    advance_pan_animation(lane, false);
                }
                if(lane.kind == chip_audio_data::lane_kind::psg)
                {
                    advance_psg_envelope(lane);
                }
                if(lane.kind == chip_audio_data::lane_kind::fm ||
                    lane.kind == chip_audio_data::lane_kind::psg)
                {
                    advance_modulation(lane);
                }
            }
        }
        if(!any_active)
        {
            engine_playing = false;
        }
    }

    void advance_envelope(fm_operator_state& op)
    {
        if(use_source_ym_envelope())
        {
            if(op.stage == env_stage::off)
            {
                op.ym_attenuation = 1023;
                op.envelope = 0;
                return;
            }
            if(!ym_eg_tick)
            {
                return;
            }

            unsigned effective_rate = 0;
            switch(op.stage)
            {
            case env_stage::attack:
                effective_rate = op.attack_rate ?
                    32u + unsigned(op.attack_rate) * 2u + op.key_scale_rate : 0u;
                break;
            case env_stage::decay:
                effective_rate = op.decay_rate ?
                    32u + unsigned(op.decay_rate) * 2u + op.key_scale_rate : 0u;
                break;
            case env_stage::sustain:
                effective_rate = op.sustain_rate ?
                    32u + unsigned(op.sustain_rate) * 2u + op.key_scale_rate : 0u;
                break;
            case env_stage::release:
                effective_rate = 34u + unsigned(op.release_rate) * 4u + op.key_scale_rate;
                break;
            case env_stage::off:
                break;
            }

            const ym_rate_parameters params = ym_rate(effective_rate);
            const unsigned mask = (1u << params.shift) - 1u;
            if((unsigned(ym_eg_counter) & mask) != 0u)
            {
                return;
            }
            const unsigned cycle = (unsigned(ym_eg_counter) >> params.shift) & 7u;
            const unsigned increment = ym_eg_inc[params.selector][cycle];
            if(increment == 0u)
            {
                return;
            }

            int attenuation = int(op.ym_attenuation);
            switch(op.stage)
            {
            case env_stage::attack:
                // Hardware attack is exponential in attenuation space.
                attenuation += ((~attenuation) * int(increment)) >> 4;
                if(attenuation <= 0)
                {
                    attenuation = 0;
                    op.stage = op.sustain_level == 0 ? env_stage::sustain : env_stage::decay;
                }
                break;
            case env_stage::decay:
            {
                attenuation += int(increment);
                const int sustain = op.sustain_level == 15 ? 992 : int(op.sustain_level) * 32;
                if(attenuation >= sustain)
                {
                    attenuation = sustain;
                    op.stage = env_stage::sustain;
                }
                break;
            }
            case env_stage::sustain:
                attenuation += int(increment);
                if(attenuation > 1023)
                {
                    attenuation = 1023;
                }
                break;
            case env_stage::release:
                attenuation += int(increment);
                if(attenuation >= 1023)
                {
                    attenuation = 1023;
                    op.stage = env_stage::off;
                }
                break;
            case env_stage::off:
                attenuation = 1023;
                break;
            }
            op.ym_attenuation = static_cast<uint16_t>(attenuation);
            op.envelope = op.stage == env_stage::off ? 0u : ym_envelope_amplitude(op.ym_attenuation);
            return;
        }

        switch(op.stage)
        {
        case env_stage::off:
            op.envelope = 0;
            return;
        case env_stage::attack:
        {
            if(op.attack_rate == 0)
            {
                return;
            }
            const uint32_t raw_step = 1u + uint32_t(op.attack_rate) * uint32_t(op.attack_rate);
            const uint32_t step = (raw_step * reference_envelope_rate + mixer_rate - 1u) / mixer_rate;
            op.envelope += step ? step : 1u;
            if(op.envelope >= 65535u)
            {
                op.envelope = 65535u;
                op.stage = env_stage::decay;
            }
            return;
        }
        case env_stage::decay:
        {
            const uint32_t target = op.sustain_level >= 15 ? 0u : 65535u - uint32_t(op.sustain_level) * 4096u;
            const uint32_t raw_step = 1u + (uint32_t(op.decay_rate) * uint32_t(op.decay_rate) >> 2);
            const uint32_t step = (raw_step * reference_envelope_rate + mixer_rate - 1u) / mixer_rate;
            if(op.envelope > target + (step ? step : 1u))
            {
                op.envelope -= step ? step : 1u;
            }
            else
            {
                op.envelope = target;
                op.stage = env_stage::sustain;
            }
            return;
        }
        case env_stage::sustain:
            if(op.sustain_rate != 0 && op.envelope != 0)
            {
                const uint32_t raw_step = 1u + (uint32_t(op.sustain_rate) * uint32_t(op.sustain_rate) >> 4);
                const uint32_t step = (raw_step * reference_envelope_rate + mixer_rate - 1u) / mixer_rate;
                const uint32_t actual_step = step ? step : 1u;
                op.envelope = op.envelope > actual_step ? op.envelope - actual_step : 0;
                if(op.envelope == 0)
                {
                    op.stage = env_stage::off;
                }
            }
            return;
        case env_stage::release:
        {
            const uint32_t rate = 1u + unsigned(op.release_rate) * 2u;
            const uint32_t raw_step = rate * rate;
            const uint32_t step = (raw_step * reference_envelope_rate + mixer_rate - 1u) / mixer_rate;
            const uint32_t actual_step = step ? step : 1u;
            op.envelope = op.envelope > actual_step ? op.envelope - actual_step : 0;
            if(op.envelope == 0)
            {
                op.stage = env_stage::off;
            }
            return;
        }
        }
    }

    int operator_sample(fm_operator_state& op, uint32_t phase_mod, int amplitude_scale)
    {
        advance_envelope(op);
        op.phase += op.increment;
        const uint32_t modded = op.phase + phase_mod;
        const int wave = sine_table[modded >> 24];
        const int tl_scale = fm_tl_q7[op.hardware_total_level & 0x7F];
        const uint32_t env = op.envelope;
        const int amplitude = int((env * unsigned(tl_scale) * unsigned(clamp_int(amplitude_scale, 0, 127))) >> 22);
        return (wave * amplitude) >> 7;
    }

    int fm_sample(lane_state& lane)
    {
        if(lane.current_note < 0)
        {
            // Release tails remain audible after note-off/rest.
            bool tail = false;
            for(const auto& op : lane.operators)
            {
                tail = tail || op.stage != env_stage::off;
            }
            if(!tail)
            {
                return 0;
            }
        }

        // The source driver implements channel volume by rewriting carrier
        // YM2612 Total-Level registers. hardware_total_level therefore already
        // contains both patch TL and the last source-authorized channel-volume
        // refresh; no second PCM-domain channel fader belongs here.
        int scale[4] = {127, 127, 127, 127};

        auto& s1op = lane.operators[0];
        // Convert signed operator output to the 32-bit wrapping phase domain
        // without left-shifting negative signed integers (undefined C++).
        // Ooze patch 4 is the source-authored three-lane brass/guitar-like
        // final-boss lead.  The generic modulation calibration leaves that
        // specific patch much darker than its intended role, so give only that
        // patch two additional phase-modulation bits while preserving every
        // other reviewed song/patch.
        const int mod_shift = (current_music_id == 0xB4 && lane.instrument == 4) ? 25 : 23;
        const auto phase_mod = [](int sample_value, int shift) -> uint32_t
        {
            const int64_t scaled = int64_t(sample_value) * (int64_t(1) << shift);
            return static_cast<uint32_t>(scaled);
        };
        const uint32_t feedback_mod = phase_mod(
            int(s1op.previous_output) * int(lane.feedback), 20);
        const int s1 = operator_sample(s1op, feedback_mod, scale[0]);
        s1op.previous_output = static_cast<int16_t>(clamp_int(s1, -32768, 32767));
        int s2 = 0, s3 = 0, s4 = 0;
        int out = 0;
        switch(lane.algorithm & 7)
        {
        case 0: // OP1->OP2->OP3->OP4
            s2 = operator_sample(lane.operators[1], phase_mod(s1, mod_shift), scale[1]);
            s3 = operator_sample(lane.operators[2], phase_mod(s2, mod_shift), scale[2]);
            out = operator_sample(lane.operators[3], phase_mod(s3, mod_shift), scale[3]);
            break;
        case 1: // (OP1+OP2)->OP3->OP4
            s2 = operator_sample(lane.operators[1], 0, scale[1]);
            s3 = operator_sample(lane.operators[2], phase_mod(s1 + s2, mod_shift), scale[2]);
            out = operator_sample(lane.operators[3], phase_mod(s3, mod_shift), scale[3]);
            break;
        case 2: // (OP1 + OP2->OP3)->OP4
            s2 = operator_sample(lane.operators[1], 0, scale[1]);
            s3 = operator_sample(lane.operators[2], phase_mod(s2, mod_shift), scale[2]);
            out = operator_sample(lane.operators[3], phase_mod(s1 + s3, mod_shift), scale[3]);
            break;
        case 3: // (OP1->OP2 + OP3)->OP4
            s2 = operator_sample(lane.operators[1], phase_mod(s1, mod_shift), scale[1]);
            s3 = operator_sample(lane.operators[2], 0, scale[2]);
            out = operator_sample(lane.operators[3], phase_mod(s2 + s3, mod_shift), scale[3]);
            break;
        case 4: // OP1->OP2 + OP3->OP4
            s2 = operator_sample(lane.operators[1], phase_mod(s1, mod_shift), scale[1]);
            s3 = operator_sample(lane.operators[2], 0, scale[2]);
            s4 = operator_sample(lane.operators[3], phase_mod(s3, mod_shift), scale[3]);
            out = s2 + s4;
            break;
        case 5: // OP1 modulates OP2/OP3/OP4
            s2 = operator_sample(lane.operators[1], phase_mod(s1, mod_shift), scale[1]);
            s3 = operator_sample(lane.operators[2], phase_mod(s1, mod_shift), scale[2]);
            s4 = operator_sample(lane.operators[3], phase_mod(s1, mod_shift), scale[3]);
            out = s2 + s3 + s4;
            break;
        case 6: // OP1->OP2 + OP3 + OP4
            s2 = operator_sample(lane.operators[1], phase_mod(s1, mod_shift), scale[1]);
            s3 = operator_sample(lane.operators[2], 0, scale[2]);
            s4 = operator_sample(lane.operators[3], 0, scale[3]);
            out = s2 + s3 + s4;
            break;
        default:
            s2 = operator_sample(lane.operators[1], 0, scale[1]);
            s3 = operator_sample(lane.operators[2], 0, scale[2]);
            s4 = operator_sample(lane.operators[3], 0, scale[3]);
            out = s1 + s2 + s3 + s4;
            break;
        }
        return out >> 2;
    }

    int fm_multisample(lane_state& lane)
    {
        if(!lane.fm_zone)
        {
            // A dynamic/debug note outside the generated static coverage still
            // has a deterministic reference path instead of going silent.
            return fm_sample(lane);
        }

        bool tail = lane.current_note >= 0;
        const unsigned char carrier_mask = carrier_masks[lane.algorithm & 7];
        int carrier_sum = 0;
        int carrier_count = 0;
        for(int index = 0; index < fm_operators; ++index)
        {
            fm_operator_state& op = lane.operators[index];
            advance_envelope(op);
            if(op.stage != env_stage::off)
            {
                tail = true;
            }
            if(carrier_mask & (1u << index))
            {
                // Source FM channel volume is a YM2612 TL delta. Using a
                // linear scale left high-attenuation voices (notably Motabia
                // Town FM2 at 29) far too prominent.
                const int channel_scale = int(fm_tl_q7[lane.volume & 0x7F]);
                const int tl_scale = fm_tl_q7[op.total_level & 0x7F];
                int amplitude = int(op.envelope >> 9); // 0..127
                amplitude = (amplitude * tl_scale) / 127;
                amplitude = (amplitude * channel_scale) / 127;
                carrier_sum += amplitude;
                ++carrier_count;
            }
        }
        if(!tail || carrier_count == 0)
        {
            return 0;
        }

        lane.fm_wave_phase += lane.current_note < 0 ? 0u : fm_effective_increment(lane);

        // Six phase-normalized snapshots follow the real patch's attack, decay
        // and operator-detune phase drift. Runtime still performs only two
        // wavetable reads + interpolation per voice, so this remains PSC-like
        // in cost rather than becoming a live four-operator synth.
        unsigned stage = lane.fm_timbre_stage;
        if(stage >= fm_multisample_data::timbre_stages)
        {
            stage = fm_multisample_data::timbre_stages - 1;
        }
        const unsigned next_stage = stage + 1u < fm_multisample_data::timbre_stages ? stage + 1u : stage;
        const signed char* wave_a = fm_multisample_data::wave(*lane.fm_zone, stage);
        const signed char* wave_b = fm_multisample_data::wave(*lane.fm_zone, next_stage);

        const unsigned index = lane.fm_wave_phase >> 25;
        const unsigned next = (index + 1u) & 127u;
        const unsigned frac = (lane.fm_wave_phase >> 17) & 0xFFu;
        const int a0 = wave_a[index];
        const int a1 = wave_a[next];
        const int b0 = wave_b[index];
        const int b1 = wave_b[next];
        const int value_a = a0 + (((a1 - a0) * int(frac)) >> 8);
        const int value_b = b0 + (((b1 - b0) * int(frac)) >> 8);
        const int blend = int(lane.fm_timbre_blend >> 8);
        const int wave_value = value_a + (((value_b - value_a) * blend) >> 8);

        const int gain_a = int(fm_multisample_data::gain(*lane.fm_zone, stage));
        const int gain_b = int(fm_multisample_data::gain(*lane.fm_zone, next_stage));
        const int calibrated_gain = gain_a + (((gain_b - gain_a) * blend) >> 8);

        if(lane.keyed && stage + 1u < fm_multisample_data::timbre_stages)
        {
            // Snapshot times are 10,45,100,200,400,800 ms. The cheap Q16
            // steps below avoid divisions in the ARM7 inner audio loop.
            static constexpr unsigned short stage_step[5] = { 60, 38, 21, 11, 6 };
            const unsigned updated = unsigned(lane.fm_timbre_blend) + stage_step[stage];
            if(updated >= 65535u)
            {
                lane.fm_timbre_stage = static_cast<unsigned char>(stage + 1u);
                lane.fm_timbre_blend = 0;
            }
            else
            {
                lane.fm_timbre_blend = static_cast<unsigned short>(updated);
            }
        }

        const int amplitude = carrier_sum / carrier_count;
        const int base = (wave_value * amplitude) >> 7;
        return (base * calibrated_gain) >> 6;
    }

    int psg_sample(lane_state& lane)
    {
        if(!lane.keyed || lane.current_note < 0)
        {
            return 0;
        }
        const int attenuation = clamp_int(int(lane.volume & 0x0F) + psg_envelope_delta(lane), 0, 15);
        if(attenuation >= 15)
        {
            return 0;
        }
        // SN76489 attenuation is logarithmic (roughly 2 dB per step), not
        // linear.  The previous `(15-attenuation)*5` approximation made
        // Motabia Town's PSG lanes at source levels 8/9 around three times
        // too loud, producing the narrow "phone tone" that masked the FM
        // bed.  Keep a small integer table so the GBA runtime stays cheap.
        static constexpr unsigned char psg_level[16] = {
            80, 64, 51, 40, 32, 25, 20, 16, 13, 10, 8, 6, 5, 4, 3, 0
        };
        const int amplitude = psg_level[attenuation];
        if(lane.psg_noise)
        {
            const uint32_t old_phase = lane.psg_phase;
            uint32_t increment = lane.psg_increment;
            if(increment == 0)
            {
                static constexpr uint32_t noise_inc[4] = {
                    0x03745BF5u, 0x01BA2DFBu, 0x00DD16FDu, 0x009364A9u
                };
                increment = noise_inc[lane.psg_noise_control & 3];
            }
            lane.psg_phase += increment;
            if(lane.psg_phase < old_phase)
            {
                const bool white = (lane.psg_noise_control & 4) != 0;
                const unsigned feedback = white ? ((lane.noise_lfsr ^ (lane.noise_lfsr >> 1)) & 1u) : (lane.noise_lfsr & 1u);
                lane.noise_lfsr = static_cast<uint16_t>((lane.noise_lfsr >> 1) | (feedback << 14));
                if(lane.noise_lfsr == 0)
                {
                    lane.noise_lfsr = 0x4001;
                }
            }
            return (lane.noise_lfsr & 1) ? amplitude : -amplitude;
        }
        lane.psg_phase += lane.psg_increment;
        return (lane.psg_phase & 0x80000000u) ? amplitude : -amplitude;
    }

    int dac_sample_value(lane_state& lane)
    {
        if(!lane.keyed || !lane.dac_data || lane.dac_length == 0)
        {
            return 0;
        }
        unsigned int index = lane.dac_position >> 16;
        if(index >= lane.dac_length)
        {
            if(lane.dac_loop)
            {
                lane.dac_position = lane.dac_reverse ? ((lane.dac_length - 1u) << 16) : 0;
                index = lane.dac_position >> 16;
            }
            else
            {
                lane.keyed = false;
                return 0;
            }
        }
        const int value = lane.dac_data[index];
        if(lane.dac_reverse)
        {
            lane.dac_position = lane.dac_position > lane.dac_step ? lane.dac_position - lane.dac_step : (lane.dac_length << 16);
        }
        else
        {
            lane.dac_position += lane.dac_step;
        }
        // cfE3_DACVolCtrl: 0 disables DAC, positive selects the Z80
        // no-volume-control path (full scale), negative selects FC-scaled
        // volume.  Treating every nonzero value as scaled changes tracks that
        // deliberately bypass the volume table.
        const unsigned char volume_control = lane.dac_volume_control;
        if(volume_control == 0)
        {
            return 0;
        }
        if(volume_control & 0x80)
        {
            const int volume = clamp_int(lane.dac_volume, 0, 16);
            return (value * volume) >> 4;
        }
        return value;
    }

    void reset_lanes()
    {
        for(auto& lane : lanes)
        {
            lane = lane_state{};
        }
    }
}

bool play(unsigned char new_music_id, int volume_0_256, bool loop)
{
    const chip_audio_data::song_header* song = chip_audio_data::find_song(new_music_id);
    if(!song || song->lane_count == 0 || song->lane_count > max_lanes)
    {
        return false;
    }
    reset_lanes();
    current_song = song;
    current_music_id = new_music_id;
    master_volume = clamp_int(volume_0_256, 0, 256);
    allow_source_loops = loop;
    tempo_period = song->tempo;
    tempo_counter = song->tempo;
    ym_eg_phase = 0;
    ym_eg_counter = 0;
    ym_eg_tick = false;
    for(unsigned index = 0; index < song->lane_count; ++index)
    {
        lane_state& lane = lanes[index];
        const chip_audio_data::track_header& header = song->tracks[index];
        lane.active = header.address != 0;
        lane.kind = static_cast<chip_audio_data::lane_kind>(header.kind);
        lane.pc = header.address;
        lane.wait = 1;
        // ClearMusicTrks zeroes $F before PlayMusic sets only $E=1.
        lane.previous_duration = 0;
        lane.transpose = header.transpose;
        lane.volume = header.volume;
        lane.psg_envelope = header.initial_psg_envelope;
        lane.pan = 0xC0;
        if(lane.kind == chip_audio_data::lane_kind::fm)
        {
            load_fm_patch(lane, 0);
        }
    }
    engine_playing = true;
    return true;
}

void stop()
{
    engine_playing = false;
    current_music_id = 0;
    current_song = nullptr;
    reset_lanes();
}

bool playing()
{
    return engine_playing;
}

unsigned char music_id()
{
    return current_music_id;
}

void set_volume(int volume_0_256)
{
    master_volume = clamp_int(volume_0_256, 0, 256);
}

void set_fm_backend(fm_backend backend)
{
    selected_fm_backend = backend;
}

fm_backend current_fm_backend()
{
    return selected_fm_backend;
}

void mix_into_output(int8_t* left, int8_t* right, int sample_count)
{
    if(!engine_playing || !left || !right || sample_count <= 0)
    {
        return;
    }

    // The original sound driver advances once per VBlank. Maxmod generates one
    // audio block per bn::core::update(), so this is the natural native-port
    // equivalent of UpdateSound.
    update_source_frame();

    for(int sample = 0; sample < sample_count; ++sample)
    {
        int tonal_left = 0;
        int tonal_right = 0;
        int dac_left = 0;
        int dac_right = 0;
        for(auto& lane : lanes)
        {
#ifdef PS4GBA_DEBUG_SOLO_LANE
            const int debug_lane_index = int(&lane - lanes);
            if(debug_lane_index != PS4GBA_DEBUG_SOLO_LANE)
            {
                continue;
            }
#endif
            if(!lane.active && lane.kind != chip_audio_data::lane_kind::fm)
            {
                continue;
            }
            int voice = 0;
            if(lane.kind == chip_audio_data::lane_kind::fm)
            {
                voice = selected_fm_backend == fm_backend::multisample ?
                    fm_multisample(lane) : fm_sample(lane);
            }
            else if(lane.kind == chip_audio_data::lane_kind::psg)
            {
                voice = psg_sample(lane);
            }
            else
            {
                voice = dac_sample_value(lane);
            }

            // Mix balance derived from listening/stem comparison against the
            // retained four-operator reference.  The first multisample mixes
            // put the PSG far too far forward and left both the FM body and
            // DAC percussion too far back.  Keep the FM bed as the anchor,
            // tuck PSG behind it, and restore DAC transient weight.
            if(lane.kind == chip_audio_data::lane_kind::fm)
            {
                // Color each FM voice before the final bus gain.  Doing this
                // per voice preserves the changing harmonic density of layered
                // patches instead of merely clipping the whole song louder.
                voice = fm_output_color(voice);
                voice = (voice * 1536) >> 8; // v13: approved punchier 6.0x FM baseline
                const int lane_index = int(&lane - lanes);

                // Targeted soundtrack corrections. These are deliberately
                // source-role trims, not global synthesis changes; reviewed
                // songs keep independent balance/headroom calibration.
                if(current_music_id == 0x8F) // Meet Them Head On
                {
                    // The source deliberately builds an audible stereo echo:
                    // FM1 is hard-right and FM5 is a near-identical hard-left
                    // phrase delayed by about 70 ms.  A contemporary MIDI
                    // transcription even labels the pair Electric Guitar /
                    // Electric Guitar Echo.  Earlier mixes let the central
                    // bass/mid lane mask that source-authored Haas layer.
                    if(lane_index == 2)
                    {
                        voice = (voice * 166) >> 8; // -3.8 dB: clear room for guitar/echo pair
                    }
                    else if(lane_index == 1)
                    {
                        voice = (voice * 416) >> 8; // +4.2 dB: main guitar layer
                    }
                    else if(lane_index == 5)
                    {
                        voice = (voice * 436) >> 8; // +4.6 dB: delayed echo copy remains audible
                    }
                    else if(lane_index == 3)
                    {
                        voice = (voice * 294) >> 8; // +1.2 dB: secondary harmonic layer
                    }
                }
                else if(current_music_id == 0xAC && lane_index == 4 && lane.instrument == 7)
                {
                    // End of the Millennium's opening bass is patch 7 on FM4.
                    // Do not transpose the source note.  Reinforce only its
                    // low-frequency body with a one-pole low shelf derived
                    // from the actual four-op voice, keeping upper harmonics
                    // and the composition intact.  alpha=1/16 puts the shelf
                    // transition in the low hundreds of Hz at 31.536 kHz.
                    lane.low_body_state += (voice - lane.low_body_state) >> 4;
                    // v17: a deliberately small step beyond v16 standard deep
                    // (~+0.5 dB at the settled fundamental, not EXTRA_DEEP).
                    voice += lane.low_body_state + (lane.low_body_state >> 3);

                    // Restore a little air after choosing the less-crunchy warm
                    // FM color curve.  alpha=1/2 puts this one-pole crossover in
                    // the several-kHz region at 31.536 kHz; 1/8 of the residual
                    // is roughly a 1 dB high shelf rather than a treble spike.
                    lane.high_air_state += (voice - lane.high_air_state) >> 1;
                    voice += (voice - lane.high_air_state) >> 3;
                }

                if(current_music_id == 0xB4 && lane.instrument == 4 &&
                   (lane_index == 1 || lane_index == 3 || lane_index == 5))
                {
                    // Ooze: patch 4 is the three-voice final-boss exclamation
                    // heard clearly from about 1:04 onward (including the
                    // user's 1:10-1:13 reference).  Restore its foreground role
                    // without touching the rapid patch-1/3 figures underneath.
                    voice = (voice * 406) >> 8; // +4.0 dB source-role calibration
                }
                else if(current_music_id == 0x88 && lane_index == 5)
                {
                    // Behind's always-on patch-4 foundation was consuming most
                    // of the tonal-bus headroom.  Ease it back instead of
                    // overdriving the changing synth voices into the limiter.
                    voice = (voice * 216) >> 8; // -1.48 dB foundation trim
                }

                // Motabia Town's FM4/patch-2 ostinato is intentionally bright,
                // but the reference four-op render leaves it slightly too far
                // forward. Apply a small mastering trim only to that source
                // voice; do not globally dull the PSG or the corrected FM bed.
                if(current_music_id == 0x84 && lane_index == 4 && lane.instrument == 2)
                {
                    voice = (voice * 218) >> 8; // -1.4 dB / 0.852x
                }
            }
            else if(lane.kind == chip_audio_data::lane_kind::psg)
            {
                voice = (voice * 88) >> 8; // v13: 0.344x; approved punchier PSG balance
            }
            else
            {
                // Ooze's opening bass/percussion body is intentionally left
                // untouched. Only source DAC sample 1 (the bright snare-like
                // hit later in the track) is trimmed.
                if(current_music_id == 0xB4 && lane.dac_sound == 1)
                {
                    voice = (voice * 216) >> 8; // -1.5 dB snare trim
                }
                // v17: apply song-specific DAC-only headroom after restoring
                // source-accurate duration-only retriggers.  Meet Them Head On's
                // old +1.5 dB local lift is folded into this table so there is
                // one auditable gain stage for the corrected percussion bus.
                voice = (voice * song_dac_gain_q8(current_music_id)) >> 8;
                voice = (voice * 1344) >> 8; // v13: 5.25x baseline DAC impact
            }

            // YM2612 B4 uses bit7=left, bit6=right. PSG is mono in the source
            // console and is therefore sent to both sides here.
            if(lane.kind == chip_audio_data::lane_kind::psg)
            {
                tonal_left += voice;
                tonal_right += voice;
            }
            else if(lane.kind == chip_audio_data::lane_kind::dac)
            {
                // Keep percussion on a separate transient bus. Limiting DAC
                // together with the sustained FM bed made dense sections lose
                // steam exactly when the drums entered.
                if(lane.pan & 0x80) dac_left += voice;
                if(lane.pan & 0x40) dac_right += voice;
            }
            else
            {
                if(lane.pan & 0x80) tonal_left += voice;
                if(lane.pan & 0x40) tonal_right += voice;
            }
        }
        int mastered_volume = (master_volume * song_master_q8(current_music_id, selected_fm_backend)) >> 8;
        mastered_volume = (mastered_volume * source_eg_headroom_q8(current_music_id, selected_fm_backend)) >> 8;
        tonal_left = soft_limit_music((tonal_left * mastered_volume) >> 8);
        tonal_right = soft_limit_music((tonal_right * mastered_volume) >> 8);
        dac_left = (dac_left * mastered_volume) >> 8;
        dac_right = (dac_right * mastered_volume) >> 8;
        const int music_left = soft_limit_music_peak(tonal_left + dac_left);
        const int music_right = soft_limit_music_peak(tonal_right + dac_right);
        left[sample] = saturate8(int(left[sample]) + music_left);
        right[sample] = saturate8(int(right[sample]) + music_right);

        // The YM2612 updates its EG after producing the current chip sample.
        // Queue the corresponding source EG tick for the next GBA sample.
        advance_ym_eg_clock();
    }
}

#undef PS4_M4A_SEQ_HOT

#if defined(PS4_FALLBACK_MIX_IWRAM)
#define PS4_M4A_MIX_HOT __attribute__((section(".iwram.audio"),noinline))
#else
#define PS4_M4A_MIX_HOT
#endif

#if defined(MD2GBA_PS4_V19_FULL_MIX)
    // P134R: use the recovered v19 exact-YM/mastering profile in the low-cost
    // PSC/Maxmod mono path. Invariant song/lane gains stay outside the 224-sample
    // hot loop so the full 52-song catalog does not restore the old live-synth cost.
    int v19_fast_master_q8(unsigned char music_id)
    {
        const int a = song_master_q8(music_id, fm_backend::multisample);
        const int b = source_eg_headroom_q8(music_id, fm_backend::multisample);
        int q8 = (a * b + 128) >> 8;
        // P134R: compensate only for the retained low-cost 13.379-kHz mono
        // transport.  Values are derived from full-track RMS against the 52
        // final v19 masters in the recovered import archive; they are not game
        // logic and are evaluated once per video frame outside the hot loop.
        static constexpr unsigned short fast_to_v19_q8[52] = {
            238,102,134,255,165,189,270,156,170,164,224,163,200,
            151,219,158,204,193,186,151,195,235,254,219,234,210,
            218,196,211,231,184,214,194,192,228,207,199,171,212,
            225,218,215,190,190,216,206,289,205,207,142,241,248
        };
        if(music_id >= 0x81u && music_id <= 0xB4u)
            q8 = (q8 * int(fast_to_v19_q8[unsigned(music_id - 0x81u)]) + 128) >> 8;
        return q8;
    }

    int v19_fast_limit(int value)
    {
        // Division-free approximation to the reference tonal + peak soft knees.
        // Linear to 108, then progressively compress to +/-126.
        const bool neg = value < 0;
        unsigned m = unsigned(neg ? -value : value);
        if(m > 108u)
        {
            if(m <= 126u) m = 108u + ((m - 108u) >> 1);
            else if(m <= 162u) m = 117u + ((m - 126u) >> 2);
            else m = 126u;
        }
        if(m > 126u) m = 126u;
        return neg ? -int(m) : int(m);
    }

    int v19_fast_fm_gain_q8(unsigned music_id, unsigned lane_index, unsigned instrument)
    {
        int q8 = 1536; // v19 production FM bus: 6.0x.
        if(music_id == 0x88u) q8 = (q8 * 288) >> 8; // exact-YM Behind synth-bed lift
        if(music_id == 0x8Fu)
        {
            if(lane_index == 2u) q8 = (q8 * 166) >> 8;
            else if(lane_index == 1u) q8 = (q8 * 416) >> 8;
            else if(lane_index == 5u) q8 = (q8 * 436) >> 8;
            else if(lane_index == 3u) q8 = (q8 * 294) >> 8;
        }
        else if(music_id == 0xB4u && instrument == 4u &&
                (lane_index == 1u || lane_index == 3u || lane_index == 5u))
            q8 = (q8 * 406) >> 8;
        else if(music_id == 0x88u && lane_index == 5u)
            q8 = (q8 * 216) >> 8;
        if(music_id == 0x84u && lane_index == 4u && instrument == 2u)
            q8 = (q8 * 218) >> 8;
        return q8;
    }

    // Resolve v19 song/lane mastering once per video frame from cold ROM.
    // The hot IWRAM mixer then performs only table loads and multiplies.
    static unsigned short v19_frame_fm_gain_q8[max_lanes];
    static unsigned short v19_frame_dac_gain_q8[max_lanes];
    static int v19_frame_master_q8 = 256;

    __attribute__((noinline)) void v19_prepare_fast_frame()
    {
        const unsigned music_id = current_music_id;
        v19_frame_master_q8 = v19_fast_master_q8(static_cast<unsigned char>(music_id));
        const int base_dac = (1344 * song_dac_gain_q8(static_cast<unsigned char>(music_id))) >> 8;
        unsigned count = current_song ? current_song->lane_count : 0u;
        if(count > unsigned(max_lanes)) count = unsigned(max_lanes);
        for(unsigned i = 0; i < count; ++i)
        {
            const lane_state& lane = lanes[i];
            // The fast player is mono, but PSIV FM/DAC is source-stereo.  A
            // mono downmix must average hard-left/right lanes rather than sum
            // every panned voice at full scale.  Resolve the YM B4 pan bits
            // here so the 224-sample hot loop stays branch-free.
            const unsigned pan_bits = unsigned(lane.pan & 0xC0u);
            const int pan_q8 = pan_bits == 0xC0u ? 256 : (pan_bits ? 128 : 0);
            int fg = lane.kind == chip_audio_data::lane_kind::fm ?
                v19_fast_fm_gain_q8(music_id,i,lane.instrument) : 1536;
            fg = (fg * pan_q8) >> 8;
            v19_frame_fm_gain_q8[i] = static_cast<unsigned short>(fg);
            int dg = base_dac;
            if(music_id == 0xB4u && lane.kind == chip_audio_data::lane_kind::dac && lane.dac_sound == 1u)
                dg = (dg * 216) >> 8;
            dg = (dg * pan_q8) >> 8;
            v19_frame_dac_gain_q8[i] = static_cast<unsigned short>(dg);
        }
    }
#endif

static inline bool p100_render_lane_enabled(chip_audio_data::lane_kind kind)
{
#if defined(MD2GBA_PS4_M4A_FM_ONLY)
    return kind == chip_audio_data::lane_kind::fm;
#elif defined(MD2GBA_PS4_M4A_PSG_ONLY)
    return kind == chip_audio_data::lane_kind::psg;
#elif defined(MD2GBA_PS4_M4A_DAC_ONLY)
    return kind == chip_audio_data::lane_kind::dac;
#else
    (void)kind;
    return true;
#endif
}

PS4_M4A_MIX_HOT void mix_psc_mono(int8_t* mono, int sample_count)
{
    if(!mono || sample_count <= 0)
    {
        return;
    }
    if(!engine_playing || !current_song)
    {
        for(int i = 0; i < sample_count; ++i) mono[i] = 0;
        return;
    }

    // MusicPlayer2000 advances player/control state once per video frame.
    // Keep PSIV's source-authoritative event semantics, but lower the render
    // side into PSC-like channel state before entering the sample loop.
    update_source_frame();
#if defined(MD2GBA_PS4_V19_FULL_MIX)
    v19_prepare_fast_frame();
#endif

#if defined(MD2GBA_PS4_M4A_SEQ_ONLY)
    // P98 isolation: unlike P97 SILENT this deliberately runs the complete
    // native sequencer/control update and suppresses only PCM voice rendering.
    // It distinguishes target-side event/player faults from sample-loop faults.
    for(int i = 0; i < sample_count; ++i) mono[i] = 0;
    return;
#else
    struct render_fm_state
    {
        lane_state* lane;
#if defined(MD2GBA_PS4_MAXMOD_MODE_A)
        const signed char* wave;
        uint32_t phase;
        uint32_t increment;
        int scale_q13;
        unsigned char stage;
#else
        const signed char* wave_a;
        const signed char* wave_b;
        uint32_t increment;
# if defined(MD2GBA_PS4_M4A_MP2K_FRAME_WAVE)
        uint32_t phase;
# endif
        unsigned char gain_a;
        unsigned char gain_b;
        unsigned char stage;
# if defined(MD2GBA_PS4_M4A_MP2K_FRAME_WAVE)
        signed short frame_wave[128];
# endif
#endif
    };
    render_fm_state fm_render[max_lanes];
    unsigned char fm_slot[max_lanes] = {};
    signed char psg_amplitude[max_lanes] = {};
    unsigned fm_count = 0;
    unsigned lane_count = current_song->lane_count;
    if(lane_count > unsigned(max_lanes)) lane_count = unsigned(max_lanes);
#if defined(MD2GBA_PS4_M4A_LIMIT_RENDER_LANES)
    if(lane_count > unsigned(MD2GBA_PS4_M4A_LIMIT_RENDER_LANES))
        lane_count = unsigned(MD2GBA_PS4_M4A_LIMIT_RENDER_LANES);
#endif

    // Resolve the immutable-per-frame WaveData-style fields once.  P96 did
    // four out-of-line wave/gain lookups plus a pitch-table lookup for every
    // FM voice sample. PSC/MP2K keeps these in channel state instead.
    for(unsigned index = 0; index < lane_count; ++index)
    {
        lane_state& lane = lanes[index];
        if(!p100_render_lane_enabled(lane.kind) ||
           !lane.active || lane.kind != chip_audio_data::lane_kind::fm ||
           !lane.fm_zone || lane.current_note < 0 || !lane.keyed)
        {
            continue;
        }
        unsigned stage = lane.fm_timbre_stage;
        if(stage >= fm_multisample_data::timbre_stages)
            stage = fm_multisample_data::timbre_stages - 1;
        const unsigned next_stage = stage + 1u < fm_multisample_data::timbre_stages ? stage + 1u : stage;
        render_fm_state& r = fm_render[fm_count];
        r.lane = &lane;
#if defined(MD2GBA_PS4_MAXMOD_MODE_A)
        const unsigned selected_stage =
            (unsigned(lane.fm_timbre_blend) >= 0x8000u && next_stage != stage) ? next_stage : stage;
        r.wave = fm_multisample_data::wave(*lane.fm_zone, selected_stage);
        r.phase = lane.fm_wave_phase;
        r.increment = chip_audio_data::fm_phase_increment(static_cast<unsigned char>(lane.current_note));
        r.scale_q13 = int(fm_multisample_data::gain(*lane.fm_zone, selected_stage)) *
                      int(fm_tl_q7[lane.volume & 0x7Fu]);
#if defined(MD2GBA_PS4_V19_FULL_MIX)
        r.scale_q13 = (r.scale_q13 * int(v19_frame_fm_gain_q8[index])) >> 8;
#endif
        r.stage = static_cast<unsigned char>(stage);
#else
        r.wave_a = fm_multisample_data::wave(*lane.fm_zone, stage);
        r.wave_b = fm_multisample_data::wave(*lane.fm_zone, next_stage);
        r.increment = chip_audio_data::fm_phase_increment(static_cast<unsigned char>(lane.current_note));
# if defined(MD2GBA_PS4_M4A_MP2K_FRAME_WAVE)
        r.phase = lane.fm_wave_phase;
# endif
        r.gain_a = fm_multisample_data::gain(*lane.fm_zone, stage);
        r.gain_b = fm_multisample_data::gain(*lane.fm_zone, next_stage);
        r.stage = static_cast<unsigned char>(stage);
# if defined(MD2GBA_PS4_M4A_MP2K_FRAME_WAVE)
        const unsigned blend = unsigned(lane.fm_timbre_blend >> 8);
        const int zg = int(r.gain_a) + (((int(r.gain_b) - int(r.gain_a)) * int(blend)) >> 8);
        const int vg = int(fm_tl_q7[lane.volume & 0x7Fu]);
        for(unsigned wi = 0; wi < 128u; ++wi)
        {
            const int wa = int(r.wave_a[wi]);
            const int wb = int(r.wave_b[wi]);
            const int wv = wa + (((wb - wa) * int(blend)) >> 8);
            r.frame_wave[wi] = static_cast<signed short>((((wv * vg) >> 7) * zg) >> 6);
        }
# endif
#endif
        fm_slot[index] = static_cast<unsigned char>(fm_count + 1u);
        ++fm_count;
    }

    // PSG envelope position/volume is frame state, not sample state. Resolve
    // its logarithmic amplitude once just like an MP2K channel volume, rather
    // than re-reading envelope metadata for every output sample.
    static constexpr unsigned char psg_level[16] = {
        80, 64, 51, 40, 32, 25, 20, 16, 13, 10, 8, 6, 5, 4, 3, 0
    };
    for(unsigned index = 0; index < lane_count; ++index)
    {
        lane_state& lane = lanes[index];
        if(!p100_render_lane_enabled(lane.kind) ||
           !lane.active || lane.kind != chip_audio_data::lane_kind::psg ||
           !lane.keyed || lane.current_note < 0) continue;
        const int attenuation = clamp_int(int(lane.volume & 0x0F) + psg_envelope_delta(lane), 0, 15);
        int psg_amp = int(psg_level[attenuation]);
#if defined(MD2GBA_PS4_V19_FULL_MIX)
        // P104 subsequently applies 3/4.  117/256 here yields the v19
        // ~0.344x PSG bus relative to the source sample.
        psg_amp = (psg_amp * 117) >> 8;
#endif
        psg_amplitude[index] = static_cast<signed char>(psg_amp);
    }

#if defined(MD2GBA_PS4_MAXMOD_CHANNEL_MAJOR) && defined(MD2GBA_PS4_MAXMOD_MODE_A)
    // Maxmod GBA Mode-A style: process one active channel across the whole
    // frame-sized mixing segment, accumulating into a hot IWRAM/stack buffer.
    // This removes lane-kind/activity branches from the 224-sample inner loop
    // and mirrors the channel-major shape of Maxmod's GBA mixer.
    if(sample_count > 224) sample_count = 224;
    signed short maxmod_mix[224];
    for(int sample = 0; sample < sample_count; ++sample) maxmod_mix[sample] = 0;

    prof_fm += fm_count;
    for(unsigned i = 0; i < fm_count; ++i)
    {
        render_fm_state& r = fm_render[i];
        uint32_t phase = r.phase;
        const uint32_t increment = r.increment;
        const signed char* wave = r.wave;
        const int scale = r.scale_q13;
        for(int sample = 0; sample < sample_count; ++sample)
        {
            phase += increment;
            maxmod_mix[sample] = static_cast<signed short>(
                int(maxmod_mix[sample]) + ((int(wave[phase >> 25]) * scale) >> 13));
        }
        r.phase = phase;
    }

    for(unsigned index = 0; index < lane_count; ++index)
    {
        lane_state& lane = lanes[index];
        if(!lane.active || !p100_render_lane_enabled(lane.kind)) continue;
        if(lane.kind == chip_audio_data::lane_kind::psg)
        {
            const int amplitude = int(psg_amplitude[index]);
            if(amplitude == 0) continue;
            prof_psg++; if(lane.psg_noise) prof_noise++;
            uint32_t phase = lane.psg_phase;
            uint16_t noise_lfsr = lane.noise_lfsr;
            uint32_t increment = lane.psg_increment;
            const bool noise = lane.psg_noise != 0;
            if(noise && increment == 0)
            {
                static constexpr uint32_t noise_inc[4] = {
                    0x03745BF5u, 0x01BA2DFBu, 0x00DD16FDu, 0x009364A9u
                };
                increment = noise_inc[lane.psg_noise_control & 3];
            }
            const bool white = (lane.psg_noise_control & 4) != 0;
            for(int sample = 0; sample < sample_count; ++sample)
            {
                int voice;
                if(noise)
                {
                    const uint32_t old_phase = phase;
                    phase += increment;
                    if(phase < old_phase)
                    {
                        const unsigned feedback = white ?
                            ((noise_lfsr ^ (noise_lfsr >> 1)) & 1u) : (noise_lfsr & 1u);
                        noise_lfsr = static_cast<uint16_t>((noise_lfsr >> 1) | (feedback << 14));
                        if(noise_lfsr == 0) noise_lfsr = 0x4001;
                    }
                    voice = (noise_lfsr & 1) ? amplitude : -amplitude;
                }
                else
                {
                    phase += increment;
                    voice = (phase & 0x80000000u) ? amplitude : -amplitude;
                }
                maxmod_mix[sample] = static_cast<signed short>(
                    int(maxmod_mix[sample]) + ((voice * 3) >> 2));
            }
            lane.psg_phase = phase;
            lane.noise_lfsr = noise_lfsr;
        }
        else if(lane.kind == chip_audio_data::lane_kind::dac)
        {
            prof_dac++;
#if defined(MD2GBA_PS4_V19_FULL_MIX)
            const int dac_gain_q8 = int(v19_frame_dac_gain_q8[index]);
            for(int sample = 0; sample < sample_count; ++sample)
            {
                const int d = (dac_sample_value(lane) * dac_gain_q8) >> 8;
                maxmod_mix[sample] = static_cast<signed short>(int(maxmod_mix[sample]) + d);
            }
#else
            for(int sample = 0; sample < sample_count; ++sample)
                maxmod_mix[sample] = static_cast<signed short>(
                    int(maxmod_mix[sample]) + dac_sample_value(lane));
#endif
        }
    }

#if defined(MD2GBA_PS4_V19_FULL_MIX)
    const int v19_master = v19_frame_master_q8;
#endif
    for(int sample = 0; sample < sample_count; ++sample)
    {
        int mixed = (int(maxmod_mix[sample]) * master_volume) >> 8;
#if defined(MD2GBA_PS4_V19_FULL_MIX)
        mixed = (mixed * v19_master) >> 8;
        mono[sample] = static_cast<int8_t>(v19_fast_limit(mixed));
#else
        mono[sample] = saturate8(mixed);
#endif
    }
#else
    for(int sample = 0; sample < sample_count; ++sample)
    {
        int mixed = 0;
        for(unsigned index = 0; index < lane_count; ++index)
        {
            lane_state& lane = lanes[index];
            if(!lane.active || !p100_render_lane_enabled(lane.kind)) continue;

            int voice = 0;
            if(lane.kind == chip_audio_data::lane_kind::fm)
            {
                const unsigned slot_plus_one = fm_slot[index];
                if(!slot_plus_one) continue;
                render_fm_state& r = fm_render[slot_plus_one - 1u];
#if defined(MD2GBA_PS4_MAXMOD_MODE_A)
                r.phase += r.increment;
                const unsigned wi = r.phase >> 25;
                voice = (int(r.wave[wi]) * r.scale_q13) >> 13;
#elif defined(MD2GBA_PS4_M4A_MP2K_FRAME_WAVE)
                r.phase += r.increment;
                const unsigned wi = r.phase >> 25;
# if defined(MD2GBA_PS4_M4A_MP2K_NEAREST)
                voice = int(r.frame_wave[wi]);
# else
                const unsigned wn = (wi + 1u) & 127u;
                const unsigned frac = (r.phase >> 17) & 0xFFu;
                const int v0 = int(r.frame_wave[wi]);
                voice = v0 + (((int(r.frame_wave[wn]) - v0) * int(frac)) >> 8);
# endif
#else
                lane.fm_wave_phase += r.increment;
                const unsigned wi = lane.fm_wave_phase >> 25;
                const unsigned wn = (wi + 1u) & 127u;
                const unsigned frac = (lane.fm_wave_phase >> 17) & 0xFFu;
                const int va0 = int(r.wave_a[wi]);
                const int vb0 = int(r.wave_b[wi]);
                const int va = va0 + (((int(r.wave_a[wn]) - va0) * int(frac)) >> 8);
                const int vb = vb0 + (((int(r.wave_b[wn]) - vb0) * int(frac)) >> 8);
                const unsigned blend = unsigned(lane.fm_timbre_blend >> 8);
                const int wv = va + (((vb - va) * int(blend)) >> 8);
                const int zg = int(r.gain_a) + (((int(r.gain_b) - int(r.gain_a)) * int(blend)) >> 8);
                const int vg = int(fm_tl_q7[lane.volume & 0x7Fu]);
                voice = (wv * vg) >> 7;
                voice = (voice * zg) >> 6;

                if(lane.keyed && unsigned(r.stage) + 1u < fm_multisample_data::timbre_stages)
                {
                    static constexpr unsigned short stage_step[5] = {142, 90, 50, 26, 14};
                    const unsigned u = unsigned(lane.fm_timbre_blend) + stage_step[r.stage];
                    if(u >= 65535u)
                    {
                        const unsigned stage = unsigned(r.stage) + 1u;
                        const unsigned next_stage = stage + 1u < fm_multisample_data::timbre_stages ? stage + 1u : stage;
                        lane.fm_timbre_stage = static_cast<unsigned char>(stage);
                        lane.fm_timbre_blend = 0;
                        r.stage = static_cast<unsigned char>(stage);
                        r.wave_a = fm_multisample_data::wave(*lane.fm_zone, stage);
                        r.wave_b = fm_multisample_data::wave(*lane.fm_zone, next_stage);
                        r.gain_a = fm_multisample_data::gain(*lane.fm_zone, stage);
                        r.gain_b = fm_multisample_data::gain(*lane.fm_zone, next_stage);
                    }
                    else lane.fm_timbre_blend = static_cast<unsigned short>(u);
                }
#endif
            }
            else if(lane.kind == chip_audio_data::lane_kind::psg)
            {
                const int amplitude = int(psg_amplitude[index]);
                if(amplitude == 0) continue;
                if(lane.psg_noise)
                {
                    const uint32_t old_phase = lane.psg_phase;
                    uint32_t increment = lane.psg_increment;
                    if(increment == 0)
                    {
                        static constexpr uint32_t noise_inc[4] = {
                            0x03745BF5u, 0x01BA2DFBu, 0x00DD16FDu, 0x009364A9u
                        };
                        increment = noise_inc[lane.psg_noise_control & 3];
                    }
                    lane.psg_phase += increment;
                    if(lane.psg_phase < old_phase)
                    {
                        const bool white = (lane.psg_noise_control & 4) != 0;
                        const unsigned feedback = white ?
                            ((lane.noise_lfsr ^ (lane.noise_lfsr >> 1)) & 1u) : (lane.noise_lfsr & 1u);
                        lane.noise_lfsr = static_cast<uint16_t>((lane.noise_lfsr >> 1) | (feedback << 14));
                        if(lane.noise_lfsr == 0) lane.noise_lfsr = 0x4001;
                    }
                    voice = (lane.noise_lfsr & 1) ? amplitude : -amplitude;
                }
                else
                {
                    lane.psg_phase += lane.psg_increment;
                    voice = (lane.psg_phase & 0x80000000u) ? amplitude : -amplitude;
                }
                voice = (voice * 3) >> 2;
            }
            else
            {
                voice = dac_sample_value(lane);
            }
            mixed += voice;
        }

        mixed = (mixed * master_volume) >> 8;
        mono[sample] = saturate8(mixed);
    }
#endif
#if defined(MD2GBA_PS4_MAXMOD_MODE_A) || defined(MD2GBA_PS4_M4A_MP2K_FRAME_WAVE)
    // Commit channel phase and timbre envelope once per block.  The standard
    // GBA player keeps stable channel state outside its tight PCM inner loop;
    // doing the same avoids repeated EWRAM state writes for every sample.
    static constexpr unsigned short stage_step[5] = {142, 90, 50, 26, 14};
    for(unsigned i = 0; i < fm_count; ++i)
    {
        render_fm_state& r = fm_render[i];
        lane_state& lane = *r.lane;
        lane.fm_wave_phase = r.phase;
        if(lane.keyed && unsigned(r.stage) + 1u < fm_multisample_data::timbre_stages)
        {
            unsigned u = unsigned(lane.fm_timbre_blend) + unsigned(stage_step[r.stage]) * unsigned(sample_count);
            if(u >= 65535u)
            {
                lane.fm_timbre_stage = static_cast<unsigned char>(unsigned(r.stage) + 1u);
                u -= 65535u;
            }
            lane.fm_timbre_blend = static_cast<unsigned short>(u > 65534u ? 65534u : u);
        }
    }
#endif
#endif
}

#undef PS4_M4A_MIX_HOT

void transaction_discard()
{
    if(transaction_pending)
    {
        restore_transaction_state(transaction_saved);
        transaction_pending=false;
    }
}

bool transaction_play(unsigned char id)
{
    transaction_discard();
    set_volume(224);
    set_fm_backend(fm_backend::multisample);
    return play(id,224,true);
}

void transaction_stop()
{
    transaction_discard();
    stop();
}

void transaction_prepare_mono(int8_t* mono,int sample_count)
{
    if(transaction_pending) return;
    save_transaction_state(transaction_saved);
    mix_psc_mono(mono,sample_count);
    transaction_pending=true;
}

void transaction_commit()
{
    transaction_pending=false;
}

bool transaction_pending_block() { return transaction_pending; }

}
