#ifndef PS4GBA_PS4_FM_MULTISAMPLE_DATA_H
#define PS4GBA_PS4_FM_MULTISAMPLE_DATA_H

namespace ps4gba::fm_multisample_data
{
    constexpr unsigned int timbre_stages = 6;

    struct fm_wave_zone
    {
        unsigned int bank_address;
        unsigned int wave_offset[timbre_stages];
        unsigned char instrument;
        unsigned char center_note;
        unsigned char gain_q6[timbre_stages]; // 64 = unity, generated from corrected four-op reference
    };

    // Release 04: flags use otherwise unused high bits of each ROM offset.
    // Bit 30 = 256 rather than 512 points; bit 31 = two source periods for
    // half-frequency operators. No larger zone or mutable channel is needed.
    inline unsigned int phase_shift(const fm_wave_zone& z) { return (z.wave_offset[0] & 0x40000000u) ? 24u : 23u; }
    inline unsigned int wave_points(const fm_wave_zone& z) { return 1u << (32u-phase_shift(z)); }
    inline unsigned int tune_increment(const fm_wave_zone& z, unsigned int increment) { return (z.wave_offset[0] & 0x80000000u) ? increment>>1 : increment; }

    [[nodiscard]] const fm_wave_zone* find_zone(unsigned int bank_address, unsigned char instrument, unsigned char note);
    [[nodiscard]] const signed char* wave(const fm_wave_zone& zone, unsigned int stage);
    [[nodiscard]] unsigned char gain(const fm_wave_zone& zone, unsigned int stage);
    [[nodiscard]] unsigned int wave_points();
    [[nodiscard]] unsigned int generated_sample_rate();
}

#endif
