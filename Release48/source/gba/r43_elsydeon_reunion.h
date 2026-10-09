/* Source $1D cave entry, $1E Elsydeon and $1F reunion. B2 resumes
 * only the real reunion map reloads; source party storage remains ED54..58. */
static void r43_resume_text(unsigned cue,unsigned finish,unsigned x,unsigned y){
 const uint8_t*p=field_dialogue_saved_cursor;field_dialogue_saved_cursor=0;
 if(cue)ram8w(0x500Au,0xF3u);ram8w(0xECFDu,1u);
 (void)field_dialogue_run_mode(p,x,y,FIELD_DIALOGUE_KEEP_PANELS|FIELD_DIALOGUE_SOURCE_PALETTE);
 if(finish){field_palette_fade_out();field_cutscene_panels_clear();field_cutscene_panels=field_active_panel_count=0u;}
}
static void r43_popup(unsigned id,unsigned count){
 if(count>field_active_panel_count)count=field_active_panel_count;field_active_panel_count-=count;
 panel_rebuild_scaled();r42_panel(id);
}
static void r43_white(unsigned start,unsigned count,unsigned save){
 if(save)field_palette_save(64u);for(unsigned i=start;i<start+count;i++)cram[i]=0xEEEu;field_dialogue_commit_palette();
}
static void r43_sanctuary(unsigned face){
 ram8w(0xEC4Eu,(uint8_t)(ram8r(0xEC4Eu)&~8u));field_custom_resume_event=0xFFu;
 field_cutscene_reload(0x16Fu,0xFFFFu,0x3Cu,0x32u,(uint8_t)face,0u);
}
__attribute__((noinline))
static int r43_cutscene_before_cave(unsigned* x,unsigned* y){
 field_event_move_party_id(lead_char_id,0x1E0u,0x200u,x,y);field_event_move_party_id(lead_char_id,0x1D0u,0x1C0u,x,y);field_event_move_party_id(lead_char_id,0x1E0u,0x190u,x,y);
 r40_panels_init();ram8w(0x500Au,0xE2u);r36_map_wait(0x77u);r42_panel(0x18Eu);field_palette_fade_in();r42_dialogue(0x30u,*x,*y);field_palette_fade_out();field_cutscene_panels_clear();field_cutscene_panels=0u;
 for(unsigned i=0u;i<5u;i++)ram8w((uint16_t)(0xED54u+i),ram8r((uint16_t)(0xF40Au+i)));
 ram8w(0xF40Au,0u);for(unsigned i=1u;i<5u;i++)ram8w((uint16_t)(0xF40Au+i),0xFFu);
 field_recover_party();ps4_event_flag_set(0xD8u);r43_sanctuary(4u);return 1;
}
__attribute__((noinline))
static int r43_cutscene_elsydeon(unsigned* x,unsigned* y){
 r40_panels_init();field_palette_fade_in();r42_panel(0x129u);r34_vint_wait(0x9Fu);r42_dialogue(0x31u,*x,*y);r34_vint_wait(0x77u);
 ram8w(0x500Au,0xDCu);r42_panel(0x12Bu);r34_vint_wait(0x1Du);r42_panel(0x12Cu);r34_vint_wait(0x77u);ram8w(0x500Au,0xA1u);r42_tone(40u,0u,0u);
 r43_popup(0x12Du,4u);for(unsigned i=48u;i<64u;i++){field_palette_saved[i]=cram[i];ram16w((uint16_t)(0xFB80u+i*2u),cram[i]);}r43_white(48u,16u,0u);
 r34_vint_wait(0x3Bu);r42_tone(40u,1u,0u);r34_vint_wait(0x3Bu);r42_panel(0x12Eu);r34_vint_wait(0x13u);r43_resume_text(1u,0u,*x,*y);
 r34_vint_wait(0x1Du);r42_panel(0x12Fu);r34_vint_wait(0xEFu);r43_white(0u,64u,1u);r40_panels_pop(2u);field_dialogue_commit_palette();r42_tone(40u,1u,0u);r34_vint_wait(0x3Bu);
 r42_panel(0x130u);r34_vint_wait(0x13u);r43_resume_text(0u,0u,*x,*y);r34_vint_wait(0x1Du);r42_panel(0x131u);r34_vint_wait(0x31u);r42_panel(0x132u);r34_vint_wait(0xEFu);
 r43_white(0u,64u,1u);r40_panels_pop(3u);field_dialogue_commit_palette();r42_tone(40u,1u,0u);r34_vint_wait(0x3Bu);
 r42_panel(0x18Cu);r34_vint_wait(0x9Fu);r42_panel(0x133u);r34_vint_wait(0x9Fu);r42_panel(0x134u);r34_vint_wait(0x9Fu);r42_panel(0x135u);r34_vint_wait(0x9Fu);r42_tone(40u,0u,0u);
 r43_popup(0x136u,5u);field_palette_save(32u);r43_white(0u,32u,0u);r34_vint_wait(0x77u);r42_tone(40u,1u,0u);r43_resume_text(0u,0u,*x,*y);
 r36_map_wait(0x3Bu);r42_tone(40u,0u,0u);field_cutscene_panels_clear();r40_panels_init();r36_map_wait(0x3Bu);field_palette_fade_in();r42_panel(0x139u);r34_vint_wait(0x77u);r42_panel(0x13Au);r34_vint_wait(0x9Fu);r42_panel(0x13Bu);r42_dialogue(0x32u,*x,*y);field_palette_fade_out();field_cutscene_panels_clear();field_cutscene_panels=0u;
 ps4_event_flag_set(0xD9u);ram8w(0xF54Cu,0x77u);ram8w(0xF54Du,0u);update_one_char_derived(0u);
 for(unsigned i=0u;i<5u;i++)ram8w((uint16_t)(0xF40Au+i),ram8r((uint16_t)(0xED54u+i)));
 r43_sanctuary(0u);return 1;
}
static void r43_npc_target(unsigned i,unsigned x,unsigned y){if(i<field_obj_count&&field_objs[i].active){r43_npc_init(i);ram16w((uint16_t)(0xC338u+i*64u),(uint16_t)x);ram16w((uint16_t)(0xC33Au+i*64u),(uint16_t)y);}}
static void r43_npc_static(unsigned i,unsigned tx,unsigned ty,unsigned x,unsigned y){
 r43_npc_target(i,tx,ty);if(i>=field_obj_count||!field_objs[i].active)return;
 do{field_update_npcs(x,y);field_run_map_updates();waitvb();}while(field_objs[i].x!=tx||field_objs[i].y!=ty);
 ram32w((uint16_t)(0xC320u+i*64u),0u);ram32w((uint16_t)(0xC324u+i*64u),0u);
}
static void r43_reunion_equipment(void){
 static const uint8_t ids[5]={1u,4u,6u,8u,9u};
 static const uint8_t eq[5][4]={{0x71u,0x88u,0xA0u,0x7Cu},{0x78u,0u,0x68u,0x67u},{0x79u,0u,0x70u,0x6Fu},{0x62u,0u,0x6Au,0x59u},{0x5Du,0x65u,0x69u,0x59u}};
 for(unsigned i=0u;i<5u;i++){uint16_t b=(uint16_t)(0xF54Cu+(unsigned)ids[i]*128u);for(unsigned q=0u;q<4u;q++)ram8w((uint16_t)(b+q),eq[i][q]);update_one_char_derived(ids[i]);}
}
__attribute__((noinline))
static int r43_cutscene_reunion(unsigned* x,unsigned* y){
 field_event_move_party_id(lead_char_id,0x1E0u,0x1C0u,x,y);r43_npc_target(0u,0x1E0u,0x1B0u);r43_npc_static(1u,0x1F0u,0x1B0u,*x,*y);r36_map_wait(0x27u);
 r40_panels_init();field_palette_fade_in();r42_panel(0x13Du);r42_dialogue(0u,*x,*y);field_palette_fade_out();field_cutscene_panels_clear();field_cutscene_panels=0u;r36_map_wait(0x3Bu);ram8w(0x500Au,0x9Bu);r36_map_wait(0x3Bu);r40_panels_init();
 for(unsigned i=0u;i<0x800u;i++)ram8w((uint16_t)(0xC000u+i),0u);for(unsigned i=0u;i<field_obj_count&&i<20u;i++)field_objs[i].active=0u;
 ram8w(0xF400u,0u);r36_space_pass();r34_queue(0xB2u,1u,0xBFu,0xFFFFu,0x3Cu,0x24u,0u,0u);return 1;
}
__attribute__((noinline))
static int r43_story_resume(unsigned* x,unsigned* y){
 if(field_custom_resume_event!=0xB2u)return 0;
 if(field_custom_resume_phase==1u){
  ram8w(0x500Au,0x89u);ram8w(0xECECu,0x89u);field_palette_fade_in();
  static const uint16_t tx[5]={0x1F0u,0x1F0u,0x1E0u,0x1D0u,0x1F0u},ty[5]={0x190u,0x180u,0x1C0u,0x1B0u,0x1B0u};for(unsigned i=0u;i<5u;i++)r43_npc_target(i,tx[i],ty[i]);
  field_event_move_party_id(lead_char_id,0x1E0u,0x190u,x,y);r36_map_wait(0x27u);r40_panels_init();field_palette_fade_in();r42_panel(0x13Fu);r34_vint_wait(0x3Bu);r42_panel(0x13Eu);r42_dialogue(1u,*x,*y);field_palette_fade_out();field_cutscene_panels_clear();field_cutscene_panels=0u;
  ps4_event_flag_set(0xDAu);r34_queue(0xB2u,2u,0xBFu,0xFFFFu,0x3Cu,0x36u,0u,0u);return 1;
 }
 if(field_custom_resume_phase==2u){r43_reunion_equipment();field_custom_resume_event=0xFFu;return 1;}return 0;
}
