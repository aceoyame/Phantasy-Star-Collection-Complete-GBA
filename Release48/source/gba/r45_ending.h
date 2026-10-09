/* US Cutscene_Ending ($78F40), credits ($79D0C) and Fin ($7A0FA).
 * Original ROM art/text, source RAM counters, one VBlank per scroll update.
 * B3 continues only the real Termi map reload; no new persistent RAM. */
#include "r45_ending_script.h"
static void r45_zero(unsigned address,unsigned bytes){for(unsigned i=0u;i<bytes;i++)ram8w((uint16_t)(address+i),0u);}
static void r45_ending_publish(unsigned columns){
 r45_scene_compose(ram16r(0xEF94u),ram16r(0xEF90u),ram16r(0xEF9Cu),ram16r(0xEF98u),columns,0u);r45_scene_present();
}
static void r45_ending_init(void){
 r36_bitmap_begin();field_palette_fade_out();presentation_hide_field_oam();
 /* trap #0 writes longs: $7FF clears BOTH 4-KiB source planes. */
 r45_zero(0x8000u,8192u);for(unsigned t=0u;t<16u;t++)for(unsigned i=0u;i<32u;i++)md_vram[t*32u+i]=(uint8_t)(t*17u);
 field_active_panel_count=0u;field_cutscene_panels=1u;panel_clear_scaled();
 waitvb();dma_copy32(field_pause_fb,(void*)MODE4_VRAM16,38400u);dma_copy32(field_pause_fb,(void*)MODE4_PAGE1,38400u);
 REG_DISPCNT=(uint16_t)(MODE4|MODE4_FRAME1|BG2_ENABLE|OBJ_1D|OBJ_ENABLE);
 for(unsigned a=0xEF90u;a<0xEFA0u;a+=4u)ram32w((uint16_t)a,0u);
 for(unsigned i=0u;i<64u;i++)cram[i]=rom16(0x9F2BCu+i*2u);field_dialogue_commit_palette();
}
static void r45_ending_narrative(unsigned x,unsigned y){
 for(unsigned k=0u;k<sizeof r45_ending_ops/sizeof r45_ending_ops[0];k++){
  const struct R45EndingOp*op=&r45_ending_ops[k];unsigned v=op->value;
  switch(op->kind){
   case R45_END_INIT:r45_ending_init();break;
   case R45_END_FADE_IN:field_palette_fade_in();break;
   case R45_END_PANEL:r42_panel(v);break;
   case R45_END_POP:if(field_active_panel_count)field_active_panel_count--;break;
   case R45_END_PUBLISH:panel_rebuild_scaled();panel_present_scaled();break;
   case R45_END_WAIT:for(unsigned f=0u;f<v;f++)waitvb();break;
   case R45_END_TEXT:ram8w(0xECFDu,1u);field_dialogue_saved_cursor=0;(void)field_dialogue_run_mode(field_dialogue_resolve(v),x,y,FIELD_DIALOGUE_KEEP_PANELS|FIELD_DIALOGUE_SOURCE_PALETTE);break;
   case R45_END_RESUME:r43_resume_text(0u,0u,x,y);break;
   case R45_END_TONE:r42_tone(v,0u,0u);break;
   case R45_END_RESTORE:r42_tone(v,1u,0u);break;
   case R45_END_SAVE:field_palette_save(64u);break;
   case R45_END_COPY:field_palette_copy_saved_to_live();break;
   case R45_END_SAVED_PD_PAL:for(unsigned i=0u;i<16u;i++){field_palette_saved[i]=rom16(0x2EEA04u+i*2u);ram16w((uint16_t)(0xFB80u+i*2u),field_palette_saved[i]);}break;
   case R45_END_ZERO_LINE3:for(unsigned i=32u;i<47u;i++)cram[i]=0u;field_dialogue_commit_palette();break;
   case R45_END_DOWN:ram8w(0xED52u,(uint8_t)v);field_palette_all64_to_black(v);break;
   case R45_END_UP:ram8w(0xED52u,(uint8_t)v);field_palette_first32_from_black_to_current(v);break;
   case R45_END_SOUND:ram8w(0x500Au,(uint8_t)v);break;
   case R45_END_CLEAR:field_active_panel_count=0u;break;
  }
 }
}
/* FF-delimited original credit entries. RunText2 uses the original 8x16
 * bitmap font and two vertically consecutive 8x8 tiles per character. */
