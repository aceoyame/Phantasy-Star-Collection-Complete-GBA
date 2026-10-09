/* Source $1A AeroPrism, $1B Rykros, $48 MeetingLeRoof and $1C LeRoofAgain.
 * Original art, component arithmetic, source clocks and progression order. */
static void r42_panel(unsigned id){pause_panel_action(0u,id);field_panel_source_palette(id);}
static void r42_dialogue(unsigned id,unsigned x,unsigned y){
    ram8w(0x500Au,0xF3u);ram8w(0xECFDu,1u);field_dialogue_saved_cursor=0;
    (void)field_dialogue_run_mode(field_dialogue_resolve(id),x,y,FIELD_DIALOGUE_KEEP_PANELS|FIELD_DIALOGUE_SOURCE_PALETTE);
}
static void r42_resume(unsigned finish,unsigned x,unsigned y){
    const uint8_t* p=field_dialogue_saved_cursor;field_dialogue_saved_cursor=0;
    ram8w(0x500Au,0xF3u);ram8w(0xECFDu,1u);
    (void)field_dialogue_run_mode(p,x,y,FIELD_DIALOGUE_KEEP_PANELS|FIELD_DIALOGUE_SOURCE_PALETTE);
    if(finish){field_palette_fade_out();field_cutscene_panels_clear();field_cutscene_panels=field_active_panel_count=0u;}
}
/* Pal_IncreaseTone/loc_58666 gate on Main_Frame_Count, not local loop phase.
 * AeroPrism's second rise restores the authored unaffected palette words. */
static void r42_tone(unsigned frames,unsigned restore,unsigned selective){
    for(unsigned f=0u;f<frames;f++){
        if(!(ram16r(0xEF1Cu)&3u)){if(restore)field_palette_restore_step();else field_palette_increase_tone_once();}
        if(selective)for(unsigned i=0u;i<64u;i++)if(i<19u||i>=32u)cram[i]=field_palette_saved[i];
        field_dialogue_commit_palette();waitvb();
    }
}
__attribute__((noinline))
static int r42_cutscene_aero_prism(unsigned* camx,unsigned* camy){
    r40_panels_init();field_palette_fade_in();r42_panel(0xA0u);field_palette_save(64u);
    r42_tone(30u,0u,0u);ram8w(0x500Au,0xE6u);r42_tone(30u,1u,0u);r42_dialogue(1u,*camx,*camy);
    r36_map_wait(0x1Du);ram8w(0x500Au,0xFDu);r36_map_wait(0u);r42_resume(0u,*camx,*camy);
    r42_panel(0xA4u);ram8w(0x500Au,0x9Fu);r34_vint_wait(0x3Bu);field_palette_save(64u);
    r42_tone(40u,0u,1u);r42_resume(0u,*camx,*camy);r42_panel(0xA5u);
    for(unsigned i=0u;i<3u;i++)cram[48u+i]=rom16(0x2B8F6Eu+i*2u);
    field_dialogue_commit_palette();
    for(unsigned i=0u;i<16u;i++){field_palette_saved[48u+i]=rom16(0x2B8F6Eu+i*2u);ram16w((uint16_t)(0xFBE0u+i*2u),field_palette_saved[48u+i]);}
    ram8w(0x500Au,0xECu);r34_vint_wait(0x13u);r42_tone(40u,1u,0u);r42_resume(0u,*camx,*camy);
    r33_party_position(0u,0x190u,0x100u,8u);r33_party_position(3u,0x1A0u,0x100u,12u);
    r33_party_position(5u,0x190u,0xF0u,0u);r33_party_position(7u,0x1A0u,0xF0u,0u);
    int slot=field_event_party_slot_for_id(10u);if(slot>=0)ram16w((uint16_t)(0xC000u+(unsigned)slot*64u),0u);
    r33_party_remove(10u);field_followers_sync(0);
    for(unsigned i=0u;i<5u;i++){uint8_t id=ram8r((uint16_t)(0xF40Au+i));if(id==0xFFu)break;field_event_sync_party_carrier(i,id,0u);}
    ps4_event_flag_set(0xC5u);field_event_set_mapload_bit(3u);
    field_cutscene_panels=field_active_panel_count=0u;return field_event_run_battle_request(0x12u,camx,camy);
}
static void r42_rykros_reveal(void){
    unsigned offset=0u;
    for(uint32_t p=0x782D2u;rom8(p)!=0xFFu;p++,offset+=18u){
        for(unsigned i=0u;i<9u;i++)cram[23u+i]=rom16(0x782DAu+offset+i*2u);
        field_dialogue_commit_palette();r34_vint_wait(rom8(p));
    }
}
__attribute__((noinline))
static int r42_cutscene_rykros(unsigned* camx,unsigned* camy){
    r40_panels_init();field_palette_fade_in();r42_panel(0x18Bu);r34_vint_wait(0x1Du);r42_panel(0x18Fu);
    r33_dialogue_tree(0x1FAAC0u);r42_dialogue(0x16u,*camx,*camy);r42_panel(0x190u);r34_vint_wait(0x13u);
    field_palette_save(64u);r42_tone(30u,0u,0u);ram8w(0x500Au,0xE6u);r42_tone(30u,1u,0u);
    r42_resume(0u,*camx,*camy);r34_vint_wait(0x3Bu);r40_panels_pop(1u);field_dialogue_commit_palette();
    r42_resume(0u,*camx,*camy);r42_rykros_reveal();r42_resume(1u,*camx,*camy);ps4_event_flag_set(0xD0u);return 1;
}
/* loc_5B26E increments source palette nibbles, with a final unchanged
 * iteration. The first32 words fade independently of the party/UI lines. */
