/* Cutscene_AlysWounded ($09). Two real RefreshMap continuations. */
static void r33_dialogue_tree(uint32_t tree){
    unsigned n=kos_decode(tree,field_dialogue,FIELD_DIALOGUE_CAP);if(n)field_dialogue_len=n;
    field_dialogue_saved_cursor=0;field_dialogue_last_stop=0u;
}
static void r33_party_position(uint8_t id,uint16_t x,uint16_t y,uint8_t face){
    int slot=field_event_party_slot_for_id(id);if(slot<0)return;uint16_t b=(uint16_t)(0xC000u+(unsigned)slot*64u);
    ram16w((uint16_t)(b+0x30u),x);ram16w((uint16_t)(b+0x34u),y);ram16w((uint16_t)(b+0x38u),x);ram16w((uint16_t)(b+0x3Au),y);
    ram16w((uint16_t)(b+6u),face);field_event_sync_party_carrier((unsigned)slot,id,0u);
}
static void r33_party_remove(uint8_t id){
    int slot=field_event_party_slot_for_id(id);if(slot<0)return;
    unsigned removed_living=r48_char_living(id);
    for(unsigned n=(unsigned)slot;n<4u;n++){
        for(unsigned q=0u;q<64u;q++)ram8w((uint16_t)(0xC000u+n*64u+q),ram8r((uint16_t)(0xC040u+n*64u+q)));
        ram8w((uint16_t)(0xF40Au+n),ram8r((uint16_t)(0xF40Bu+n)));
    }
    for(unsigned q=0u;q<64u;q++)ram8w((uint16_t)(0xC100u+q),0u);
    ram8w(0xF40Eu,0xFFu);field_event_macro_remove_char(id);ram8w((uint16_t)(PS4_CHAR_BASE+(unsigned)id*128u+0x16u),0u);
    if(id==lead_char_id){lead_char_id=ram8r(0xF40Au);field_event_sync_party_carrier(0u,lead_char_id,0u);}
    r48_scripted_departure_guard(removed_living);
}
static void r33_alys_queue_inn(void){
    ram8w(0x500Au,0xA5u);r33_party_remove(2u);r33_party_remove(1u);
    ram8w(0xF40Du,6u);field_event_macro_add_attack(6u);
    field_custom_resume_event=0xA9u;field_custom_resume_phase=1u;
    field_cutscene_reload(0x3Fu,0x3Eu,0x46u,0x44u,8u,0u);
}
__attribute__((noinline))
static int r33_cutscene_alys_wounded(unsigned* camx,unsigned* camy){
    field_palette_fade_in();field_dialogue_show(0x43u,*camx,*camy);
    ram8w(0x500Au,0xDEu);ram8w(0xED52u,3u);field_palette_all64_to_black(3u);
    field_cutscene_panels_clear();r33_map_wait(0x3Bu,camx,camy);field_palette_copy_saved_to_live();field_palette_fade_in();
    field_cutscene_panel_wait(0x4Au,0x46u);
    if(field_dialogue_saved_cursor)(void)field_dialogue_resume_saved_keep_panels(*camx,*camy);
    ram8w(0x500Au,0xDEu);r33_map_wait(0x45u,camx,camy);field_cutscene_panels_clear();
    if(ps4_event_flag_test(0x12u)){r33_alys_queue_inn();return 1;}
    field_custom_resume_event=0xA9u;field_custom_resume_phase=0u;
    field_cutscene_reload(0x39u,0u,0x4Cu,0x6Cu,4u,4u);return 1;
}
__attribute__((noinline))
static int r33_cutscene_alys_resume(unsigned* camx,unsigned* camy){
    if(field_custom_resume_event!=0xA9u)return 0;
    if(field_custom_resume_phase==0u){
        ram8w(0x500Au,0x83u);(void)load_nem_vram_auto(0x139FF8u,0x2B8u);
        r31_actor_spawn(0,0x8194u,0x260u,0x310u,0u,0x2B8u);r31_actor_update_one(0u);
        ram8w((uint16_t)(PS4_CHAR_BASE+128u+0x16u),(uint8_t)(ram8r((uint16_t)(PS4_CHAR_BASE+128u+0x16u))|4u));field_palette_fade_in();
        field_event_move_party_id(lead_char_id,0x260u,0x320u,camx,camy);
        ram8w(0xECFEu,(uint8_t)(ram8r(0xECFEu)|1u));field_event_move_party_id(2u,0x270u,0x310u,camx,camy);
        field_event_set_party_facing(2u,12u);field_event_set_source_object_facing(0u,8u,camx,camy);
        r33_dialogue_tree(0x1E32A0u);field_dialogue_show(0x2Cu,*camx,*camy);
        field_event_set_party_facing(2u,0u);field_event_set_source_object_facing(0u,0u,camx,camy);r31_resume_dialogue(camx,camy);
        (void)load_nem_vram_auto(0x1D48FAu,0x580u);ram8w(0x500Au,0xCCu);
        r31_actor_spawn(7,0x1A0u,0x1A8u,0x1FAu,0u,0x580u);r31_actor_update_one(7u);
        field_objs[7].x=0x268u;field_objs[7].y=0x318u;ram32w(0xC4F0u,0x2680000u);ram32w(0xC4F4u,0x3180000u);
        while(ram16r(0xC4C0u))field_event_choreography_tick(*camx,*camy);
        r33_map_wait(0x1Du,camx,camy);field_event_set_party_facing(2u,12u);field_event_set_source_object_facing(0u,8u,camx,camy);r31_resume_dialogue(camx,camy);
        field_event_set_source_object_facing(0u,0u,camx,camy);r31_resume_dialogue(camx,camy);
        ram16w(0xC338u,0x260u);ram16w(0xC33Au,0x2B0u);
        for(unsigned n=0u;n<5u;n++){uint8_t id=ram8r((uint16_t)(0xF40Au+n));if(id!=0xFFu)field_event_set_party_dest(id,0x260u,0x2B0u);}
        field_event_move_party_id(2u,0x270u,0x2B0u,camx,camy);field_palette_fade_out();ps4_event_flag_set(0x12u);
        r33_alys_queue_inn();return 1;
    }
    if(field_custom_resume_phase!=1u)return 0;
    ram8w(0x500Au,0xA5u);ram8w(0xECECu,0xA5u);
    r33_party_position(0u,0x1E0u,0x1E0u,12u);r33_party_position(4u,0x1D0u,0x200u,4u);
    r33_party_position(5u,0x1F0u,0x1E0u,12u);r33_party_position(6u,0x1E0u,0x1F0u,4u);
    field_event_choreography_tick(*camx,*camy);field_palette_fade_in();field_event_move_camera_to(0x1E0u,0x1E0u,2u,camx,camy);
    field_event_set_source_object_facing(0u,12u,camx,camy);field_event_set_source_object_facing(1u,12u,camx,camy);
    r33_dialogue_tree(0x1E32A0u);field_dialogue_show(0x2Du,*camx,*camy);
    field_event_set_source_object_facing(1u,0u,camx,camy);r31_resume_dialogue(camx,camy);
    field_event_set_party_facing(0u,0u);r31_resume_dialogue(camx,camy);
    field_event_set_source_object_facing(1u,12u,camx,camy);field_event_set_party_facing(0u,12u);r31_resume_dialogue(camx,camy);
    field_event_set_source_object_facing(1u,0u,camx,camy);r31_resume_dialogue(camx,camy);
    field_event_set_party_facing(0u,8u);field_event_set_source_object_facing(0u,0u,camx,camy);r31_resume_dialogue(camx,camy);
    field_event_set_party_facing(0u,0u);r31_resume_dialogue(camx,camy);
    field_event_set_party_facing(5u,0u);r31_resume_dialogue(camx,camy);
    field_event_set_party_facing(0u,4u);field_event_set_party_facing(5u,12u);r31_resume_dialogue(camx,camy);
    field_event_set_party_facing(0u,12u);r31_resume_dialogue(camx,camy);
    field_event_set_party_facing(0u,4u);r31_resume_dialogue(camx,camy);
    ram8w(0xECFEu,(uint8_t)(ram8r(0xECFEu)|1u));field_event_move_party_id(lead_char_id,0x210u,0x210u,camx,camy);
    ram8w(0xECFEu,(uint8_t)(ram8r(0xECFEu)&~1u));field_event_move_party_id(lead_char_id,0x210u,0x220u,camx,camy);
    field_event_move_camera_to((uint16_t)chaz.x,(uint16_t)chaz.y,2u,camx,camy);ps4_event_flag_set(0x47u);
    r33_dialogue_tree(0x1E2680u);field_custom_resume_event=0xFFu;field_custom_resume_phase=0u;return 1;
}
