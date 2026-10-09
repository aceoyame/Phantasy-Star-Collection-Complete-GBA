/* Original $10 Landale ($77150) / $11 DarkForce1Defeated ($771D2).
 * AE carries two real RefreshMap continuations in existing native bytes. */
static void r37_dialogue5(unsigned id,unsigned camx,unsigned camy){
    ram8w(0x500Au,0xF3u);field_dialogue_show_keep_panels(id,camx,camy);
    field_palette_fade_out();field_cutscene_panels_clear();
    field_cutscene_panels=0u;field_active_panel_count=0u;
}
/* Event_MoveCamera: X then Y, shortest 12-bit route, FG/BG together.
 * Source map updates precede camera motion and object updates on each VB. */
__attribute__((noinline))
static void r37_dezo_camera(uint16_t x,uint16_t y,unsigned speed,unsigned* camx,unsigned* camy){
    unsigned targets[2]={((unsigned)x-0x98u)&0xFFFu,((unsigned)y-0x58u)&0xFFFu};
    for(unsigned axis=0u;axis<2u;axis++){
        uint16_t pos=(uint16_t)(axis?0xEF90u:0xEF94u),bg=(uint16_t)(axis?0xEF98u:0xEF9Cu);
        uint16_t step=(uint16_t)(axis?0xEC54u:0xEC50u),bstep=(uint16_t)(axis?0xEC5Cu:0xEC58u);
        int delta=(int)targets[axis]-(int)ram16r(pos);if(delta>0x7FF)delta-=0x1000;else if(delta<-0x7FF)delta+=0x1000;
        int v=delta<0?-(int)speed:(int)speed;unsigned n=(unsigned)(delta<0?-delta:delta);
        while(n){
            field_run_map_updates();ram32w(step,(uint32_t)(v*65536));ram32w(bstep,(uint32_t)(v*65536));
            /* Dezolis is a 4096x4096 wrapped overworld; retain fractions. */
            ram32w(pos,(ram32r(pos)+(uint32_t)(v*65536))&0x0FFFFFFFu);
            ram32w(bg,(ram32r(bg)+(uint32_t)(v*65536))&0x0FFFFFFFu);
            *camx=ram16r(0xEF94u);*camy=ram16r(0xEF90u);field_update_npcs(*camx,*camy);r35_camera_present(*camx,*camy);waitvb();
            n=n>speed?n-speed:0u;
        }
    }
}
__attribute__((noinline))
static int r37_dezo_spaceport(unsigned* camx,unsigned* camy){
    r36_field_camera_begin(*camx,*camy);
    /* All three streams use their original RAM destinations. Immutable OBJ
     * poses sample the same art without sharing live field tile storage. */
    (void)nemesis_fast(0x1D3C1Eu,&work_ram[0u],0x800u);
    (void)nemesis_fast(0x1D4234u,&work_ram[0x800u],0x800u);
    (void)nemesis_fast(0x1D46C6u,&work_ram[0x1000u],0x800u);
    ram32w(0xEF84u,ram32r(0xEF94u));ram32w(0xEF80u,ram32r(0xEF90u));
    ram32w(0xEF8Cu,ram32r(0xEF9Cu));ram32w(0xEF88u,ram32r(0xEF98u));
    r37_dezo_camera(0xC0u,0x480u,1u,camx,camy);
    for(int counter=0x170;counter>=0;counter--){
        if(!(counter&7))ram8w(0x500Au,0xDDu);
        field_run_map_updates();field_update_npcs(*camx,*camy);r35_camera_present(*camx,*camy);waitvb();
        /* Source launches after publishing this frame, in table order. */
        for(unsigned k=0u;k<24u;k++){
            uint32_t p=0x6C1BCu+k*8u;if(rom16(p)!=(unsigned)counter)continue;
            int i=r31_actor_spawn(-1,0x7Cu,(uint16_t)rom16(p+2u),(uint16_t)rom16(p+4u),4u,(uint16_t)rom16(p+6u));
            if(i>=0){uint16_t b=(uint16_t)(0xC300u+(unsigned)i*64u);ram32w((uint16_t)(b+0x20u),0xFFFFC000u);ram32w((uint16_t)(b+0x24u),0x4000u);}
            break;
        }
        if(counter==0xF8){r31_actor_spawn(-1,0x78u,0xC0u,0x480u,0u,0x4AEu);r31_actor_spawn(-1,0x84u,0xC0u,0x480u,12u,0x49Eu);}
        if(counter==0x3D)(void)field_layout_write_world(6u*32u,0x24u*32u,1u,0x2Cu);
    }
    r37_dezo_camera(ram16r(0xC030u),ram16r(0xC034u),2u,camx,camy);ps4_event_flag_set(0x82u);return 1;
}
__attribute__((noinline))
static int r37_cutscene_landale(unsigned* camx,unsigned* camy){
    r36_bitmap_begin();r34_panels_begin();r34_panel(0x8Bu,0x1Du);r37_dialogue5(0x24u,*camx,*camy);
    r34_queue(0xAEu,0u,1u,0x15Fu,0x12u,0x92u,8u,0xCu);return 1;
}
static void r37_radar(void){
    for(uint32_t p=0x77332u;rom8(p)!=0xFFu;p+=4u){
        unsigned color=16u+rom8(p+1u)/2u;cram[color]=(uint16_t)rom16(p+2u);
        field_dialogue_commit_palette();r34_vint_wait(rom8(p));
    }
}
__attribute__((noinline))
static int r37_cutscene_dark_force1(unsigned* camx,unsigned* camy){
    r36_bitmap_begin();r34_panels_begin();r34_panel(0x8Eu,0u);r37_dialogue5(7u,*camx,*camy);
    r36_map_wait(0x3Bu);ram8w(0x500Au,0x9Bu);r36_map_wait(0x3Bu);
    for(unsigned i=0u;i<0x1000u;i++)ram8w((uint16_t)(0xC000u+i),0u);
    for(unsigned i=0u;i<field_obj_count;i++){field_objs[i].active=0u;field_objs[i].type=0u;field_obj_art_stamp[i].epoch=0u;}
    ram32w(0xEF90u,0u);ram8w(0xF400u,3u);r36_space_pass();
    r34_queue(0xAEu,1u,0x18Eu,0xFFFFu,0x3Eu,0x20u,0u,8u);return 1;
}
__attribute__((noinline))
static int r37_story_resume(unsigned* camx,unsigned* camy){
    if(field_custom_resume_event!=0xAEu)return 0;
    if(field_custom_resume_phase==0u){
        ram8w(0x500Au,0x99u);ram8w(0xECECu,0x99u);field_palette_fade_in();
        (void)r37_dezo_spaceport(camx,camy);
    }else if(field_custom_resume_phase==1u){
        ram8w(0x500Au,0x89u);ram8w(0xECECu,0x89u);r36_bitmap_begin();r34_panels_begin();
        r34_panel(0x92u,0x3Bu);r34_panel(0x93u,0x77u);ram8w(0x500Au,0xF8u);r37_radar();ram8w(0x500Au,0xFDu);waitvb();
        r37_dialogue5(8u,*camx,*camy);
        /* The normal source route owns a Canceller, guaranteeing a slot even
         * when all 40 inventory positions are occupied. Remove only one. */
        (void)ps4_inventory_remove_first(0x9Au);(void)ps4_inventory_add(0x97u);ps4_event_flag_set(0x89u);
        r34_field_restore(*camx,*camy);field_palette_fade_in();
    }else return 0;
    field_custom_resume_event=0xFFu;field_custom_resume_phase=0u;return 1;
}
