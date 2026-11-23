#include "psiv_viewport.h"
static int limit(int n, int low, int high) {
    return n < low ? low : (n > high ? high : n);
}
PsivCamera psiv_camera_at(int source_w, int source_h, int player_x, int player_y) {
    PsivCamera c;
    c.source_w = (int16_t)(source_w < 0 ? 0 : source_w);
    c.source_h = (int16_t)(source_h < 0 ? 0 : source_h);
    c.x = (int16_t)limit(player_x - PSIV_GBA_W / 2, 0,
                         source_w > PSIV_GBA_W ? source_w - PSIV_GBA_W : 0);
    c.y = (int16_t)limit(player_y - PSIV_GBA_H / 2, 0,
                         source_h > PSIV_GBA_H ? source_h - PSIV_GBA_H : 0);
    return c;
}
void psiv_camera_follow(PsivCamera *c, int player_x, int player_y) {
    /* Move only beyond the GBA-centred 64x48 dead zone; no resampling. */
    int screen_x = player_x - c->x;
    int screen_y = player_y - c->y;
    int x = c->x, y = c->y;
    if (screen_x < 88) x -= 88 - screen_x;
    else if (screen_x > 152) x += screen_x - 152;
    if (screen_y < 56) y -= 56 - screen_y;
    else if (screen_y > 104) y += screen_y - 104;
    c->x = (int16_t)limit(x, 0, c->source_w > PSIV_GBA_W ? c->source_w - PSIV_GBA_W : 0);
    c->y = (int16_t)limit(y, 0, c->source_h > PSIV_GBA_H ? c->source_h - PSIV_GBA_H : 0);
}
PsivClip psiv_clip_rect(PsivCamera camera, PsivRect source) {
    PsivClip clipped;
    int x0 = source.x > camera.x ? source.x : camera.x;
    int y0 = source.y > camera.y ? source.y : camera.y;
    int x1 = source.x + source.w < camera.x + PSIV_GBA_W
        ? source.x + source.w : camera.x + PSIV_GBA_W;
    int y1 = source.y + source.h < camera.y + PSIV_GBA_H
        ? source.y + source.h : camera.y + PSIV_GBA_H;
    if (x1 < x0) x1 = x0;
    if (y1 < y0) y1 = y0;
    clipped.world = (PsivRect){(int16_t)x0, (int16_t)y0, (int16_t)(x1 - x0), (int16_t)(y1 - y0)};
    clipped.destination = (PsivRect){(int16_t)(x0 - camera.x), (int16_t)(y0 - camera.y),
                                    clipped.world.w, clipped.world.h};
    return clipped;
}
void psiv_ui_layout(PsivUILayout *u) {
    u->field = (PsivRect){0, 0, 240, 160};
    u->portrait = (PsivRect){4, 114, 40, 40};
    u->dialogue = (PsivRect){48, 112, 188, 44};
    u->battle_field = (PsivRect){0, 0, 240, 112};
    u->battle_actions = (PsivRect){4, 4, 72, 66};
    u->battle_target = (PsivRect){80, 4, 156, 76};
    u->party_status = (PsivRect){4, 116, 232, 40};
}
int psiv_rect_inside_screen(PsivRect r) {
    return r.x >= 0 && r.y >= 0 && r.w > 0 && r.h > 0 &&
           (int)r.x + r.w <= PSIV_GBA_W && (int)r.y + r.h <= PSIV_GBA_H;
}
