/* Source $12 RajaSick ($7734C), $13 MeetingKyra ($77788).
 * AF resumes real map loads using the existing continuation bytes. */
static void r38_main_wait(unsigned count,unsigned camx,unsigned camy){
    for(unsigned f=0u;f<=count;f++){
        field_update_npcs(camx,camy);field_run_map_updates();
        field_event_redraw_explicit(camx,camy);waitvb();
    }
}
/* Event_UpdateObjFacing animates frame zero immediately and publishes one VB;
 * it does not advance the map or the other objects. */
static void r38_party_face(uint8_t id,uint8_t face,unsigned camx,unsigned camy,unsigned publish){
    int slot=field_event_party_slot_for_id(id);if(slot<0)return;
    uint16_t b=(uint16_t)(0xC000u+(unsigned)slot*64u);
    field_event_set_party_facing(id,face);ram8w((uint16_t)(b+0x10u),0u);
    ram8w((uint16_t)(b+0x11u),(uint8_t)(publish?field_party_anim_period(id)-1u:0u));
    ram8w((uint16_t)(b+2u),(uint8_t)(ram8r((uint16_t)(b+2u))|0x60u));
    if(id==lead_char_id){chaz.anim=0u;chaz.anim_clock=0u;hw_chaz_key=0xFFFFu;}
    else{struct PartyFollowerLite* p=field_event_follower_for_id(id);if(p){p->anim=0u;p->anim_clock=0u;}}
    if(publish){field_event_redraw_explicit(camx,camy);waitvb();}
}
static void r38_face_raja(uint8_t id,uint16_t rx,uint16_t ry,unsigned camx,unsigned camy){
    int slot=field_event_party_slot_for_id(id);if(slot<0)return;
    uint16_t b=(uint16_t)(0xC000u+(unsigned)slot*64u);
    int dx=(int)rx-(int)ram16r((uint16_t)(b+0x30u)),dy=(int)ry-(int)ram16r((uint16_t)(b+0x34u));
    unsigned ax=(unsigned)(dx<0?-dx:dx),ay=(unsigned)(dy<0?-dy:dy);
    uint8_t face=(uint8_t)ram16r((uint16_t)(b+6u));
    if(ax>ay)face=(uint8_t)(dx<0?12u:8u);else if(ay>ax)face=(uint8_t)(dy<0?4u:0u);
    ram32w((uint16_t)(b+0x20u),0u);ram32w((uint16_t)(b+0x24u),0u);
    r38_party_face(id,face,camx,camy,0u);
}
__attribute__((noinline))
static int r38_cutscene_raja_sick(unsigned* camx,unsigned* camy){
    if(field_obj_count<=2u||field_event_party_slot_for_id(8u)<0)return 0;
    uint8_t face=(uint8_t)(ram16r(0xC006u)^4u);
    field_event_set_source_object_facing(2u,face,camx,camy);
    field_objs[2].anim=0u;ram8w(0xC390u,0u);ram8w(0xC391u,(uint8_t)rom8(rom32(0x4A504u+face)+1u));
    field_event_redraw_explicit(*camx,*camy);waitvb();field_dialogue_show(0x59u,*camx,*camy);
    static const uint8_t faces[8]={0u,8u,4u,12u,0u,8u,4u,12u};
    for(unsigned i=0u;i<8u;i++){r38_party_face(8u,faces[i],*camx,*camy,1u);r36_map_wait(9u);}
    int slot=field_event_party_slot_for_id(8u);uint16_t b=(uint16_t)(0xC000u+(unsigned)slot*64u);
    uint16_t rx=ram16r((uint16_t)(b+0x30u)),ry=ram16r((uint16_t)(b+0x34u));
    (void)load_nem_vram_auto(0x1D78CAu,0x37Fu);r31_party_hide(8u,1u);
    r31_actor_spawn(7,0x374u,rx,ry,0u,0x37Fu);
    ram16w(0xC4F8u,ram16r((uint16_t)(b+0x38u)));ram16w(0xC4FAu,ram16r((uint16_t)(b+0x3Au)));
    ram8w(0x500Au,0xE1u);r38_main_wait(0x13u,*camx,*camy);
    ram8w(0x500Au,0x9Fu);r38_main_wait(0x13u,*camx,*camy);
    static const uint8_t ids[4]={0u,3u,5u,7u};
    for(unsigned i=0u;i<4u;i++)r38_face_raja(ids[i],rx,ry,*camx,*camy);
    field_dialogue_show(0x62u,*camx,*camy);field_palette_fade_out();ps4_event_flag_set(0x94u);r33_party_remove(8u);
    r34_queue(0xAFu,0u,0x134u,0x133u,0x34u,0x38u,0u,0u);return 1;
}
/* Keep the complete original NPC carrier, including its palette/mapping
 * state. Only the source-authored type, steps, coordinates and facing change. */
