/* Original $16 LashiecDefeated ($77A68), $17 DarkForce2Defeated ($77BDA),
 * $18 GumbiousBishop ($77DC6). B1 reuses the native continuation bytes. */
static void r40_panels_init(void){
    r36_bitmap_begin();field_palette_fade_out();presentation_hide_field_oam();field_cutscene_panels_clear();
    for(unsigned i=0u;i<64u;i++)cram[i]=rom16(0x9F2BCu+i*2u);
    field_palette_mirror_live(64u);hw_load_palettes();
}
/* Source multiple Panel_Destroy calls share one DMAPlanes_VInt publication. */
static void r40_panels_pop(unsigned count){
    if(count>field_active_panel_count)count=field_active_panel_count;
    field_active_panel_count-=count;panel_rebuild_scaled();panel_present_scaled();
}
static void r40_dialogue3(unsigned id,unsigned camx,unsigned camy){
    ram8w(0x500Au,0xF3u);field_dialogue_show_keep_panels(id,camx,camy);
}
static void r40_resume_dialogue(unsigned finish,unsigned camx,unsigned camy){
    ram8w(0x500Au,0xF3u);(void)field_dialogue_resume_saved_keep_panels(camx,camy);
    if(finish){field_palette_fade_out();field_cutscene_panels_clear();field_cutscene_panels=field_active_panel_count=0u;}
}
static void r40_clear_field_objects(void){
    for(unsigned i=0u;i<0x1000u;i++)ram8w((uint16_t)(0xC000u+i),0u);
    for(unsigned i=0u;i<field_obj_count;i++){field_objs[i].active=0u;field_objs[i].type=0u;field_obj_art_stamp[i].epoch=0u;}
}
/* Retail scans beyond Inventory when all 40 slots are full. Keep the critical
 * reward inside the inventory: the player explicitly exchanges a sellable item.
 * No item is silently discarded and no story flag substitutes for the torch. */
static void r40_torch_reward(void){
    if(ps4_inventory_add(0x8Eu)>=0)return;
    REG_BLDCNT=0u;REG_BLDY=0u;
    static const char* const choice[2]={"EXCHANGE","KEEP ITEM"};
    for(;;){
        int slot=ps4_ui_select_inventory("EXCHANGE FOR ECLPS TORCH",0,0u);if(slot<0)continue;
        uint8_t item=ram8r((uint16_t)(PS4_INV_BASE+(unsigned)slot));
        if(!ps4_item_price(item)){ps4_ui_message("KEEP THIS KEY ITEM",0);continue;}
        if(ps4_ui_select_strings("EXCHANGE SELECTED ITEM?",choice,2u,1u)!=0)continue;
        ram8w((uint16_t)(PS4_INV_BASE+(unsigned)slot),0x8Eu);field_palette_fade_out();return;
    }
}
__attribute__((noinline))
static int r40_cutscene_lashiec(unsigned* camx,unsigned* camy){
    r40_panels_init();field_palette_fade_in();r34_panel(0x107u,0u);r40_dialogue3(0x38u,*camx,*camy);
    r34_vint_wait(0x77u);r40_panels_pop(3u);field_palette_fade_out();r40_panels_init();
    ram8w(0x500Au,0xADu);r34_vint_wait(0x27u);ram8w(0x500Au,0xADu);r34_vint_wait(0x13u);
    ram8w(0x500Au,0xADu);r34_panel(0x10Au,0u);field_palette_fade_in();r34_vint_wait(0xEFu);
    ram8w(0x500Au,0x99u);waitvb();r40_resume_dialogue(1u,*camx,*camy);field_palette_fade_out();
    r36_map_wait(0x3Bu);r40_torch_reward();r36_map_wait(0x3Bu);r40_panels_init();r40_clear_field_objects();
    ram8w(0xF400u,1u);r36_space_pass();r34_queue(0xB1u,0u,0x162u,0xFFFFu,0x4Eu,0x2Cu,4u,4u);return 1;
}
__attribute__((noinline))
static int r40_cutscene_dark_force2(unsigned* camx,unsigned* camy){
    if(field_event_party_slot_for_id(9u)<0)return 0;
    r40_panels_init();field_palette_fade_in();r34_panel(0x10Eu,0u);ram8w(0x500Au,0xADu);r34_vint_wait(0u);
    for(unsigned i=0u;i<4u;i++){ram8w(0x500Au,0xDDu);r34_vint_wait(7u);}ram8w(0x500Au,0xDDu);
    r34_panel(0x10Fu,0u);r40_dialogue3(0x3Bu,*camx,*camy);r34_panel(0x110u,0u);
    ram8w(0x500Au,0x9Du);r34_vint_wait(0x3Bu);r40_resume_dialogue(0u,*camx,*camy);
    r40_panels_pop(2u);r34_vint_wait(0x3Bu);r40_resume_dialogue(1u,*camx,*camy);r36_map_wait(0x27u);
    r40_panels_init();field_palette_fade_in();r34_panel(0x116u,0u);ram8w(0x500Au,0xADu);r34_vint_wait(0x77u);
    ram8w(0x500Au,0xA9u);r34_vint_wait(0u);r34_panel(0xA8u,0u);r40_resume_dialogue(1u,*camx,*camy);
    ps4_event_flag_set(0xA1u);r33_party_remove(9u);ram8w(0x500Au,0xFBu);r36_map_wait(0x13u);
    ram8w(0x500Au,0x9Du);ram8w(0xECECu,0x9Du);r34_queue(0xB1u,1u,1u,0xFFFFu,0x174u,0xEu,0u,0u);return 1;
}
__attribute__((noinline))
static int r40_cutscene_bishop(unsigned* camx,unsigned* camy){
    field_dialogue_show(0x33u,*camx,*camy);field_palette_fade_out();r36_map_wait(0x3Bu);
    ram8w(0x500Au,0x9Bu);r36_map_wait(0x3Bu);r40_panels_init();r40_clear_field_objects();
    ram8w(0xF400u,0u);r36_space_pass();r34_queue(0xB1u,2u,0xBFu,0xFFFFu,0x3Cu,0x26u,0u,0u);return 1;
}
__attribute__((noinline))
static int r40_story_resume(unsigned* camx,unsigned* camy){
    if(field_custom_resume_event!=0xB1u)return 0;
    if(field_custom_resume_phase==0u){
        ram8w(0x500Au,0x93u);ram8w(0xECECu,0x93u);field_palette_fade_in();
        r33_dialogue_tree(0x1EF130u);field_dialogue_show(0x39u,*camx,*camy);r33_dialogue_tree(0x1FA480u);
    }else if(field_custom_resume_phase==1u){
        /* RefreshMap installs source final party/world position and palette. */
    }else if(field_custom_resume_phase==2u){
        ram8w(0x500Au,0x89u);field_palette_fade_in();field_event_move_party_id(lead_char_id,0x1E0u,0x170u,camx,camy);
        r33_dialogue_tree(0x1FAAC0u);field_dialogue_show(0x17u,*camx,*camy);
        (void)ps4_inventory_remove_first(0x8Eu);(void)ps4_inventory_add(0x98u);
        field_reload_palette_from_map();hw_load_palettes();ps4_event_flag_set(0x9Du);ps4_event_flag_set(0xC0u);
    }else return 0;
    field_custom_resume_event=0xFFu;field_custom_resume_phase=0u;return 1;
}
