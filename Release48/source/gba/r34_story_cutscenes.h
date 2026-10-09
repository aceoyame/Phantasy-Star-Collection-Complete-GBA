/* Source Cutscene_PsycoWand ($75200) and Cutscene_ZioDefeated ($75A12).
 * AA/AB use the existing map-continuation bytes; no additional RAM globals. */
static void r34_vint_wait(unsigned count){for(unsigned f=0u;f<=count;f++)waitvb();}
static void r34_panel(unsigned id,unsigned count){field_cutscene_panel_wait(id,count+1u);}
static void r34_panels_begin(void){
    field_palette_fade_out();field_cutscene_panels_clear();
    for(unsigned i=0u;i<64u;i++)cram[i]=rom16(0x9F2BCu+i*2u);
    field_palette_mirror_live(64u);hw_load_palettes();field_palette_fade_in();
}
static void r34_field_restore(unsigned camx,unsigned camy){
    field_cutscene_panels_clear();hw_field_init(camx,camy);
}
/* The source's pushed A0 survives KosDecomp into the same dialogue RAM.
 * Restore that exact F7 segment after a real map load overwrites the buffer.
 * Parameter bytes are skipped, so an operand can never become a terminator. */
static void r34_dialogue_segment(uint32_t tree,unsigned id,unsigned seams){
    r33_dialogue_tree(tree);const uint8_t* p=field_dialogue_resolve(id);const uint8_t* end=field_dialogue+field_dialogue_len;
    while(seams&&p<end){
        unsigned op=*p++;if(op==0xFFu){p=end;break;}if(op==0xF7u){--seams;continue;}
        unsigned n=0u;
        if(op==0xF4u||op==0xF9u)n=1u;
        else if(op==0xF5u||op==0xFAu||op==0xF6u)n=2u;
        else if(op==0xF2u&&p<end){unsigned a=*p++;n=a==0u?2u:((a==3u||a==4u||a==0xBu)?1u:0u);}
        if((unsigned)(end-p)<n){p=end;break;}p+=n;
    }
    field_dialogue_saved_cursor=p<end&&!seams?p:0;field_dialogue_last_stop=0u;
}
static void r34_queue(uint8_t event,uint8_t phase,uint16_t map,uint16_t prev,uint16_t x,uint16_t y,uint16_t facing,uint16_t align){
    field_custom_resume_event=event;field_custom_resume_phase=phase;field_cutscene_reload(map,prev,x,y,facing,align);
    /* The outer dispatcher must not consume a pushed segment before reload. */
    field_dialogue_saved_cursor=0;field_dialogue_last_stop=0u;
}
/* Source Event_MoveObject updates one actor; Event_MoveCharacter updates the
 * full field. Both use authored destinations and ECE0/ECFE movement controls. */
