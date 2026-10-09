/* Native translations of Event_RuneFlaeli and Event_MeetingDorin. */
static void r31_party_hide(uint8_t id,unsigned hide){
    int slot=field_event_party_slot_for_id(id);if(slot<0)return;uint16_t b=(uint16_t)(0xC002u+(unsigned)slot*64u);uint8_t q=ram8r(b);ram8w(b,(uint8_t)(hide?q|2u:q&~2u));
}
static void r31_resume_dialogue(unsigned* camx,unsigned* camy){if(field_dialogue_saved_cursor)field_dialogue_resume_saved(*camx,*camy);}
static void r31_party_dest_update(void){
    for(unsigned slot=0u;slot<5u;slot++){
        uint8_t id=ram8r((uint16_t)(0xF40Au+slot));if(id==0xFFu)break;uint16_t b=(uint16_t)(0xC000u+slot*64u);
        int x=ram16r((uint16_t)(b+0x30u)),y=ram16r((uint16_t)(b+0x34u));int tx=ram16r((uint16_t)(b+0x38u)),ty=ram16r((uint16_t)(b+0x3Au));int d=0;unsigned speed=field_event_source_speed();
        if(x!=tx){d=tx-x;if(d>(int)speed)d=(int)speed;if(d<-(int)speed)d=-(int)speed;x+=d;ram16w((uint16_t)(b+6u),d>0?8u:12u);}
        else if(y!=ty){d=ty-y;if(d>(int)speed)d=(int)speed;if(d<-(int)speed)d=-(int)speed;y+=d;ram16w((uint16_t)(b+6u),d>0?0u:4u);}
        ram16w((uint16_t)(b+0x30u),(uint16_t)x);ram16w((uint16_t)(b+0x34u),(uint16_t)y);field_event_sync_party_carrier(slot,id,(uint8_t)(d!=0));
    }
}
static void r31_party_drift(uint8_t id,int dx,int dy,unsigned count,unsigned* camx,unsigned* camy){
    int slot=field_event_party_slot_for_id(id);if(slot<0)return;uint16_t b=(uint16_t)(0xC000u+(unsigned)slot*64u);
    for(unsigned f=0u;f<count;f++){ram16w((uint16_t)(b+0x30u),(uint16_t)(ram16r((uint16_t)(b+0x30u))+dx));ram16w((uint16_t)(b+0x34u),(uint16_t)(ram16r((uint16_t)(b+0x34u))+dy));field_event_sync_party_carrier((unsigned)slot,id,1u);field_event_choreography_tick(*camx,*camy);}
    ram16w((uint16_t)(b+0x38u),ram16r((uint16_t)(b+0x30u)));ram16w((uint16_t)(b+0x3Au),ram16r((uint16_t)(b+0x34u)));
}
__attribute__((noinline))
static int r31_event_rune_flaeli(unsigned* camx,unsigned* camy){
    if(field_event_party_slot_for_id(3u)<0)return 0;
    (void)load_nem_vram_auto(0x1D628Cu,0x179u);(void)load_nem_vram_auto(0x1D642Eu,0x191u);(void)load_nem_vram_auto(0x1D6A2Eu,0x1FBu);(void)load_nem_vram_auto(0x1D6BC8u,0x20Cu);
    for(unsigned i=0u;i<16u;i++)cram[48u+i]=rom16(0x1DE0B8u+i*2u);field_palette_mirror_live(64u);hw_load_palettes();ram16w(0xECF2u,0u);
    field_dialogue_show(0x26u,*camx,*camy);
    field_event_set_party_dest(0u,0x1F0u,0x260u);field_event_set_party_dest(1u,0x200u,0x260u);field_event_set_party_dest(2u,0x200u,0x270u);
    ram8w(0xECFEu,(uint8_t)(ram8r(0xECFEu)|5u));field_event_move_party_id(3u,0x1F0u,0x240u,camx,camy);
    for(unsigned f=0u;f<60u;f++){r31_party_dest_update();field_event_choreography_tick(*camx,*camy);}
    ram8w(0xECFEu,(uint8_t)(ram8r(0xECFEu)&~1u));
    for(unsigned id=0u;id<3u;id++)field_event_set_party_facing((uint8_t)id,4u);
    field_event_wait_updates(0x1Du,camx,camy);field_event_set_party_facing(3u,4u);field_event_wait_updates(0x3Bu,camx,camy);r31_resume_dialogue(camx,camy);
    int rs=field_event_party_slot_for_id(3u);uint16_t rb=(uint16_t)(0xC000u+(unsigned)rs*64u);r31_party_hide(3u,1u);
    r31_actor_spawn(0,0x214u,ram16r((uint16_t)(rb+0x30u)),ram16r((uint16_t)(rb+0x34u)),0u,0x179u);ram8w(0xECF3u,0x3Cu);
    for(unsigned f=0u;f<240u;f++){
        unsigned phase=ram8r(0xECF3u);
        if(phase){
            if(phase==0x3Bu)ram8w(0x500Au,0xC0u);
            for(unsigned q=0u;q<14u;q++){
                uint32_t a=0x6DA64u+q*6u;if(rom16(a)==phase)r31_actor_spawn(-1,0x220u,rom16(a+2u),rom16(a+4u),20u,0x20Cu);
                a=0x6DB20u+q*10u;if(rom8(a)==phase){int i=r31_actor_spawn(-1,0x21Cu,rom16(a+2u),rom16(a+4u),(uint8_t)rom8(a+1u),0x1FBu);if(i>=0){uint16_t b=(uint16_t)(0xC300u+(unsigned)i*64u);ram32w((uint16_t)(b+0x20u),(uint32_t)((int32_t)(int16_t)rom16(a+6u)*256));ram32w((uint16_t)(b+0x24u),(uint32_t)((int32_t)(int16_t)rom16(a+8u)*256));}}
            }
            if(phase==0x32u){(void)field_layout_write_world(15u*32u,14u*32u,1u,0x0Cu);(void)field_layout_write_world(16u*32u,14u*32u,1u,0x0Du);(void)field_layout_write_world(15u*32u,15u*32u,1u,0x14u);(void)field_layout_write_world(16u*32u,15u*32u,1u,0x15u);}
        }
        field_event_choreography_tick(*camx,*camy);if(ram8r(0xECF2u))ram8w(0xECF3u,(uint8_t)(ram8r(0xECF3u)-1u));
    }
    r31_party_hide(3u,0u);r31_actor_clear(0u);field_event_wait_updates(9u,camx,camy);field_event_set_party_facing(3u,0u);
    ram8w(0xECFEu,(uint8_t)(ram8r(0xECFEu)&~4u));field_event_move_camera_to(ram16r((uint16_t)(rb+0x30u)),ram16r((uint16_t)(rb+0x34u)),2u,camx,camy);r31_resume_dialogue(camx,camy);
    ram8w(0xECFEu,(uint8_t)(ram8r(0xECFEu)|2u));field_event_move_party_id(lead_char_id,0x1F0u,0x220u,camx,camy);ram8w(0xECFEu,(uint8_t)(ram8r(0xECFEu)&~2u));field_event_move_camera_to((uint16_t)chaz.x,(uint16_t)chaz.y,2u,camx,camy);field_event_move_party_id(lead_char_id,0x1F0u,0x210u,camx,camy);
    ram8w(0xECFEu,(uint8_t)(ram8r(0xECFEu)&~4u));ram16w(0xECF2u,0u);ps4_event_flag_set(0x13u);return 1;
}
__attribute__((noinline))
static int r31_event_meeting_dorin(unsigned* camx,unsigned* camy){
    ram8w(0x500Au,0xFBu);field_event_wait_updates(9u,camx,camy);ram8w(0x500Au,0xA6u);
    field_event_set_source_object_facing(0u,(uint8_t)((chaz.facing<<2)^4u),camx,camy);field_dialogue_show(0x18u,*camx,*camy);
    if(field_dialogue_last_stop==0xFFu||!field_dialogue_saved_cursor){ram8w(0x500Au,0xFBu);field_event_wait_updates(9u,camx,camy);ram8w(0x500Au,0x81u);return 1;}
    (void)load_nem_vram_auto(0x1D49B0u,0x4A5u);
    r31_actor_spawn(6,0x1A4u,0x1F0u,0x1E0u,0u,0x4A5u);r31_actor_hidden(6u,1u);r31_actor_update_one(6u);
    r31_actor_spawn(9,0x1A8u,0x1F3u,0x1D8u,4u,0x4A5u);r31_actor_hidden(9u,1u);r31_actor_update_one(9u);
    r31_actor_spawn(7,0x1ACu,0x1D0u,0x1E8u,12u,0x48Fu);r31_actor_hidden(7u,1u);r31_actor_update_one(7u);
    ram8w(0xECFEu,(uint8_t)(ram8r(0xECFEu)|1u));field_event_move_party_id(1u,0x200u,0x1E0u,camx,camy);field_event_set_party_facing(1u,12u);r31_party_drift(1u,-1,0,8u,camx,camy);
    r31_party_hide(1u,1u);r31_actor_hidden(6u,0u);hw_update_objects(*camx,*camy);waitvb();
    for(unsigned timer=0xCCu;timer;timer--){
        ram16w(0xC49Cu,(uint16_t)timer);
        if(timer==0x82u){r31_actor_hidden(0u,1u);r31_actor_hidden(7u,0u);ram8w(0x500Au,0xBEu);}
        if(timer==0x3Cu){r31_actor_hidden(0u,0u);r31_actor_hidden(7u,1u);}
        if(timer==0x50u)r31_actor_hidden(9u,0u);
        field_event_choreography_tick(*camx,*camy);
    }
    ram16w(0xC49Cu,0u);r31_resume_dialogue(camx,camy);field_event_set_source_object_facing(0u,8u,camx,camy);r31_resume_dialogue(camx,camy);
    r31_actor_hidden(9u,1u);r31_actor_hidden(7u,0u);field_objs[7].facing=16u;field_objs[7].anim=0u;field_objs[7].x=field_objs[7].y=0x1E0u;
    ram16w(0xC4C6u,16u);ram32w(0xC4F0u,0x1E00000u);ram32w(0xC4F4u,0x1E00000u);ram8w(0xC4D0u,0u);ram8w(0xC4D1u,0x1Eu);ram16w(0xC4DCu,10u);
    while(ram8r(0xC4D1u)){
        unsigned timer=ram16r(0xC4DCu)-1u;if(!timer){r31_actor_hidden(7u,!(ram8r(0xC4C2u)&2u));timer=10u;}ram16w(0xC4DCu,(uint16_t)timer);field_event_choreography_tick(*camx,*camy);
    }
    r31_resume_dialogue(camx,camy);
    int rs=field_event_party_slot_for_id(3u);unsigned face=0u;if(rs>=0&&field_obj_count){uint16_t b=(uint16_t)(0xC000u+(unsigned)rs*64u);unsigned ry=ram16r((uint16_t)(b+0x34u));if(ry<field_objs[0].y)face=4u;else if(ry==field_objs[0].y)face=8u;}
    field_event_set_source_object_facing(0u,(uint8_t)face,camx,camy);r31_resume_dialogue(camx,camy);
    r31_actor_clear(6u);r31_actor_clear(9u);r31_party_hide(1u,0u);field_event_choreography_tick(*camx,*camy);r31_party_drift(1u,1,0,8u,camx,camy);ps4_event_flag_set(0x36u);return 1;
}
