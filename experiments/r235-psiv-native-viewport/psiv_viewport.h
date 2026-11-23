#ifndef PSC_PSIV_VIEWPORT_H
#define PSC_PSIV_VIEWPORT_H
/* Experimental native-pixel framing math for R235 PSIV; no ROM dependencies.
   Do not integrate until the exact R235 renderer source is authenticated. */
#include <stdint.h>
enum { PSIV_GBA_W = 240, PSIV_GBA_H = 160,
       PSIV_GENESIS_W = 320, PSIV_GENESIS_H = 224 };
typedef struct { int16_t x, y, w, h; } PsivRect;
typedef struct { int16_t x, y, source_w, source_h; } PsivCamera;
typedef struct {
    PsivRect world;
    PsivRect destination;
} PsivClip;
typedef struct {
    PsivRect field;
    PsivRect portrait;
    PsivRect dialogue;
    PsivRect battle_field;
    PsivRect battle_actions;
    PsivRect battle_target;
    PsivRect party_status;
} PsivUILayout;
PsivCamera psiv_camera_at(int source_w, int source_h, int player_x, int player_y);
void psiv_camera_follow(PsivCamera *camera, int player_x, int player_y);
PsivClip psiv_clip_rect(PsivCamera camera, PsivRect source);
void psiv_ui_layout(PsivUILayout *layout);
int psiv_rect_inside_screen(PsivRect rect);
#endif
