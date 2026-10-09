/* Event_EclipseTorchStolen $46 and EclipseTorchUsed $47.
 * Revision>0 source dialogue, actors, waits, chunk copy and terminal flags. */
static void r41_source_face(unsigned slot,uint8_t face,unsigned camx,unsigned camy){
    if(slot>=field_obj_count||!field_objs[slot].active)return;
    struct FieldObjLite* o=&field_objs[slot];uint16_t b=(uint16_t)(0xC300u+slot*64u);
    o->facing=(uint8_t)(face&0x1Cu);ram16w((uint16_t)(b+6u),o->facing);
    ram8w((uint16_t)(b+0x10u),0xFFu);ram8w((uint16_t)(b+0x11u),0u);r31_actor_animate(slot);
    field_obj_art_stamp[slot].epoch=0u;field_event_redraw_explicit(camx,camy);waitvb();
}
/* Event_MoveCamera moves X then Y along the shortest 12-bit source route.
 * RunMapUpdates precedes the camera/object publication on each source tick. */
static void r41_camera(uint16_t wx,uint16_t wy,unsigned* camx,unsigned* camy){
    unsigned target[2]={((unsigned)wx-0x98u)&0xFFFu,((unsigned)wy-0x58u)&0xFFFu};
    for(unsigned axis=0u;axis<2u;axis++){
        unsigned* pos=axis?camy:camx;*pos&=0xFFFu;unsigned delta=(target[axis]-*pos)&0xFFFu;
        int direction=delta<=0x7FFu?1:-1;unsigned count=direction>0?delta:0x1000u-delta;
        for(unsigned f=0u;f<count;f++){
            field_run_map_updates();*pos=(unsigned)((int)*pos+direction)&0xFFFu;
            ram16w(axis?0xEF90u:0xEF94u,(uint16_t)*pos);field_update_npcs(*camx,*camy);
            field_event_redraw_explicit(*camx,*camy);waitvb();
        }
    }
}
static void r41_actor_change(unsigned i,uint16_t type){
    if(i>=field_obj_count)return;field_objs[i].type=type;field_objs[i].active=1u;field_objs[i].move_kind=0u;
    ram16w((uint16_t)(0xC300u+i*64u),type);field_obj_art_stamp[i].epoch=0u;
}
__attribute__((noinline))
static int r41_event_torch_stolen(unsigned* camx,unsigned* camy){
    if(field_obj_count<=6u||!field_objs[6].active)return 0;
    r33_dialogue_tree(0x1F9580u);struct FieldObjLite* bishop=&field_objs[6];uint8_t old_move=bishop->move_kind;
    bishop->type=0x8194u;bishop->move_kind=0u;ram16w(0xC480u,0x8194u);
    ram16w(0xC4B8u,bishop->x);ram16w(0xC4BAu,bishop->y);
    r41_source_face(6u,(uint8_t)(ram16r(0xC006u)^4u),*camx,*camy);ram8w(0x500Au,0xF3u);field_dialogue_show(0x32u,*camx,*camy);
    (void)load_nem_vram_auto(0x1B1D06u,0x36Eu);r41_source_face(6u,4u,*camx,*camy);r36_map_wait(0x1Du);
    for(unsigned slot=0u;slot<5u;slot++)r38_party_face(ram8r((uint16_t)(0xF40Au+slot)),4u,*camx,*camy,1u);
    r36_map_wait(0x1Du);ram8w(0x500Au,0xA8u);r41_camera(0x270u,0xF0u,camx,camy);ram8w(0x500Au,0xDEu);
    static const uint16_t xs[3]={0x250u,0x270u,0x290u},ys[3]={0xF0u,0xE0u,0xF0u};
    for(unsigned i=0u;i<3u;i++)r31_actor_spawn((int)(8u+i),0x280u,xs[i],ys[i],0u,0x36Eu);
    for(unsigned i=0u;i<3u;i++)r31_actor_spawn((int)(11u+i),0x28Cu,xs[i],ys[i],0u,0x36Eu);
    r38_main_wait(0x61u,*camx,*camy);ram8w(0x500Au,0xF3u);r31_resume_dialogue(camx,camy);ram8w(0x500Au,0xDEu);
    ram16w(0xC520u,2u);ram16w(0xC538u,0x290u);ram16w(0xC5A0u,0xFFFEu);ram16w(0xC5B8u,0x250u);
    static const uint32_t vx[4]={0x18000u,0x10000u,0xFFFE8000u,0xFFFF0000u};
    static const uint16_t gx[4]={0x250u,0x250u,0x290u,0x290u},dest[4]={0x290u,0x290u,0x24Fu,0x250u};
    for(unsigned i=0u;i<4u;i++){
        r31_actor_spawn((int)(14u+i),0x294u,gx[i],0xF0u,0u,0x36Eu);uint16_t b=(uint16_t)(0xC680u+i*64u);
        ram32w((uint16_t)(b+0x20u),vx[i]);ram16w((uint16_t)(b+0x38u),dest[i]);
    }
    r38_main_wait(0xFu,*camx,*camy);r31_actor_clear(0u);r31_actor_clear(1u);
    r38_main_wait(0x59u,*camx,*camy);ram8w(0x500Au,0xF3u);r31_resume_dialogue(camx,camy);ram8w(0x500Au,0xDEu);
    for(unsigned i=8u;i<11u;i++){r41_actor_change(i,0x288u);r31_actor_hidden(i,1u);if(i!=9u)ram16w((uint16_t)(0xC320u+i*64u),0u);}
    for(unsigned i=11u;i<14u;i++)r41_actor_change(i,0x290u);
    r38_main_wait(0x55u,*camx,*camy);r36_map_wait(0x3Bu);r41_camera(ram16r(0xC030u),ram16r(0xC034u),camx,camy);
    bishop=&field_objs[6];bishop->type=0x38u;bishop->move_kind=old_move;ram16w(0xC480u,0x8038u);
    field_obj_art_stamp[6].epoch=0u;hw_refresh_field_object_art(6u);
    ram8w(0x500Au,0xFBu);r36_map_wait(9u);ram8w(0x500Au,0xA6u);r36_map_wait(0u);ram8w(0x500Au,0xF3u);r31_resume_dialogue(camx,camy);
    ram8w(0x500Au,0xFBu);r36_map_wait(9u);ram8w(0x500Au,0x93u);r33_dialogue_tree(0x1FA480u);ps4_event_flag_set(0x98u);return 1;
}
static void r41_disembark(unsigned camx,unsigned camy){
    if(!ram16r(0xF43Cu))return;
    ram8w(0xECECu,0x99u);ram8w(0x500Au,0xFDu);waitvb();ram8w(0x500Au,0x99u);
    field_event_redraw_explicit(camx,camy);waitvb();
    for(unsigned i=0u;i<16u;i++)cram[32u+i]=rom16(0x296300u+i*2u);
    field_palette_mirror_live(64u);hw_load_palettes();
    uint16_t x=ram16r(0xC030u),y=ram16r(0xC034u);uint8_t face=(uint8_t)ram16r(0xC006u);
    for(unsigned slot=0u;slot<5u;slot++){
        uint8_t id=ram8r((uint16_t)(0xF40Au+slot));if(id==0xFFu)break;
        uint16_t b=(uint16_t)(0xC000u+slot*64u);ram16w(b,(uint16_t)(id*4u+4u));r33_party_position(id,x,y,face);
    }
    ram16w(0xF43Cu,0u);field_followers_sync_aligned(0u);hw_chaz_key=0xFFFFu;
    field_update_npcs(camx,camy);field_event_redraw_explicit(camx,camy);waitvb();
}
static void r41_torch_panel(unsigned camx,unsigned camy){
    /* Existing native field snapshot keeps the field behind this small panel.
     * Page0 stores the clean base; page1 is the displayed panel page. */
    ps4_menu_base_cached=0u;ps4_ui_clear_field(camx,camy);ps4_menu_base_cached=0u;
    pause_draw_sprite_rom(lead_art_address(),sprite_frame_offset(chaz.facing,chaz.anim),chaz.x-(int)camx,chaz.y-(int)camy-2,2u,chaz.facing==3u);
    for(unsigned i=0u;i<field_follower_count;i++){
        struct PartyFollowerLite* f=&field_followers[i];if(!f->active)continue;
        pause_draw_sprite_rom(char_field_art_address(f->id),sprite_frame_offset(f->facing,f->anim),f->x-(int)camx,f->y-(int)camy-2,2u,f->facing==3u);
    }
    field_cutscene_panels=1u;field_active_panel_count=0u;panel_capture_base_scaled();pause_panel_action(0u,0x10Du);
    /* Panel_Draw replaces source palette line3. Mirror it into the source
     * live palette so both timed flashes save/restore the panel's colors. */
    for(unsigned i=0u;i<16u;i++)cram[48u+i]=rom16(0x2D6724u+i*2u);
    field_palette_mirror_live(64u);hw_load_palettes();
}
static void r41_torch_flash(void){
    field_palette_save(64u);for(unsigned i=0u;i<64u;i++)cram[i]=0xEEEu;field_palette_mirror_live(64u);hw_load_palettes();
    r36_map_wait(1u);
    for(unsigned f=0u;f<40u;f++){
        if(!(ram16r(0xEF1Cu)&3u))field_palette_restore_step();field_palette_mirror_live(64u);hw_load_palettes();waitvb();
    }
}
__attribute__((noinline))
static int r41_event_torch_used(unsigned* camx,unsigned* camy){
    r41_disembark(*camx,*camy);r41_torch_panel(*camx,*camy);r36_map_wait(0x77u);
    ram8w(0x500Au,0xDCu);r41_torch_flash();ram8w(0x500Au,0xDCu);r41_torch_flash();
    pause_panel_action(1u,0u);field_cutscene_panels=field_active_panel_count=0u;hw_field_init(*camx,*camy);
    (void)load_nem_vram_auto(0x1D712Cu,0x3AFu);
    static const uint16_t x[9]={0xBA0u,0xB80u,0xBA0u,0xBC0u,0xB60u,0xB80u,0xBA0u,0xBC0u,0xBE0u};
    for(unsigned i=0u;i<9u;i++){
        r31_actor_spawn((int)i,0x338u,x[i],(uint16_t)(i==0u?0xC0u:(i<4u?0xA0u:0x80u)),(uint8_t)(i==0u?0u:(i<4u?4u:8u)),0x3AFu);
        ram8w((uint16_t)(0xC311u+i*64u),(uint8_t)(i==0u?7u:(i<4u?0xBu:0xFu)));
    }
    field_manager_copy_ram_words(0x6100u,0x6960u,16u);field_update_npcs(*camx,*camy);
    hw_ring_layout_dirty=1u;field_event_redraw_explicit(*camx,*camy);waitvb();ram8w(0x500Au,0xBCu);r38_main_wait(0x4Fu,*camx,*camy);
    r33_dialogue_tree(0x1F9580u);ram8w(0x500Au,0xF3u);field_dialogue_show(0x2Au,*camx,*camy);
    field_manager_copy_ram_words(0x6100u,0x6960u,16u);hw_ring_layout_dirty=1u;ps4_event_flag_set(0x9Cu);return 1;
}
