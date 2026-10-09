/* Source $0C MeetingWren ($75FC8) and $0F CrashLanding ($7658A).
 * AC owns three real map continuations. All temporary state is on the stack
 * or in the original PSIV object/camera records; no new EWRAM globals. */
__attribute__((noinline))
static int r35_cutscene_meeting_wren(unsigned* camx,unsigned* camy){
    field_event_set_source_object_facing(0u,(uint8_t)(ram16r(0xC006u)^4u),camx,camy);
    r34_panels_begin();r34_panel(0x76u,0x13u);field_dialogue_show_keep_panels(1u,*camx,*camy);
    /* Source creates Character_4 before CalcPartyNumber appends its ID. */
    ram16w(0xC0C0u,0x20u);ram32w(0xC0F0u,0x1F00000u);ram32w(0xC0F4u,0xC00000u);
    ram16w(0xC0F8u,0x1F0u);ram16w(0xC0FAu,0xC0u);ram16w(0xC0C6u,0u);
    unsigned n=0u;while(n<5u&&ram8r((uint16_t)(0xF40Au+n))!=0xFFu)n++;
    if(n<5u){ram8w((uint16_t)(0xF40Au+n),7u);field_event_macro_add_attack(7u);}
    r34_sync_preserved_party();field_event_redraw_explicit(*camx,*camy);waitvb();
    for(unsigned q=0u;q<32u;q++)ram8w((uint16_t)(0xC300u+q),0u);
    if(field_obj_count){field_objs[0].active=0u;field_objs[0].type=0u;}
    ps4_event_flag_set(0x70u);field_palette_copy_saved_to_live();r34_field_restore(*camx,*camy);return 1;
}
/* RefreshMap bit 3 skips both party and NPC creation in the original engine.
 * The manga framebuffer is dead after this map handoff; borrow it only for
 * the synchronous reload snapshot instead of enlarging EWRAM or the stack. */
__attribute__((noinline))
static int r35_load_preserving_objects(uint16_t map){
    unsigned count=field_obj_count;uint8_t* saved=field_pause_fb;
    for(unsigned i=0u;i<0xB00u;i++)saved[i]=ram8r((uint16_t)(0xC000u+i));
    const uint8_t* objs=(const uint8_t*)(const void*)field_objs;
    for(unsigned i=0u;i<sizeof field_objs;i++)saved[0xB00u+i]=objs[i];
    int rc=ps4_load_field_map(map);
    for(unsigned i=0u;i<0xB00u;i++)ram8w((uint16_t)(0xC000u+i),saved[i]);
    uint8_t* dst=(uint8_t*)(void*)field_objs;for(unsigned i=0u;i<sizeof field_objs;i++)dst[i]=saved[0xB00u+i];
    field_obj_count=count;return rc;
}
/* Original bounded-map camera arithmetic uses a 320x224 source viewport.
 * This is scene simulation, independent of the native 240x160 projection. */
static void r35_camera_axis(uint16_t pos,uint16_t step,unsigned cells,unsigned view){
    int32_t q=(int32_t)ram32r(pos)+(int32_t)ram32r(step);
    int32_t limit=(int32_t)(cells*32u-view)*65536;
    if(q<0){q=0;ram32w(step,0u);}else if(q>limit){q=limit;ram32w(step,0u);}
    ram32w(pos,(uint32_t)q);
}
/* Scripted FG/BG steps must survive presentation. The normal scroll helper
 * derives BG from map update rates, whereas this scene authors both cameras.
 * Reuse the existing rings and HBlank tables without that rate conversion. */