static uint32_t r45_credit_entry(uint32_t address,unsigned id){
 while(id&&address<0x300000u){if(rom8(address++)==0xFFu)--id;}return address;
}
static void r45_credit_text(const uint8_t* text,uint32_t rom_address,unsigned plane,unsigned tile){
 unsigned base=(tile&0x7FFu)*32u;r45_zero(0u,2048u);for(unsigned i=0u;i<2048u;i++)md_vram[(base+i)&0xFFFFu]=0u;
 unsigned bank=plane&0xF000u,row=plane&0xF80u,col=plane&0x7Fu;
 for(unsigned c=0u;c<32u;c++){
  unsigned cell=bank|row|((col+c*2u)&0x7Fu);ram16w((uint16_t)cell,(uint16_t)(tile+c*2u));ram16w((uint16_t)(bank|((row+128u)&0xF80u)|((col+c*2u)&0x7Fu)),(uint16_t)(tile+c*2u+1u));
  ram16w((uint16_t)(bank|((row-128u)&0xF80u)|((col+c*2u)&0x7Fu)),0u);
 }
 unsigned c=0u;while(c<32u){unsigned q=text?*text++:rom8(rom_address++);if(q>=0xF0u)break;if(q>=0xE0u)q=text?*text++:rom8(rom_address++);
  for(unsigned y=0u;y<16u;y++){unsigned bits=rom8(0x2A3542u+(q&127u)*16u+y);for(unsigned x=0u;x<4u;x++){unsigned shift=6u-x*2u,v=((bits>>shift)&2u?0xF0u:0u)|((bits>>shift)&1u?15u:0u);unsigned off=c*64u+y*4u+x;ram8w((uint16_t)off,(uint8_t)v);md_vram[(base+off)&0xFFFFu]=(uint8_t)v;}}
  ++c;
 }
 ram8w(0xED5Au,(uint8_t)c);
}
static void r45_credit_intro(void){
 r42_tone(60u,0u,0u);r45_zero(0xFB80u,256u);for(unsigned i=0u;i<64u;i++)field_palette_saved[i]=0u;
 for(unsigned a=0xEF90u;a<0xEFA0u;a+=4u)ram32w((uint16_t)a,0u);r45_zero(0x8000u,8192u);r45_ending_publish(0u);
 static const uint16_t address[4]={0x8406u,0x8586u,0x8706u,0x8886u};
 for(unsigned i=0u;i<4u;i++){r45_credit_text(field_dialogue_resolve(0x12u+i),0u,address[i],0xC100u+i*0x80u);r45_ending_publish(0u);}
 for(;;){if(!(ram16r(0xEF1Cu)&7u)){uint16_t q=(uint16_t)(cram[47]-0x222u);if(!q)break;cram[47]=q;}field_dialogue_commit_palette();waitvb();}
 r34_vint_wait(179u);ram8w(0x500Au,0xAEu);r34_vint_wait(179u);
 for(;;){if(!(ram16r(0xEF1Cu)&7u)){uint16_t q=(uint16_t)(cram[47]+0x222u);if(q&0x1000u)break;cram[47]=q;}field_dialogue_commit_palette();waitvb();}
 r45_zero(0x8000u,8192u);r45_ending_publish(0u);r42_tone(60u,1u,0u);
}
static void r45_eni_to_ram(uint32_t address,unsigned base,unsigned dest){
 unsigned n=enigma_decode(address,(uint16_t)base,panel_map_scratch,2048u);for(unsigned i=0u;i<n;i++)ram16w((uint16_t)(dest+i*2u),panel_map_scratch[i]);
}
static void r45_credit_flags(unsigned set,unsigned clear){ram8w(0xED95u,(uint8_t)((ram8r(0xED95u)|set)&~clear));}
static void r45_credit_header(unsigned cmd,unsigned id){
 unsigned x=0x11u,y=5u;r45_zero(cmd==11u?0x8B34u:cmd==10u?0x86A2u:0x82A2u,cmd==11u?40u:96u);
 if(cmd==9u){r45_zero(0x86A2u,96u);y=13u;}if(cmd==10u){r45_zero(0x8A26u,80u);x=19u;y=20u;}if(cmd==11u){x=26u;y=22u;}
 uint32_t p=r45_credit_entry(0x1FEE26u,id);for(unsigned guard=0u;guard<64u;guard++){unsigned c=rom8(p++);if(c>=0xF0u)break;if(c&128u)c+=0xC0u;ram16w((uint16_t)(0x8000u+((y&31u)*64u+(x++&63u))*2u),(uint16_t)(0xE680u+c));}
 ram16w(0xED96u,(uint16_t)(ram16r(0xED96u)+1u));r45_credit_flags(1u,0u);
}
static void r45_credit_commands(uint32_t table){
 unsigned phase=ram16r(0xED90u)&63u,cmd=0xFFu;for(unsigned i=0u;i<32u;i++){unsigned q=rom8(table+i*2u);if(q==255u)break;if(q==phase){cmd=rom8(table+i*2u+1u);break;}}
 if(cmd==0u||cmd==9u||cmd==10u||cmd==11u)r45_credit_header(cmd,ram16r(0xED96u)>>3);
 else if(cmd>=1u&&cmd<=4u){unsigned id=(ram16r(0xED98u)>>3)*4u+cmd-1u;r45_credit_text(0,r45_credit_entry(0x1FEB42u,id),0x84A8u+(cmd-1u)*0x200u,0x4300u+(cmd-1u)*0x80u);if(cmd==4u)ram16w(0xED98u,(uint16_t)(ram16r(0xED98u)+1u));r45_credit_flags(1u,0u);}
 else if(cmd>=5u&&cmd<=8u){r45_credit_flags(1u<<(cmd-3u),0u);if(cmd==5u||cmd==7u)ram8w(0xECF2u,0u);}
 else if(cmd==12u)r45_credit_flags(64u,0u);
}
static void r45_credit_header_color(unsigned reverse){
 unsigned phase=ram8r(0xECF2u),p=0x7A6DEu+(reverse?7u-phase:phase)*10u;for(unsigned i=0u;i<5u;i++)cram[50u+i]=rom16(p+i*2u);ram8w(0xECF2u,(uint8_t)((phase+1u)&7u));
}
static void r45_credit_fades(void){
 unsigned flags=ram8r(0xED95u),clock=ram16r(0xEF1Cu);
 if((flags&4u)&&!(clock&3u)){r45_credit_header_color(0u);if(!ram8r(0xECF2u))r45_credit_flags(0u,4u);}
 if((flags&8u)&&!(clock&7u)){uint16_t q=(uint16_t)(cram[47]+0x222u);if(q&0x1000u)r45_credit_flags(0u,8u);else cram[47]=q;}
 if((flags&16u)&&!(clock&1u)){r45_credit_header_color(1u);if(!ram8r(0xECF2u)){r45_credit_flags(1u,16u);r45_zero(0x82A2u,96u);}}
 if((flags&32u)&&!(clock&1u)){if(cram[47])cram[47]=(uint16_t)(cram[47]-0x222u);else{r45_credit_flags(1u,32u);for(unsigned i=0u;i<4u;i++){r45_zero(0x84A8u+i*0x200u,84u);r45_zero(0x8528u+i*0x200u,84u);}}}
 if((flags&64u)&&!(clock&3u)){r45_credit_header_color(0u);if(!ram8r(0xECF2u))r45_credit_flags(128u,4u);}
 field_dialogue_commit_palette();
}
static void r45_credit_init(void){
 r45_ending_init();for(unsigned a=0xED90u;a<=0xED9Au;a+=2u)ram16w((uint16_t)a,0u);ram32w(0xEF98u,0xFF200000u);
 (void)load_kos_vram_auto(0x1DE278u,0x100u);(void)load_kos_vram_auto(0x1DEA48u,0x200u);(void)load_nem_vram_auto(0x2A3350u,0x7C0u);
 for(unsigned i=0u;i<32u;i++)cram[i]=rom16(0x1DF53Au+i*2u);for(unsigned i=0u;i<16u;i++)cram[48u+i]=rom16(0x1DF57Au+i*2u);
 static const uint32_t art[6]={0x1DF09Cu,0x1DF17Cu,0x1DF294u,0x1DF3E0u,0x1DF3E0u,0x1DF4C2u};static const uint16_t dst[6]={0x1000u,0x1380u,0x1700u,0x1A80u,0x1E00u,0x2000u};
 for(unsigned i=0u;i<6u;i++)r45_eni_to_ram(art[i],i==5u?0x2200u:0x100u,dst[i]);
 for(unsigned y=0u;y<28u;y++)for(unsigned x=0u;x<16u;x++)ram16w((uint16_t)(0x9000u+y*128u+x*2u),ram16r((uint16_t)(0x1A80u+y*32u+x*2u)));
 r45_ending_publish(0u);field_dialogue_commit_palette();field_palette_fade_in();for(unsigned i=48u;i<64u;i++)cram[i]=0u;cram[47]=0u;ram16w(0xECF2u,0u);
}
static void r45_credit_tick(unsigned stage){
 uint32_t old=ram32r(stage==1u?0xEF98u:0xEF90u);ram32w(0xEF98u,ram32r(0xEF98u)+(stage==1u?0x2000u:0x1000u));
 if(stage!=1u){ram32w(0xEF90u,old+0x2000u);for(unsigned i=0u;i<20u;i++){ram16w((uint16_t)(0xA000u+i*4u),(uint16_t)(i<8u?ram16r(0xEF90u):0u));ram16w((uint16_t)(0xA002u+i*4u),ram16r(0xEF98u));}}
 uint32_t diff=old^ram32r(stage==1u?0xEF98u:0xEF90u);if(diff&0x10000u)ram16w(0xED90u,(uint16_t)(ram16r(0xED90u)+1u));
 if(diff&0x80000u){unsigned row=ram16r(0xED92u);if(stage==1u)row++;unsigned source=(stage==1u?0x1000u:0x2000u)+row*32u,dest=(stage==1u?0x9000u:0x8000u)+((row*128u)&0xFFFu);
  for(unsigned i=0u;i<16u;i++)ram16w((uint16_t)(dest+i*2u),ram16r((uint16_t)(source+i*2u)));ram16w(0xED92u,(uint16_t)(ram16r(0xED92u)+1u));r45_credit_flags(stage==1u?2u:1u,0u);
 }
 if(ram8r(0xED95u)&1u)r45_credit_flags(0u,1u);else if(ram8r(0xED95u)&2u)r45_credit_flags(0u,2u);
 r45_ending_publish(stage==1u?0u:2u);if(stage!=3u||!(ram8r(0xED95u)&128u)){r45_credit_commands(stage==3u?0x7A548u:0x7A522u);r45_credit_fades();}
}
static void r45_credits(void){
 r45_credit_init();while(ram16r(0xED90u)!=896u)r45_credit_tick(1u);ram16w(0xED92u,0u);
 while(ram16r(0xED90u)!=1024u)r45_credit_tick(2u);while(ram16r(0xED90u)!=1152u)r45_credit_tick(3u);
 for(unsigned step=0u;step<14u;){if(!(ram16r(0xEF1Cu)&7u)){for(unsigned i=0u;i<8u;i++)cram[18u+i]=rom16(0x7A274u+(14u-step)*2u+i*2u);if(step>=9u)for(unsigned i=0u;i<2u;i++)cram[30u+i]=rom16(0x7A2A0u+(step-9u)*4u+i*2u);++step;}field_dialogue_commit_palette();waitvb();}
 r42_tone(60u,0u,0u);
}
static unsigned r45_fin_cell(unsigned x,unsigned y){return 0x8000u+((((ram16r(0xEF90u)>>3)+y)&31u)*64u+(((ram16r(0xEF94u)>>3)+x)&63u))*2u;}
static int r45_ending_resume(unsigned* x,unsigned* y){
 (void)x;(void)y;field_custom_resume_event=0xFFu;r36_bitmap_begin();presentation_hide_field_oam();field_active_panel_count=0u;field_cutscene_panels=1u;
 for(unsigned j=0u;j<32u;j++)for(unsigned i=0u;i<64u;i++){ram16w((uint16_t)(0x8000u+(j*64u+i)*2u),field_entry(0xA000u,i*8u,j*8u));ram16w((uint16_t)(0x9000u+(j*64u+i)*2u),field_entry(0xB000u,i*8u,j*8u));}
 r45_zero(0xC000u,320u);r45_zero(0xC300u,448u);r45_zero(0xC540u,64u);field_follower_count=0u;
 for(unsigned i=0u;i<field_obj_count;i++)if(i<7u||i==9u){field_objs[i].active=0u;field_obj_art_stamp[i].epoch=0u;}
 for(unsigned j=0u;j<28u;j++)for(unsigned i=0u;i<24u;i++){unsigned a=r45_fin_cell(i,j);ram16w((uint16_t)a,0x8002u);ram16w((uint16_t)(a+0x1000u),0x8002u);}
 field_palette_save(64u);for(unsigned i=0u;i<64u;i++)cram[i]=0xEEEu;(void)load_nem_vram_auto(0x1DEEB8u,0x580u);r45_eni_to_ram(0x1DF52Cu,0xC580u,0u);
 for(unsigned j=0u;j<4u;j++)for(unsigned i=0u;i<12u;i++)ram16w((uint16_t)r45_fin_cell(6u+i,11u+j),ram16r((uint16_t)((j*12u+i)*2u)));
 for(unsigned i=0u;i<16u;i++){field_palette_saved[32u+i]=rom16(0x1DF59Au+i*2u);ram16w((uint16_t)(0xFBC0u+i*2u),field_palette_saved[32u+i]);}
 REG_BLDCNT=0u;REG_BLDY=0u;r45_ending_publish(0u);r42_tone(40u,1u,0u);
 /* Native waitvb updates clocks/audio, not the MD joypad mirror. Poll the
  * canonical mapped input here and preserve the source pressed-edge test. */
 uint8_t prev=md_io_read_psiv_pad();for(;;){for(unsigned i=0u;i<field_obj_count;i++)if(field_objs[i].active)r31_actor_update_one(i);waitvb();uint8_t now=md_io_read_psiv_pad(),pressed=(uint8_t)(now&(uint8_t)~prev);prev=now;ram8w(0xEF02u,now);ram8w(0xEF03u,pressed);if(pressed&0x80u)break;}
 md_write8(0xA130F1u,1u);md_write8(0x200035u,1u);md_write8(0xA130F1u,0u);ram8w(0xED52u,3u);field_palette_all64_to_black(3u);ps4_return_to_title=1u;return 1;
}
__attribute__((noinline))
static int r45_cutscene_ending(unsigned* x,unsigned* y){
 r33_dialogue_tree(0x1FC920u);if(!(md_io_read_psiv_pad()&0x40u)){r45_ending_narrative(*x,*y);r45_credit_intro();}
 r45_credits();ram8w(0xEC4Eu,(uint8_t)(ram8r(0xEC4Eu)&~8u));r34_queue(0xB3u,0u,0x7Bu,0xFFFFu,0x36u,0x28u,0u,0u);return 1;
}
