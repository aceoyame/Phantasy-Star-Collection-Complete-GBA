/* Original $14 LutzRevelation ($77896), $15 FindingAirCastle ($77A2E),
 * $19 MeetingSeth ($77EAC). B0 uses the existing map continuation bytes. */
__attribute__((noinline))
static int r39_cutscene_lutz(unsigned* camx,unsigned* camy){
    if(field_event_party_slot_for_id(9u)<0)return 0;
    field_dialogue_show(0x2Au,*camx,*camy);r36_map_wait(0x1Du);
    field_event_move_party_id(lead_char_id,0x200u,0x1E0u,camx,camy);
    ram8w(0x500Au,0xDFu);ram8w(0xED52u,2u);field_palette_all64_to_black(2u);
    presentation_hide_field_oam();
    r34_queue(0xB0u,0u,0x16Fu,0x16Eu,0x3Cu,0x4Cu,12u,0u);return 1;
}
__attribute__((noinline))
static int r39_cutscene_air_castle(unsigned* camx,unsigned* camy){
    r36_bitmap_begin();r34_panels_begin();r34_panel(0x18Au,0u);
    r37_dialogue5(0x18u,*camx,*camy);ps4_event_flag_set(0x99u);
    return r36_spaceship_begin(0u,camx,camy);
}
__attribute__((noinline))
static int r39_cutscene_seth(unsigned* camx,unsigned* camy){
    r36_bitmap_begin();r34_panels_begin();r34_panel(0x118u,0u);
    r33_dialogue_tree(0x1FC920u);r37_dialogue5(0u,*camx,*camy);
    ram8w(0xF40Eu,10u);field_event_macro_add_attack(10u);
    r34_queue(0xB0u,2u,0u,0xFFFFu,0xEEu,0x136u,0u,0u);return 1;
}
__attribute__((noinline))
static int r39_story_resume(unsigned* camx,unsigned* camy){
    if(field_custom_resume_event!=0xB0u)return 0;
    if(field_custom_resume_phase==0u){
        int slot=field_event_party_slot_for_id(9u);if(slot<0)return 0;
        ram8w(0xED52u,2u);field_palette_first32_from_black_to_current(2u);
        field_event_move_party_id(lead_char_id,0x1E0u,0x1F0u,camx,camy);
        uint16_t b=(uint16_t)(0xC000u+(unsigned)slot*64u);
        ram8w(0xECFEu,(uint8_t)(ram8r(0xECFEu)|1u));
        field_event_move_party_id(9u,(uint16_t)(ram16r((uint16_t)(b+0x30u))+0x10u),ram16r((uint16_t)(b+0x34u)),camx,camy);
        static const uint8_t faces[5]={4u,8u,12u,8u,4u},holds[5]={0x27u,9u,9u,9u,0x3Bu};
        for(unsigned i=0u;i<5u;i++){r38_party_face(9u,faces[i],*camx,*camy,1u);r38_main_wait(holds[i],*camx,*camy);}
        field_event_move_party_id(9u,0x1F0u,0x1F0u,camx,camy);
        ram8w(0xECFEu,(uint8_t)(ram8r(0xECFEu)&(uint8_t)~1u));
        field_dialogue_show(0x2Fu,*camx,*camy);
        r36_bitmap_begin();r34_panels_begin();r34_panel(0x9Eu,0u);r34_vint_wait(0x63u);
        ram8w(0x500Au,0xF3u);(void)field_dialogue_resume_saved_keep_panels(*camx,*camy);
        field_palette_fade_out();field_cutscene_panels_clear();field_cutscene_panels=field_active_panel_count=0u;
        ps4_event_flag_set(0x97u);r34_queue(0xB0u,1u,0x16Fu,0x16Eu,0x3Cu,0x48u,0u,8u);return 1;
    }else if(field_custom_resume_phase==1u){
        /* RefreshMap already installed the source final party alignment. */
    }else if(field_custom_resume_phase==2u){
        /* Source sets SethJoined only after the Motavia RefreshMap. */
        ps4_event_flag_set(0xC1u);
    }else return 0;
    field_custom_resume_event=0xFFu;field_custom_resume_phase=0u;return 1;
}