static void r38_messenger_copy(void){
    uint16_t base=field_objs[2].base_tile;uint8_t blocks=field_objs[2].blocks,dialog=field_objs[2].dialog;
    (void)r31_actor_spawn(8,0x8194u,0x190u,0x240u,8u,base);
    for(unsigned q=0u;q<64u;q++)ram8w((uint16_t)(0xC500u+q),ram8r((uint16_t)(0xC380u+q)));
    ram16w(0xC500u,0x8194u);ram32w(0xC520u,0u);ram32w(0xC524u,0u);ram32w(0xC528u,0u);
    ram16w(0xC530u,0x190u);ram16w(0xC534u,0x240u);ram16w(0xC538u,0x1C0u);ram16w(0xC53Au,0x210u);ram16w(0xC506u,8u);
    field_objs[8].anim=ram8r(0xC510u);field_objs[8].blocks=blocks;field_objs[8].dialog=dialog;
}
__attribute__((noinline))
static int r38_cutscene_meeting_kyra(unsigned* camx,unsigned* camy){
    r36_bitmap_begin();field_cutscene_panels_clear();field_cutscene_panels=1u;field_active_panel_count=0u;
    r34_panel(0x140u,0u);r33_dialogue_tree(0x1F9580u);field_dialogue_show_keep_panels(0x27u,*camx,*camy);
    field_cutscene_panels_clear();for(unsigned i=0u;i<64u;i++)cram[i]=rom16(0x9F2BCu+i*2u);
    field_palette_mirror_live(64u);hw_load_palettes();ram8w(0x500Au,0x92u);field_palette_fade_in();r34_panel(0x99u,0u);
    ram8w(0x500Au,0xF3u);field_dialogue_show_keep_panels(0x28u,*camx,*camy);
    r34_vint_wait(0x3Bu);r34_panel(0x9Bu,0x77u);
    ram8w(0x500Au,0xF3u);(void)field_dialogue_resume_saved_keep_panels(*camx,*camy);
    field_palette_fade_out();field_cutscene_panels_clear();field_cutscene_panels=field_active_panel_count=0u;
    ram8w(0xF40Eu,9u);field_event_macro_add_attack(9u);ps4_event_flag_set(0xA0u);
    r34_queue(0xAFu,1u,1u,0xFFFFu,0x174u,0x1Cu,0u,0u);return 1;
}
__attribute__((noinline))
static int r38_story_resume(unsigned* camx,unsigned* camy){
    if(field_custom_resume_event!=0xAFu)return 0;
    if(field_custom_resume_phase==0u){
        if(field_obj_count<=2u)return 0;
        static const uint16_t positions[4][2]={{0x1A0u,0x1C0u},{0x1B0u,0x1C0u},{0x1B0u,0x1D0u},{0x1A0u,0x1E0u}};
        for(unsigned i=0u;i<4u;i++)r33_party_position(ram8r((uint16_t)(0xF40Au+i)),positions[i][0],positions[i][1],12u);
        for(unsigned q=0x20u;q<=0x28u;q+=4u)ram32w((uint16_t)(0xC380u+q),0u);
        ram32w(0xC3B0u,0x180u<<16);ram32w(0xC3B4u,0x1E0u<<16);ram32w(0xC3B8u,0u);ram32w(0xC3BAu,0u);
        ram16w(0xC3B8u,0x180u);ram16w(0xC3BAu,0x1E0u);ram16w(0xC386u,4u);
        struct FieldObjLite* o=&field_objs[2];o->x=0x180u;o->y=0x1E0u;o->facing=4u;
        o->move_left=0u;
        /* The common native loader owns sprite carriers, but does not fill
         * the source NPC mapping/palette fields. Install the original Esper
         * initialization before its complete record is used by this scene. */
        ram16w(0xC380u,0x8248u);ram32w(0xC388u,0x4A504u);ram32w(0xC38Cu,0x4A7D0u);
        ram8w(0xC382u,8u);ram8w(0xC390u,0u);ram8w(0xC391u,4u);ram8w(0xC393u,0x40u);
        ram16w(0xC396u,o->base_tile);ram16w(0xC39Cu,0u);ram16w(0xC3BCu,0x404u);ram16w(0xC3BEu,0x202u);
        hw_refresh_field_object_art(2u);
        r38_main_wait(0u,*camx,*camy);field_palette_fade_in();
        r34_dialogue_segment(0x1EC870u,0x62u,1u);r31_resume_dialogue(camx,camy);
        r36_field_camera_begin(*camx,*camy);r37_dezo_camera(0x1A0u,0x240u,4u,camx,camy);
        ram8w(0x500Au,0xDFu);r36_map_wait(0x13u);r38_messenger_copy();
        r38_main_wait(0x2Fu,*camx,*camy);ram8w(0x500Au,0xA9u);r36_map_wait(9u);
        field_dialogue_show(0x63u,*camx,*camy);
        static const uint8_t ids[4]={0u,3u,5u,7u};for(unsigned i=0u;i<4u;i++)r38_party_face(ids[i],0u,*camx,*camy,1u);
        r37_dezo_camera(ram16r(0xC030u),ram16r(0xC034u),4u,camx,camy);
        r31_resume_dialogue(camx,camy);ram8w(0xECECu,0xA9u);
    }else if(field_custom_resume_phase==1u){
        ram8w(0x500Au,0x8Du);ram8w(0xECECu,0x8Du);
        ram16w(0xC000u,0xB4u);for(unsigned i=1u;i<5u;i++)ram16w((uint16_t)(0xC000u+i*64u),0u);
        ram16w(0xF43Cu,2u);field_event_set_mapload_bit(0u);
        for(unsigned i=0u;i<field_follower_count;i++)field_followers[i].active=0u;field_follower_count=0u;
        r30_vehicle_palette(2u);hw_chaz_key=0xFFFFu;hw_field_init(*camx,*camy);field_palette_fade_in();
    }else return 0;
    field_custom_resume_event=0xFFu;field_custom_resume_phase=0u;return 1;
}