__attribute__((noinline))
static void r35_camera_present(unsigned camx,unsigned camy){
    field_publish_layout_dirty((int)(camx>>3),(int)(camy>>3));
    unsigned next=(unsigned)(field_hblank_active_index^1u);uint16_t* regs=field_hblank_table(next);
    unsigned bgx=ram16r(0xEF9Cu),bgy=ram16r(0xEF98u);
    for(unsigned y=0u;y<FIELD_HBLANK_ROWS;y++){
        uint16_t* r=regs+y*8u;r[0]=(uint16_t)(camx&511u);r[1]=(uint16_t)(camy&255u);
        r[2]=(uint16_t)(bgx&511u);r[3]=(uint16_t)(bgy&255u);r[4]=r[0];r[5]=r[1];r[6]=r[2];r[7]=r[3];
    }
    int atx=(int)(camx>>3),btx=(int)(bgx>>3),ty=(int)(camy>>3),bty=(int)(bgy>>3);
    if(hw_hscroll_ring_valid){
        atx=field_near_tile_origin(atx,hw_hsa_tx,((unsigned)field_map.row_fg+1u)*4u);
        btx=field_near_tile_origin(btx,hw_hsb_tx,((unsigned)field_map.row_bg+1u)*4u);
        hw_stream_hscroll_plane(0u,atx,ty);hw_stream_hscroll_plane(1u,btx,bty);
    }else {hw_fill_hscroll_plane(0u,atx,ty);hw_fill_hscroll_plane(1u,btx,bty);hw_hscroll_ring_valid=1u;}
    hw_last_camx=camx;hw_last_camy=camy;hw_last_cam_valid=1u;field_hblank_active_index=(uint8_t)next;
    field_hscroll_dirty=0u;field_hscroll_prepared=0u;field_hscroll_rearm();hw_update_objects(camx,camy);
}
/* Source table has 44 timed snow launches. Field_LoadObject scans from slot 3. */
static void r35_crash_snow(unsigned counter){
    for(unsigned k=0u;k<44u;k++){
        uint32_t p=0x76EDAu+k*12u;if(rom16(p)!=counter)continue;
        unsigned i=3u;while(i<FIELD_MAX_OBJECTS&&i<field_obj_count&&field_objs[i].type)i++;
        if(i>=FIELD_MAX_OBJECTS)return;
        int slot=r31_actor_spawn((int)i,0x1F4u,rom16(p+2u),rom16(p+4u),(uint8_t)rom16(p+10u),0x2C3u);
        if(slot>=0){uint16_t b=(uint16_t)(0xC300u+(unsigned)slot*64u);
            ram32w((uint16_t)(b+0x20u),(uint32_t)rom16(p+6u)<<8);
            ram32w((uint16_t)(b+0x24u),0u-((uint32_t)rom16(p+8u)<<8));ram8w(0x500Au,0xBEu);}
        return;
    }
}
__attribute__((noinline))
static void r35_crash_pan(unsigned* camx,unsigned* camy){
    int32_t vx=0x40000,vy=-0x20000,bvx=0x60000,bvy=-0x40000;unsigned counter=0u;
    ram8w(0xECF2u,10u);ram8w(0xECF3u,0u);
    field_hscroll_mode=4u;hw_hscroll_ring_valid=0u;
    REG_BG0CNT=(uint16_t)((16u<<8)|(1u<<14));REG_BG1CNT=(uint16_t)((18u<<8)|(1u<<14));
    REG_BG2CNT=(uint16_t)(2u|(20u<<8)|(1u<<14));REG_BG3CNT=(uint16_t)(3u|(22u<<8)|(1u<<14));
    ram32w(0xEF94u,(uint32_t)*camx<<16);ram32w(0xEF90u,(uint32_t)*camy<<16);
    ram32w(0xEF9Cu,(uint32_t)*camx<<16);ram32w(0xEF98u,(uint32_t)*camy<<16);
    do{
        ram32w(0xEC50u,(uint32_t)vx);ram32w(0xEC54u,(uint32_t)vy);
        ram32w(0xEC58u,(uint32_t)bvx);ram32w(0xEC5Cu,(uint32_t)bvy);
        if(counter)r35_crash_snow(counter);
        r35_camera_axis(0xEF94u,0xEC50u,(unsigned)field_map.row_fg+1u,320u);
        r35_camera_axis(0xEF9Cu,0xEC58u,(unsigned)field_map.row_bg+1u,320u);
        r35_camera_axis(0xEF90u,0xEC54u,(unsigned)field_map.col_fg+1u,224u);
        r35_camera_axis(0xEF98u,0xEC5Cu,(unsigned)field_map.col_bg+1u,224u);
        *camx=ram16r(0xEF94u);*camy=ram16r(0xEF90u);field_update_npcs(*camx,*camy);
        uint8_t t=(uint8_t)(ram8r(0xECF2u)-1u);ram8w(0xECF2u,t);
        if(!t){ram8w(0xECF2u,10u);unsigned old=ram8r(0xECF3u)&1u;ram8w(0xECF3u,(uint8_t)(ram8r(0xECF3u)^1u));cram[1u]=old?0xEu:0u;}
        field_palette_mirror_live(64u);hw_load_palettes();r35_camera_present(*camx,*camy);waitvb();
        vx-=0x178;if(vx<0)vx=0;bvy+=0x1000;if(bvy>=0)bvy=0;
        if(ram16r(0xEF90u)<=0x78u){ram16w(0xEF90u,0x78u);vy=0;counter++;}
        if(ram16r(0xEF94u)>=0x1F8u){bvy=0;bvx=vx;ram32w(0xEF9Cu,ram32r(0xEF94u));ram32w(0xEF98u,ram32r(0xEF90u));}
    }while(ram16r(0xEF94u)<0x558u);
    *camx=ram16r(0xEF94u);*camy=ram16r(0xEF90u);
}
static void r35_crash_door(unsigned* camx,unsigned* camy){
    uint32_t p=0x76E58u;while(rom8(p)!=0xFFu){
        for(unsigned i=0u;i<4u;i++)(void)field_layout_write_world((47u+(i&1u))*32u,(9u+(i>>1))*32u,1u,(uint8_t)rom8(p++));
        r33_map_wait(5u,camx,camy);
    }
}
static void r35_chaz_drift(unsigned* camx,unsigned* camy){
    int slot=field_event_party_slot_for_id(0u);if(slot<0)return;uint16_t b=(uint16_t)(0xC000u+(unsigned)slot*64u);
    ram32w((uint16_t)(b+0x20u),0x8000u);ram32w((uint16_t)(b+0x24u),0u);
    for(unsigned f=0u;f<32u;f++){
        ram32w((uint16_t)(b+0x30u),ram32r((uint16_t)(b+0x30u))+0x8000u);
        field_event_sync_party_carrier((unsigned)slot,0u,1u);field_run_map_updates();field_event_redraw_explicit(*camx,*camy);waitvb();
    }
    ram32w((uint16_t)(b+0x20u),0u);ram32w((uint16_t)(b+0x24u),0u);
    ram16w((uint16_t)(b+0x38u),ram16r((uint16_t)(b+0x30u)));ram16w((uint16_t)(b+0x3Au),ram16r((uint16_t)(b+0x34u)));
}
__attribute__((noinline))
static int r35_cutscene_crash_landing(unsigned* camx,unsigned* camy){
    r34_panels_begin();r34_panel(0x80u,0u);
    static const uint8_t first[][2]={{0xBEu,0x13u},{0xCEu,0x13u},{0xD3u,0x1Du},{0xD3u,7u},{0xCEu,9u},{0xBFu,0x13u}};
    for(unsigned i=0u;i<6u;i++){ram8w(0x500Au,first[i][0]);r34_vint_wait(first[i][1]);}
    ram8w(0x500Au,0xBEu);field_dialogue_show_keep_panels(3u,*camx,*camy);r34_panel(0x86u,0u);
    static const uint8_t last[][2]={{0xC0u,7u},{0xBFu,9u},{0xBFu,5u},{0xADu,9u},{0xC0u,5u},{0xC0u,0xB3u}};
    for(unsigned i=0u;i<6u;i++){ram8w(0x500Au,last[i][0]);r34_vint_wait(last[i][1]);}
    ram8w(0xED52u,9u);field_palette_all64_to_black(9u);field_cutscene_panels_clear();ram8w(0x500Au,0xFBu);waitvb();
    r34_panels_begin();r33_map_wait(0x77u,camx,camy);r34_panel(0x87u,0u);ram8w(0x500Au,0x99u);waitvb();
    r33_dialogue_tree(0x1E99F0u);field_palette_fade_in();field_dialogue_show_keep_panels(7u,*camx,*camy);field_palette_fade_out();
    r34_queue(0xACu,0u,1u,0x18Cu,0x20u,0xBAu,0u,0u);return 1;
}
__attribute__((noinline))
static int r35_crash_resume(unsigned* camx,unsigned* camy){
    if(field_custom_resume_event!=0xACu)return 0;
    if(field_custom_resume_phase==0u){
        ram8w(0xF400u,1u);for(unsigned i=0u;i<5u;i++){uint16_t b=(uint16_t)(0xC002u+i*64u);ram8w(b,(uint8_t)(ram8r(b)|2u));}
        field_event_redraw_explicit(*camx,*camy);waitvb();r33_map_wait(0x77u,camx,camy);field_palette_fade_in();
        field_event_move_camera_to(0x240u,0x5D0u,2u,camx,camy);field_palette_fade_out();
        r34_queue(0xACu,1u,0x14Cu,1u,6u,0x2Eu,0u,0u);return 1;
    }
    if(field_custom_resume_phase==1u){
        for(unsigned i=0u;i<4u;i++){uint8_t id=ram8r((uint16_t)(0xF40Au+i));r33_party_position(id,0x5F0u,0x120u,0u);r31_party_hide(id,1u);}
        field_event_redraw_explicit(*camx,*camy);waitvb();(void)load_nem_vram_auto(0x1D547Cu,0x2C3u);ram8w(0xECF4u,1u);field_palette_fade_in();
        r35_crash_pan(camx,camy);ram8w(0x500Au,0xADu);r33_map_wait(0x27u,camx,camy);ram8w(0x500Au,0xE4u);
        for(uint32_t p=0x7713Eu;rom8(p)!=0xFFu;p+=2u){cram[1u]=(uint16_t)rom8(p+1u);field_palette_mirror_live(64u);hw_load_palettes();r34_vint_wait(rom8(p));}
        r33_map_wait(0x3Cu,camx,camy);
        for(unsigned i=0u;i<4u;i++)ram32w((uint16_t)(0xEC50u+i*4u),0u);
        ram32w(0xEF94u,0x5580000u);ram32w(0xEF9Cu,0x5580000u);ram32w(0xEF90u,0x780000u);ram32w(0xEF98u,0x780000u);
        *camx=0x558u;*camy=0x78u;field_event_move_camera_to(0x5F0u,0x120u,1u,camx,camy);
        static const uint16_t xs[4]={0x5F0u,0x5E0u,0x610u,0x600u};
        for(unsigned i=0u;i<4u;i++){uint8_t id=ram8r((uint16_t)(0xF40Au+i));r33_party_position(id,xs[i],0x120u,0u);r31_party_hide(id,0u);}
        field_event_redraw_explicit(*camx,*camy);waitvb();r34_object_move(0u,0x5D0u,0x190u,0u,camx,camy);
        r33_map_wait(0x3Bu,camx,camy);ram8w(0x500Au,0xE5u);r35_crash_door(camx,camy);r33_map_wait(0x3Bu,camx,camy);
        ram8w(0xECE0u,0u);field_event_move_party_id(lead_char_id,0x5F0u,0x140u,camx,camy);ram8w(0xECE0u,1u);
        r34_panels_begin();ram8w(0x500Au,0xA6u);ram8w(0xECECu,0xA6u);r34_panel(0x88u,0u);
        r33_dialogue_tree(0x1E99F0u);field_dialogue_show_keep_panels(8u,*camx,*camy);
        r33_party_position(0u,0x5F0u,0x190u,12u);r33_party_position(5u,0x610u,0x190u,12u);
        r33_party_position(3u,0x600u,0x1A0u,12u);r33_party_position(7u,0x600u,0x150u,4u);
        field_event_set_source_object_facing(0u,8u,camx,camy);ps4_event_flag_set(0x85u);
        ram16w(0xEC28u,0x14Cu);ram16w(0xEC2Au,1u);field_event_set_mapload_bit(3u);
        field_custom_resume_phase=2u;field_event_reload_pending=1u;field_dialogue_saved_cursor=0;field_dialogue_last_stop=0u;return 1;
    }
    if(field_custom_resume_phase!=2u)return 0;
    ram8w(0xEC4Eu,(uint8_t)(ram8r(0xEC4Eu)&~8u));field_palette_fade_in();r33_map_wait(0x3Bu,camx,camy);
    r34_dialogue_segment(0x1E99F0u,8u,1u);r31_resume_dialogue(camx,camy);
    ram8w(0xECFEu,(uint8_t)(ram8r(0xECFEu)|1u));ram8w(0xECE0u,0u);field_event_move_party_id(7u,0x600u,0x180u,camx,camy);
    field_event_set_party_facing(0u,4u);r31_resume_dialogue(camx,camy);
    field_event_set_party_facing(0u,12u);field_event_set_party_facing(7u,12u);r31_resume_dialogue(camx,camy);
    field_event_set_party_dest(5u,0x620u,0x190u);field_event_set_party_dest(7u,0x610u,0x180u);
    ram8w(0xECE0u,0u);field_event_move_party_id(3u,0x610u,0x1A0u,camx,camy);ram8w(0xECE0u,1u);
    field_event_set_party_facing(5u,12u);field_event_set_party_facing(7u,0u);field_event_set_party_facing(3u,4u);
    r35_chaz_drift(camx,camy);field_event_set_party_facing(0u,8u);r31_resume_dialogue(camx,camy);
    r33_map_wait(0x3Bu,camx,camy);field_event_set_party_facing(7u,12u);field_event_set_party_facing(3u,12u);field_event_set_party_facing(0u,12u);
    r31_resume_dialogue(camx,camy);ram8w(0xECFEu,(uint8_t)(ram8r(0xECFEu)&~1u));
    for(unsigned q=0u;q<64u;q++)ram8w((uint16_t)(0xC100u+q),ram8r((uint16_t)(0xC300u+q)));
    for(unsigned q=0u;q<32u;q++)ram8w((uint16_t)(0xC300u+q),0u);
    if(field_obj_count){field_objs[0].active=0u;field_objs[0].type=0u;}
    ram8w(0xF40Eu,8u);field_event_macro_add_attack(8u);r34_sync_preserved_party();
    field_event_move_party_id(lead_char_id,(uint16_t)chaz.x,(uint16_t)(chaz.y+0x10),camx,camy);
    r33_dialogue_tree(0x1E99F0u);ps4_event_flag_set(0x88u);ram8w(0xECF4u,0u);
    field_custom_resume_event=0xFFu;field_custom_resume_phase=0u;return 1;
}
