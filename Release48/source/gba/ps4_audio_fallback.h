#ifndef PS4_AUDIO_FALLBACK_H
#define PS4_AUDIO_FALLBACK_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int ps4_audio_fallback_play(uint8_t id);
void ps4_audio_fallback_stop(void);
int ps4_audio_fallback_playing(void);
uint8_t ps4_audio_fallback_current(void);
void ps4_audio_fallback_prepare_528(int8_t out528[528]);
void ps4_audio_fallback_commit(void);
void ps4_audio_fallback_discard(void);
/* CP22: advance the source sequencer exactly one frame (224 samples @13379 Hz). */
void ps4_audio_fallback_render_224(int8_t out224[224]);
void ps4_audio_fallback_set_mute_mask(uint16_t lane_mask);
#ifdef __cplusplus
}
#endif
#endif