static void r34_object_move(unsigned slot,uint16_t x,uint16_t y,unsigned all,unsigned* camx,unsigned* camy){
    if(slot>=field_obj_count)return;uint16_t b=(uint16_t)(0xC300u+slot*64u);
    ram16w((uint16_t)(b+0x38u),x);ram16w((uint16_t)(b+0x3Au),y);
    do{
        if(all){r31_party_dest_update();field_event_choreography_tick(*camx,*camy);}
        else {r31_actor_update_one(slot);field_run_map_updates();field_event_redraw_explicit(*camx,*camy);waitvb();}
    }while(field_objs[slot].active&&(field_objs[slot].x!=x||field_objs[slot].y!=y));
    ram32w((uint16_t)(b+0x20u),0u);ram32w((uint16_t)(b+0x24u),0u);
}
__attribute__((noinline))
static int r34_cutscene_psyco_wand(unsigned* camx,unsigned* camy){
    r34_panels_begin();field_dialogue_show_keep_panels(5u,*camx,*camy);
    r34_queue(0xAAu,0u,0x3Fu,0x3Eu,0x46u,0x44u,12u,0u);return 1;
}
__attribute__((noinline))
static int r34_psyco_resume(unsigned* camx,unsigned* camy){
    if(field_custom_resume_event!=0xAAu)return 0;
    if(field_custom_resume_phase==0u){
        ram8w(0xC313u,0x40u);r33_map_wait(0x3Bu,camx,camy);ram8w(0x500Au,0xA4u);field_palette_fade_in();
        field_event_set_party_dest(4u,0x1E0u,0x200u);field_event_set_party_dest(5u,0x1F0u,0x1E0u);
        field_event_set_party_dest(6u,0x1E0u,0x200u);field_event_set_party_dest(3u,0x1E0u,0x1F0u);
        ram8w(0xECFEu,(uint8_t)(ram8r(0xECFEu)|1u));field_event_move_party_id(0u,0x1E0u,0x1E0u,camx,camy);
        field_event_move_party_id(4u,0x1D0u,0x200u,camx,camy);
        field_event_set_party_facing(0u,12u);field_event_set_party_facing(4u,4u);field_event_set_party_facing(5u,12u);
        field_event_set_party_facing(6u,4u);field_event_set_party_facing(3u,12u);
        r33_dialogue_tree(0x1E32A0u);field_dialogue_show(0x2Eu,*camx,*camy);
        for(unsigned i=0u;i<32u;i++)cram[i]=0u;field_palette_mirror_live(32u);hw_load_palettes();waitvb();
        r34_panels_begin();r34_panel(0x56u,0x59u);r34_panel(0x57u,0x13u);
        (void)field_dialogue_resume_saved_keep_panels(*camx,*camy);
        ram8w(0xED52u,5u);field_palette_all64_to_black(5u);r34_panels_begin();r34_vint_wait(0x3Bu);
        r34_panel(0x5Au,0x27u);r34_panel(0x5Bu,9u);(void)field_dialogue_resume_saved_keep_panels(*camx,*camy);
        r34_panel(0x5Du,0x27u);r34_panel(0x5Eu,0x77u);ram8w(0xED52u,9u);field_palette_all64_to_black(9u);
        r34_field_restore(*camx,*camy);r33_map_wait(0x59u,camx,camy);r34_panels_begin();
        r34_panel(0x60u,0x3Bu);r34_panel(0x61u,0x27u);r34_panel(0x62u,0x4Fu);r34_panel(0x63u,0x27u);r34_panel(0x64u,0x12Bu);
        ram8w(0xED52u,9u);field_palette_all64_to_black(9u);r34_panels_begin();
        r34_panel(0x65u,0x3Bu);r34_panel(0x66u,0x59u);r34_panel(0x67u,0x3Bu);
        (void)field_dialogue_resume_saved_keep_panels(*camx,*camy);
        pause_panel_action(1u,0u);pause_panel_action(1u,0u);pause_panel_action(1u,0u);r34_vint_wait(0x59u);
        r34_panel(0x69u,0x3Bu);r34_panel(0x6Au,0x13u);(void)field_dialogue_resume_saved_keep_panels(*camx,*camy);
        r34_vint_wait(0x3Bu);r34_panel(0x6Cu,0x77u);ram8w(0xED52u,9u);field_palette_all64_to_black(9u);
        ram8w(0x5005u,1u);ram8w(0x5004u,0x1Fu);ram8w(0x500Au,0xFBu);
        r34_field_restore(*camx,*camy);field_recover_party();r34_vint_wait(0x3Bu);
        r34_queue(0xAAu,1u,0x39u,0u,0x28u,0x1Cu,12u,0x10u);return 1;
    }
    if(field_custom_resume_phase!=1u)return 0;
    (void)field_layout_write_world(9u*32u,5u*32u,0u,0x8Fu);(void)field_layout_write_world(9u*32u,6u*32u,1u,0x90u);
    (void)load_nem_vram_auto(0x139FF8u,0x2B8u);r31_actor_spawn(0,0x8194u,0x110u,0xE0u,0u,0x2B8u);
    r31_actor_spawn(12,0xCu,0x120u,0xE0u,0u,0x480u);field_event_choreography_tick(*camx,*camy);
    field_event_set_source_object_facing(0u,8u,camx,camy);field_event_set_source_object_facing(12u,8u,camx,camy);
    ram8w(0x500Au,0x83u);ram8w(0xECECu,0x83u);field_palette_fade_in();
    r34_dialogue_segment(0x1E32A0u,0x2Eu,5u);r31_resume_dialogue(camx,camy);
    r34_object_move(12u,0x100u,0xE0u,0u,camx,camy);ram16w(0xC338u,0xC0u);ram16w(0xC33Au,0x1A0u);
    r34_object_move(12u,0xC0u,0x1A0u,1u,camx,camy);r31_actor_clear(12u);r31_actor_clear(0u);
    (void)load_nem_vram_auto(0x121EACu,0x2B8u);ram8w(0xECE0u,0u);ram8w(0xECFEu,(uint8_t)(ram8r(0xECFEu)|1u));
    field_event_move_party_id(0u,0x130u,0xC0u,camx,camy);ram8w(0xECE0u,1u);ram8w(0xECFEu,(uint8_t)(ram8r(0xECFEu)&~1u));
    r33_map_wait(0x3Bu,camx,camy);field_event_set_party_facing(0u,0u);r31_resume_dialogue(camx,camy);
    ram8w(0xECFEu,(uint8_t)(ram8r(0xECFEu)|1u));field_event_move_party_id(lead_char_id,0x120u,0xE0u,camx,camy);
    ram8w(0xECFEu,(uint8_t)(ram8r(0xECFEu)&~1u));field_event_move_party_id(lead_char_id,0x110u,0xE0u,camx,camy);
    field_event_move_camera_to((uint16_t)chaz.x,(uint16_t)chaz.y,2u,camx,camy);
    ps4_event_flag_set(0x63u);ps4_event_flag_set(0x67u);r33_dialogue_tree(0x1E2680u);
    field_custom_resume_event=0xFFu;field_custom_resume_phase=0u;return 1;
}
/* loc_5A99E updates only the selected character, with literal 16.16 steps;
 * it neither moves the rest of the party nor rewrites destinations. */
