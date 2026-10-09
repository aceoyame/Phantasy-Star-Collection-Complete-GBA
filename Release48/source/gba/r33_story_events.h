/* Source Event_PsycoWandChest $6EA14. No story flag substitutes. */
static void r33_map_wait(unsigned count,unsigned* camx,unsigned* camy){
    for(unsigned n=0u;n<=count;n++){field_run_map_updates();field_event_redraw_explicit(*camx,*camy);waitvb();}
}
__attribute__((noinline))
static int r33_event_psyco_wand_chest(unsigned* camx,unsigned* camy){
    if(field_event_party_slot_for_id(3u)<0||!field_obj_count||field_objs[0].type!=0x1F8u)return 0;
    field_dialogue_show(7u,*camx,*camy);ram8w(0x500Au,0xFBu);
    ram8w(0xECFEu,(uint8_t)(ram8r(0xECFEu)|1u));
    field_event_move_party_id(3u,0x1E0u,(uint16_t)(ram16r(0xC034u)-0x10u),camx,camy);
    /* Source DBF waits 60 updates, extending one update whenever the final
     * frame is hidden. The flicker controller keeps its normal phase. */
    for(unsigned remaining=60u;remaining;remaining--){
        field_event_choreography_tick(*camx,*camy);
        if(remaining==1u&&(ram8r(0xC302u)&2u))remaining++;
    }
    ram8w(0x500Au,0xE6u);field_objs[0].facing=4u;ram16w(0xC306u,4u);
    while(ram16r(0xC300u))field_event_choreography_tick(*camx,*camy);
    for(unsigned q=0u;q<18u;q++){
        /* The accepted source ROM contains LEA 8(A0),A0 after reading each
         * eight-byte record. Preserve its literal 16-byte stride, including
         * the source's reads beyond the 144-byte coordinate table. */
        uint32_t a=0x6EBD2u+q*16u;unsigned i=q+2u;
        r31_actor_spawn((int)i,0x1FCu,rom16(a),rom16(a+2u),(uint8_t)(q*4u),0x2E6u);
        uint16_t b=(uint16_t)(0xC300u+i*64u);
        ram32w((uint16_t)(b+0x20u),(uint32_t)((int32_t)(int16_t)rom16(a+4u)*256));
        ram32w((uint16_t)(b+0x24u),(uint32_t)((int32_t)(int16_t)rom16(a+6u)*256));
    }
    while(ram16r(0xC380u))field_event_choreography_tick(*camx,*camy);
    r33_map_wait(0x3Bu,camx,camy);ram8w(0x500Au,0xFBu);waitvb();
    field_dialogue_show(8u,*camx,*camy);
    field_event_move_camera_to(ram16r(0xC030u),0x160u,1u,camx,camy);
    (void)load_nem_vram_auto(0x1D5DF0u,0x2E6u);
    for(unsigned i=0u;i<16u;i++)cram[48u+i]=rom16(0x1DE098u+i*2u);
    field_palette_mirror_live(64u);hw_load_palettes();
    r31_actor_spawn(0,0x20Cu,0x1D8u,0x160u,0u,0x2E6u);ram8w(0x500Au,0xA3u);
    while(field_objs[0].facing!=12u||!(ram8r(0xC302u)&4u))field_event_choreography_tick(*camx,*camy);
    ram8w(0x500Au,0xA8u);r33_map_wait(0u,camx,camy);
    r31_resume_dialogue(camx,camy);ps4_event_flag_set(0x69u);
    return field_event_run_battle_request(5u,camx,camy);
}
