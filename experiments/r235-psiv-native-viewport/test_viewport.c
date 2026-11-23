#include "psiv_viewport.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    PsivCamera c = psiv_camera_at(320, 224, 160, 112);
    assert(c.x == 40 && c.y == 32);
    PsivClip a = psiv_clip_rect(c, (PsivRect){40, 32, 240, 160});
    assert(a.world.w == 240 && a.world.h == 160);
    assert(a.destination.x == 0 && a.destination.y == 0);
    c = psiv_camera_at(320, 224, 0, 0);
    assert(c.x == 0 && c.y == 0);
    c = psiv_camera_at(320, 224, 319, 223);
    assert(c.x == 80 && c.y == 64);
    c = psiv_camera_at(1024, 1024, 300, 300);
    int before_x = c.x, before_y = c.y;
    psiv_camera_follow(&c, 300, 300);
    assert(c.x == before_x && c.y == before_y);
    psiv_camera_follow(&c, 470, 450);
    assert(c.x > before_x && c.y > before_y);
    PsivClip edge = psiv_clip_rect(c, (PsivRect){c.x-10, c.y-5, 16, 12});
    assert(edge.destination.x == 0 && edge.destination.y == 0);
    assert(edge.world.w == 6 && edge.world.h == 7);
    c = psiv_camera_at(80, 64, 40, 32);
    assert(c.x == 0 && c.y == 0);
    PsivUILayout ui;
    psiv_ui_layout(&ui);
    PsivRect rects[] = {ui.field, ui.portrait, ui.dialogue, ui.battle_field,
                        ui.battle_actions, ui.battle_target, ui.party_status};
    for (unsigned i = 0; i < sizeof(rects)/sizeof(rects[0]); i++)
        assert(psiv_rect_inside_screen(rects[i]));
    assert(ui.battle_field.y + ui.battle_field.h <= ui.party_status.y);
    puts("PASS: pixel-perfect camera, edge clamping, clipping, and UI layout");
    return 0;
}