static void r34_demi_drift(unsigned count,unsigned* camx,unsigned* camy){
    int slot=field_event_party_slot_for_id(6u);if(slot<0)return;uint16_t b=(uint16_t)(0xC000u+(unsigned)slot*64u);
    ram32w((uint16_t)(b+0x20u),0u);ram32w((uint16_t)(b+0x24u),0xFFFF0000u);
    for(unsigned f=0u;f<=count;f++){
        ram32w((uint16_t)(b+0x34u),ram32r((uint16_t)(b+0x34u))-0x10000u);
        field_event_sync_party_carrier((unsigned)slot,6u,1u);field_run_map_updates();field_event_redraw_explicit(*camx,*camy);waitvb();
    }
    ram32w((uint16_t)(b+0x20u),0u);ram32w((uint16_t)(b+0x24u),0u);
}
static void r34_elevator(unsigned reverse,unsigned* camx,unsigned* camy){
    uint32_t p=reverse?0x75FA8u:0x75F34u;
    while(rom8(p)!=0xFFu){
        for(unsigned i=0u;i<3u;i++)(void)field_layout_write_world((14u+i)*32u,12u*32u,0u,(uint8_t)rom8(p++));
        r33_map_wait(reverse?5u:7u,camx,camy);
    }
}
/* Map_Load_Flags bit 3 keeps all five source party objects through RefreshMap.
 * Local stack storage avoids adding to the nearly full EWRAM allocation. */
