#include "r30_vehicle_field.h"
__attribute__((noinline))
static void r30_vehicle_palette(unsigned vehicle){
    if(vehicle<1u||vehicle>3u)return;
    for(unsigned i=0u;i<16u;i++)field_palette_live_word(48u+i,rom16(0x29754Eu+(vehicle-1u)*32u+i*2u));
}
__attribute__((noinline))
static int r30_vehicle_draw(unsigned camx,unsigned camy){
    unsigned v=ram16r(0xF43Cu);if(v<1u||v>3u)return 0;
    unsigned frame=chaz.step_left?(chaz.anim&1u):0u;uint16_t key=(uint16_t)(0x8000u|(v<<8)|((unsigned)chaz.facing<<1)|frame);
    if(hw_chaz_key!=key){dma_copy32(r30_vehicle_field[v-1u][chaz.facing&3u][frame],(void*)&OBJ_VRAM16[FIELD_FOLLOWER_TILE_BASE*16u],1024u);hw_chaz_key=key;}
    hw_obj_field(0u,chaz.x-(int)camx-4,chaz.y-(int)camy,64u,32u,FIELD_FOLLOWER_TILE_BASE,3u,0);
    for(unsigned i=0u;i<FIELD_MAX_FOLLOWERS;i++)hw_hide_obj(40u+i);return 1;
}