static void r42_first32_from_saved(unsigned delay){
    for(;;){unsigned changes=0u;
        for(unsigned i=0u;i<32u;i++){uint16_t c=cram[i],t=field_palette_saved[i],out=0u;
            for(unsigned n=0u;n<4u;n++){unsigned a=(c>>(n*4u))&15u,b=(t>>(n*4u))&15u;if(a!=b){++a;changes=1u;}out=(uint16_t)(out|(uint16_t)(a<<(n*4u)));}cram[i]=out;
        }
        field_dialogue_commit_palette();field_palette_wait(delay);if(!changes)break;
    }
}
static void r42_leroof_cycle(void){
    if(ram16r(0xEF1Cu)&3u)return;unsigned q=ram8r(0xECF3u);if(q>=11u)q=0u;
    cram[11u]=rom16(0x70672u+q*2u);cram[12u]=rom16(0x70688u+q*2u);ram8w(0xECF3u,(uint8_t)(q+1u));
}
static void r42_leroof_wait(unsigned frames){for(unsigned f=0u;f<frames;f++){r42_leroof_cycle();field_dialogue_commit_palette();waitvb();}}
static void r42_leroof_reveal(void){
    unsigned n=0u;for(int remaining=70;remaining>=0;remaining--){
        for(unsigned i=0u;i<9u;i++)cram[2u+i]=0u;
        for(unsigned i=0u;i<=n;i++)cram[2u+i]=rom16(0x70626u+(8u-n+i)*2u);
        if(!(remaining&7))++n;r42_leroof_cycle();field_dialogue_commit_palette();waitvb();
    }
}
/* Source Nemesis/Enigma full-screen art uses existing virtual VRAM and
 * presentation planes/framebuffer. No generated asset or new EWRAM. */
static void r42_leroof_art(void){
    r36_bitmap_begin();presentation_hide_field_oam();r36_scene_planes_clear();
    (void)load_nem_vram_auto(0x1DB2C8u,0x260u);r36_scene_map(0u,0x1DDE0Au,0x260u,40u,28u,0u,0u,0u);
    for(unsigned i=0u;i<16u;i++){field_palette_saved[i]=rom16(0x1DE258u+i*2u);field_palette_saved[16u+i]=0u;ram16w((uint16_t)(0xFB80u+i*2u),field_palette_saved[i]);ram16w((uint16_t)(0xFBA0u+i*2u),0u);}
    r36_scene_compose(0u);field_cutscene_panels=1u;field_active_panel_count=0u;field_dialogue_commit_palette();panel_capture_base_scaled();panel_present_scaled();
    ram8w(0x500Au,0xA1u);ram8w(0xED52u,2u);r42_first32_from_saved(2u);r42_leroof_wait(180u);r42_leroof_reveal();r42_leroof_wait(120u);
}
__attribute__((noinline))
static int r42_event_meeting_leroof(unsigned* camx,unsigned* camy){
    field_event_move_party_id(lead_char_id,0x1F0u,0x1E0u,camx,camy);r36_map_wait(0x3Bu);
    ram8w(0xED52u,2u);field_palette_first32_black(2u);r36_map_wait(0x3Bu);r42_leroof_art();
    r42_dialogue(0u,*camx,*camy);r42_leroof_wait(120u);ram8w(0x500Au,0xFBu);ram8w(0xED52u,2u);field_palette_first32_black(2u);
    field_cutscene_panels=field_active_panel_count=0u;hw_field_init(*camx,*camy);
    for(unsigned i=0u;i<32u;i++){field_palette_saved[i]=rom16(0x18711Au+i*2u);ram16w((uint16_t)(0xFB80u+i*2u),field_palette_saved[i]);}
    ram8w(0xED52u,2u);r42_first32_from_saved(2u);ram8w(0x500Au,0x9Au);ps4_event_flag_set(0xD1u);return 1;
}
__attribute__((noinline))
static int r42_cutscene_leroof_again(unsigned* camx,unsigned* camy){
    field_event_move_party_id(lead_char_id,0x1F0u,0x1F0u,camx,camy);ram8w(0xED52u,2u);field_palette_first32_black(2u);r36_map_wait(0x3Bu);r42_leroof_art();
    r42_dialogue(0xBu,*camx,*camy);r40_panels_init();field_palette_fade_in();r42_panel(0x11Bu);r42_resume(0u,*camx,*camy);
    ram8w(0xED52u,5u);field_palette_all64_to_black(5u);field_active_panel_count=0u;r36_map_wait(0x3Bu);
    r40_panels_init();field_palette_fade_in();r42_panel(0x125u);r42_dialogue(0xCu,*camx,*camy);
    field_palette_fade_out();field_cutscene_panels_clear();field_cutscene_panels=field_active_panel_count=0u;
    ps4_event_flag_set(0xD6u);ps4_event_flag_set(0xD7u);return r36_spaceship_begin(0u,camx,camy);
}