__attribute__((noinline))
static int r34_load_preserving_party(uint16_t map){
    uint8_t saved[5u*64u];for(unsigned i=0u;i<sizeof saved;i++)saved[i]=ram8r((uint16_t)(0xC000u+i));
    int rc=ps4_load_field_map(map);for(unsigned i=0u;i<sizeof saved;i++)ram8w((uint16_t)(0xC000u+i),saved[i]);return rc;
}
static void r34_sync_preserved_party(void){
    field_followers_sync(0);for(unsigned i=0u;i<5u;i++){uint8_t id=ram8r((uint16_t)(0xF40Au+i));if(id!=0xFFu)field_event_sync_party_carrier(i,id,0u);}
}
__attribute__((noinline))
static int r34_cutscene_zio_defeated(unsigned* camx,unsigned* camy){
    r34_panels_begin();r34_panel(0x6Du,0u);ram8w(0x500Au,0xCEu);r34_vint_wait(5u);
    ram8w(0x500Au,0xCEu);r34_vint_wait(5u);ram8w(0x500Au,0xCEu);r34_vint_wait(2u);r34_vint_wait(0x77u);
    field_dialogue_show_keep_panels(0u,*camx,*camy);r33_map_wait(0x3Bu,camx,camy);
    for(unsigned f=0u;f<40u;f++){if(!(ram16r(0xEF1Cu)&3u))field_palette_increase_tone_once();field_palette_mirror_live(64u);hw_load_palettes();waitvb();}
    r34_field_restore(*camx,*camy);r33_map_wait(0x3Bu,camx,camy);field_palette_fade_out();
    field_event_clear_source_object(0u);field_event_clear_source_object(1u);r33_party_position(6u,0x1E8u,0x1C0u,0u);
    field_custom_resume_event=0xABu;field_custom_resume_phase=0u;field_event_set_mapload_bit(3u);field_event_reload_pending=1u;
    field_dialogue_saved_cursor=0;field_dialogue_last_stop=0u;return 1;
}
__attribute__((noinline))
static int r34_zio_resume(unsigned* camx,unsigned* camy){
    if(field_custom_resume_event!=0xABu)return 0;
    if(field_custom_resume_phase==0u){
        ram8w(0xEC4Eu,(uint8_t)(ram8r(0xEC4Eu)&~8u));field_palette_fade_in();
        field_event_move_camera_to(0x1E8u,0x1C0u,1u,camx,camy);field_event_set_party_facing(6u,12u);
        r33_map_wait(9u,camx,camy);field_event_set_party_facing(6u,0u);
        r34_dialogue_segment(0x1F9020u,0u,1u);r31_resume_dialogue(camx,camy);ram8w(0xECFEu,(uint8_t)(ram8r(0xECFEu)|5u));
        field_event_move_camera_to(0x1E8u,0x1C0u,1u,camx,camy);r34_demi_drift(0xFu,camx,camy);field_event_set_party_facing(6u,4u);
        r34_demi_drift(0x1Fu,camx,camy);r33_map_wait(0x3Bu,camx,camy);ram8w(0x500Au,0xE0u);r34_elevator(0u,camx,camy);
        r33_map_wait(0x3Bu,camx,camy);r34_demi_drift(0x2Fu,camx,camy);r33_map_wait(0x3Bu,camx,camy);
        ram8w(0x500Au,0xE0u);r34_elevator(1u,camx,camy);r33_map_wait(0x3Bu,camx,camy);r31_resume_dialogue(camx,camy);
        ram8w(0x500Au,0xE4u);r33_map_wait(0x77u,camx,camy);cram[28u]=0x24u;cram[29u]=0x240u;field_palette_mirror_live(64u);hw_load_palettes();
        r34_vint_wait(0x3Bu);r31_resume_dialogue(camx,camy);field_palette_fade_out();r34_panels_begin();r34_panel(0x70u,0x13u);
        (void)field_dialogue_resume_saved_keep_panels(*camx,*camy);ram8w(0x500Au,0xFBu);r33_map_wait(0x13u,camx,camy);
        r34_queue(0xABu,1u,0u,0xACu,0x70u,0xBCu,0u,0u);return 1;
    }
    if(field_custom_resume_phase==1u){
        ram8w(0x500Au,0x8Cu);for(unsigned i=0u;i<5u;i++){uint16_t b=(uint16_t)(0xC002u+i*64u);ram8w(b,(uint8_t)(ram8r(b)|2u));}
        field_event_redraw_explicit(*camx,*camy);waitvb();field_palette_fade_in();(void)field_event_place_appearing(7u,camx,camy);
        field_palette_fade_out();r34_panels_begin();r34_dialogue_segment(0x1F9020u,0u,5u);
        r34_panel(0x73u,0x1Du);r34_panel(0x74u,0x3Bu);r34_panel(0x75u,0x13u);(void)field_dialogue_resume_saved_keep_panels(*camx,*camy);
        for(unsigned i=0u;i<5u;i++){uint16_t b=(uint16_t)(0xC002u+i*64u);ram8w(b,(uint8_t)(ram8r(b)&~2u));}
        field_event_redraw_explicit(*camx,*camy);waitvb();ram8w(0xECFEu,(uint8_t)(ram8r(0xECFEu)&~4u));
        r33_party_remove(4u);r33_party_remove(6u);
        /* Release 48: common departure guard protects the last-survivor case. */
        r34_queue(0xABu,2u,0u,0xACu,0x6Cu,0xB8u,0u,0u);return 1;
    }
    if(field_custom_resume_phase!=2u)return 0;
    field_palette_fade_in();ps4_event_flag_set(0x68u);ps4_event_flag_set(0x66u);ps4_event_flag_set(0x61u);
    field_custom_resume_event=0xFFu;field_custom_resume_phase=0u;return 1;
}
