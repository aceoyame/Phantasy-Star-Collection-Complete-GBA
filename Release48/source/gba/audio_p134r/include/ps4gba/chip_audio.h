#ifndef PS4GBA_CHIP_AUDIO_H
#define PS4GBA_CHIP_AUDIO_H

#include <stdint.h>

namespace ps4gba::chip_audio
{
    enum class fm_backend : unsigned char
    {
        multisample = 0,
        four_operator_reference = 1,
    };

    // Native GBA reimplementation of the PSIV music driver + sound chips.
    // It executes the original PSIV music bytecode, but never executes 68000/Z80
    // machine code. The PSC/M4A target uses the mono Direct Sound entry below.
    [[nodiscard]] bool play(unsigned char music_id, int volume_0_256 = 256, bool loop = true);
    void stop();
    [[nodiscard]] bool playing();
    [[nodiscard]] unsigned char music_id();
    void set_volume(int volume_0_256);
    // Source SFX steals specific YM/PSG hardware channels while it plays.
    // Bits use generated song lane order: 0=DAC, 1..5=FM1..FM5, 6..8=PSG1..PSG3.
    void set_mute_mask(uint16_t lane_mask);

    // Production now defaults to the four-operator backend because listening
    // tests showed that multisampling can lose characteristic PSIV FM timbres.
    // The PSC-inspired multisample backend remains available as a low-CPU
    // fallback and A/B diagnostic path.
    void set_fm_backend(fm_backend backend);
    [[nodiscard]] fm_backend current_fm_backend();

    // left/right are signed 8-bit reference output buffers for one rendered
    // frame. Existing samples are preserved with saturating addition.
    void mix_into_output(int8_t* left, int8_t* right, int sample_count);

    // PSC/MusicPlayer2000-style mono mixer: advance the source sequence once
    // for the physical video frame and render one signed 8-bit Direct Sound
    // slice. This is the normal P93 profile path; it deliberately avoids the
    // live four-operator YM backend and all song-specific mastering layers.
    void mix_psc_mono(int8_t* mono, int sample_count);

    // Transactional wrapper used by PSCSTYLE's speculative audio preparation.
    bool transaction_play(unsigned char music_id);
    void transaction_stop();
    void transaction_prepare_mono(int8_t* mono, int sample_count);
    void transaction_commit();
    void transaction_discard();
    [[nodiscard]] bool transaction_pending_block();
}

#endif
