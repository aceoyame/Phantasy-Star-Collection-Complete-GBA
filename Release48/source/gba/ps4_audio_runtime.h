#ifndef PS4_AUDIO_RUNTIME_H
#define PS4_AUDIO_RUNTIME_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void ps4_audio_init(void);
void ps4_audio_prepare_frame(void);
void ps4_audio_prepare_guard_frame(void);
void ps4_audio_vblank_commit(void);
void ps4_audio_late_vblank(void);
void ps4_audio_frame_service(void);
int ps4_audio_play(uint8_t sound_id);
int ps4_audio_sound_index(uint8_t sound_id);
void ps4_audio_stop(void);
uint8_t ps4_audio_current_music(void);
uint8_t ps4_audio_current_sfx(void);
uint8_t ps4_audio_current_special_sfx(void);
#ifdef __cplusplus
}
#endif
#endif
