/* Event_AlysFound: shared by the map trigger and explicit event dispatch. */
__attribute__((noinline))
static int r30_event_alys_found(unsigned* camx,unsigned* camy){
    int oi=field_find_object_type(0x0068u);if(oi<0)return 0;
    field_animate_alys_to_chaz((unsigned)oi,*camx,*camy);
    field_dialogue_show(field_objs[oi].dialog,*camx,*camy);
    uint16_t ax=field_objs[oi].x,ay=field_objs[oi].y,cx=(uint16_t)chaz.x,cy=(uint16_t)chaz.y;
    field_event_clear_source_object((unsigned)oi);ram8w(0xF40Au,1u);ram8w(0xF40Bu,0u);lead_char_id=1u;
    chaz.x=ax;chaz.y=ay;chaz.facing=3u;chaz.anim=0u;chaz.step_left=0u;field_event_macro_add_attack(1u);
    field_followers_sync(1);struct PartyFollowerLite* f=field_event_follower_for_id(0u);if(f){f->x=f->tx=cx;f->y=f->ty=cy;}
    hw_chaz_key=0xFFFFu;field_event_move_camera_to(ax,ay,2u,camx,camy);field_sync_native_party_to_source();ps4_event_flag_set(8u);return 1;
}
/* Event_GettingLandRover scrolls only the BG conveyor, one source pixel per
 * update. The FG and party remain stationary. Stream one new column per tile. */
__attribute__((noinline))
static void r30_land_rover_conveyor(unsigned camx,unsigned camy){
    int bgx=(int)camx,last=(int)(camx>>3);unsigned span=((unsigned)field_map.row_bg+1u)*32u;
    for(unsigned f=0u;f<360u;f++){
        if(bgx>0)--bgx;int tx=bgx>=0?bgx>>3:-((7-bgx)>>3);
        if(tx!=last){for(int y=0;y<32;y++){int py=(int)(camy>>3)+y;unsigned wx=field_wrap_pixel_int(tx*8,span),wy=(unsigned)py*8u;
            uint16_t e=field_entry(0xB000u,wx,wy),g=md_entry_to_gba(e);unsigned mi=((unsigned)py&31u)*32u+((unsigned)tx&31u);
            BG1_MAP[mi]=(e&0x8000u)?g:0u;BG3_MAP[mi]=(e&0x8000u)?0u:g;}
            last=tx;
        }
        ram32w(0xEC58u,bgx?0xFFFF0000u:0u);ram16w(0xEF9Cu,(uint16_t)bgx);r30_tile_animation_update();field_run_map_updates();
        REG_BG1HOFS=(uint16_t)(bgx&255);REG_BG3HOFS=(uint16_t)(bgx&255);waitvb();
    }
}
__attribute__((noinline))
static int r30_event_land_rover(unsigned* camx,unsigned* camy){
    field_dialogue_show(9u,*camx,*camy);
    static const uint8_t ids[4]={0u,4u,5u,6u};static const uint16_t xs[4]={0x1D0u,0x200u,0x1F0u,0x1E0u},ys[4]={0x1D0u,0x1B0u,0x1E0u,0x1F0u};
    ram8w(0xECE0u,0u);ram8w(0xECFEu,(uint8_t)(ram8r(0xECFEu)|1u));
    for(unsigned i=0u;i<4u;i++)field_event_set_party_dest(ids[i],xs[i],ys[i]);
    for(unsigned f=0u;f<100u;f++){
        for(unsigned i=0u;i<4u;i++){int slot=field_event_party_slot_for_id(ids[i]);if(slot<0)continue;
            uint16_t b=(uint16_t)(0xC000u+(unsigned)slot*64u);int x=ram16r((uint16_t)(b+0x30u)),y=ram16r((uint16_t)(b+0x34u));unsigned face=ram16r((uint16_t)(b+6u));uint8_t moved=1u;
            if(x<xs[i]){++x;face=8u;}else if(x>xs[i]){--x;face=12u;}else if(y<ys[i]){++y;face=0u;}else if(y>ys[i]){--y;face=4u;}else moved=0u;
            ram16w((uint16_t)(b+6u),(uint16_t)face);ram16w((uint16_t)(b+0x30u),(uint16_t)x);ram16w((uint16_t)(b+0x34u),(uint16_t)y);field_event_sync_party_carrier((unsigned)slot,ids[i],moved);
        }
        field_event_choreography_tick(*camx,*camy);
    }
    for(unsigned i=0u;i<4u;i++)field_event_set_party_facing(ids[i],0u);
    if(field_dialogue_saved_cursor)field_dialogue_resume_saved(*camx,*camy);
    ram8w(0x500Au,0xF8u);field_event_wait_updates(0x27u,camx,camy);field_event_move_camera_to(0x1E0u,0x200u,1u,camx,camy);
    ram8w(0x500Au,0xE7u);r30_land_rover_conveyor(*camx,*camy);ram8w(0x500Au,0xFDu);field_event_set_party_facing(6u,4u);field_event_wait_updates(0x1Du,camx,camy);
    if(field_dialogue_saved_cursor)field_dialogue_resume_saved(*camx,*camy);
    ram8w(0xECFEu,(uint8_t)(ram8r(0xECFEu)&~1u));field_event_move_party_id(lead_char_id,0x1E0u,0x190u,camx,camy);ram8w(0xECE0u,1u);
    field_palette_fade_out();ram8w(0x500Au,0xFBu);field_event_wait_updates(0x0Eu,camx,camy);ram8w(0x500Au,0x8Du);ram8w(0xECECu,0x8Du);
    ram16w(0xF43Cu,1u);field_cutscene_reload(0u,0xB7u,0xE4u,0x160u,4u,4u);field_custom_resume_event=0x2Bu;field_custom_resume_phase=1u;return 1;
}
__attribute__((noinline))
static int r30_event_land_rover_resume(unsigned* camx,unsigned* camy){
    ram16w(0xC000u,0xB0u);for(unsigned i=1u;i<5u;i++)ram16w((uint16_t)(0xC000u+i*64u),0u);
    field_follower_count=0u;chaz.x=0x720;chaz.y=0xB00;chaz.facing=1u;ram16w(0xC030u,0x720u);ram16w(0xC034u,0xB00u);hw_chaz_key=0xFFFFu;
    r30_vehicle_palette(1u);field_event_redraw(camx,camy);field_palette_fade_in();field_dialogue_show(0x0Cu,*camx,*camy);
    ps4_inventory_remove_first(0x99u);ps4_inventory_add(0x96u);ps4_event_flag_set(0x44u);field_custom_resume_event=0xFFu;field_custom_resume_phase=0u;return 1;
}
