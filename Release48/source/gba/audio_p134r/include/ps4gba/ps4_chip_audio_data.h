#ifndef PS4GBA_PS4_CHIP_AUDIO_DATA_H
#define PS4GBA_PS4_CHIP_AUDIO_DATA_H

namespace ps4gba::chip_audio_data
{
    enum class lane_kind : unsigned char
    {
        dac = 0,
        fm = 1,
        psg = 2,
    };

    struct track_header
    {
        unsigned char kind;
        unsigned char reserved0;
        unsigned char reserved1;
        unsigned int address;
        signed char transpose;
        unsigned char volume;
        unsigned char initial_modulation;
        unsigned char initial_psg_envelope;
    };

    struct song_header
    {
        unsigned char music_id;
        unsigned char fm_count;
        unsigned char psg_count;
        unsigned char tick_multiplier;
        unsigned char tempo;
        unsigned char lane_count;
        unsigned char has_dac;
        unsigned char reserved;
        unsigned int instrument_bank_address;
        track_header tracks[9];
    };

    struct dac_sample
    {
        unsigned int offset;
        unsigned int length;
    };

    struct envelope_span
    {
        unsigned int offset;
        unsigned int length;
    };

    [[nodiscard]] const song_header* find_song(unsigned char music_id);
    [[nodiscard]] unsigned char source_byte(unsigned int address);
    [[nodiscard]] const signed char* dac_sample_data(unsigned char sound_id, unsigned int& length);
    [[nodiscard]] const unsigned char* psg_envelope_data(unsigned char envelope_id, unsigned int& length);
    [[nodiscard]] unsigned int fm_phase_increment(unsigned char note_index);
    [[nodiscard]] unsigned int psg_phase_increment(unsigned char note_index);
}

#endif
