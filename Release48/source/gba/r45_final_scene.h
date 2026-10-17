/* Final scene uses the original plane buffers ($8000/$9000), not the native
 * field chunk layouts. Both planes and party use one 320x224 projection. */
/* Snapshot source party state and column offsets once per frame.  These data
 * are stable while the synchronous Mode 4 composition runs; the old code
 * reread the same MD mirror registers up to 192,000 times per frame. */
static void r45_scene_compose(unsigned fgx,unsigned fgy,unsigned bgx,unsigned bgy,unsigned columns,unsigned party){
 int16_t col_offset[20]={0};
 uint8_t party_valid[5]={0};
 unsigned party_art[5]={0};
 int party_left[5]={0},party_top[5]={0};
 if(columns){
  unsigned base=columns==2u?0xA000u:0u;
  for(unsigned i=0u;i<20u;i++)col_offset[i]=(int16_t)ram16r((uint16_t)(base+i*4u));
 }
 if(party)for(unsigned i=0u;i<5u;i++){
  unsigned id=ram8r((uint16_t)(0xF40Au+i)),base=0xC000u+i*64u;
  if(id>10u||(ram8r((uint16_t)(base+2u))&2u))continue;
  party_valid[i]=1u;
  party_left[i]=(int)ram16r((uint16_t)(base+0x30u))-16;
  party_top[i]=(int)ram16r((uint16_t)(base+0x34u))-2;
  party_art[i]=char_field_art_address(id);
 }
 for(unsigned y=0u;y<160u;y++){
  unsigned sy=src_y[y];
  uint8_t party_row[5]={0};
  unsigned party_py[5]={0};
  if(party)for(unsigned i=0u;i<5u;i++)if(party_valid[i]){
   int py=(int)sy-party_top[i];
   if(py>=0&&py<32){party_row[i]=1u;party_py[i]=(unsigned)py;}
  }
  for(unsigned x=0u;x<240u;x++){
   unsigned sx=src_x[x],ax=(sx+fgx)&511u,bx=(sx+bgx)&511u;
   unsigned ay=(sy+(columns?col_offset[sx>>4]:fgy))&255u,by=(sy+bgy)&255u;
   uint16_t a=ram16r((uint16_t)(0x8000u+((ay>>3)*64u+(ax>>3))*2u)),b=ram16r((uint16_t)(0x9000u+((by>>3)*64u+(bx>>3))*2u));
   unsigned ca=tile_px(a,ax&7u,ay&7u),cb=tile_px(b,bx&7u,by&7u);
   unsigned q=ca&&((a&0x8000u)||!cb||!(b&0x8000u))?((a>>13)&3u)*16u+ca:cb?((b>>13)&3u)*16u+cb:ca?((a>>13)&3u)*16u+ca:0u;
   if(party)for(int i=4;i>=0;i--)if(party_row[i]){
    int px=(int)sx-party_left[i];
    if(px>=0&&px<16){
     unsigned ci=sprite_art_px_rom(party_art[i],0x30u,(unsigned)px,party_py[i],0);
     if(ci)q=32u+ci;
    }
   }
   field_pause_fb[y*240u+x]=(uint8_t)q;
  }
 }
}
static void r45_scene_present(void){
 r36_scene_palette();waitvb();REG_DISPCNT=0x1444u;dma_copy32(field_pause_pal,(void*)BG_PALETTE,512u);dma_copy32(field_pause_fb,(void*)MODE4_VRAM16,38400u);
}
static void r45_pd_column_update(void){
 unsigned angle=(uint8_t)(ram8r(0xECFFu)+1u);ram8w(0xECFFu,(uint8_t)angle);
 for(unsigned i=0u;i<20u;i++){int q=field_source_sine8((uint8_t)angle);q>>=5;ram16w((uint16_t)(i*4u),(uint16_t)q);ram16w((uint16_t)(i*4u+2u),ram16r(0xEF98u));angle=(angle+0x2Cu)&255u;}
 ram16w(0xEF90u,ram16r(0u));
}
static void r45_pd_reveal(void){
 unsigned countdown=16u;
 for(unsigned f=0u;f<600u;f++){
  field_source_palette_update30();r45_pd_column_update();--countdown;
  if(countdown<8u){cram[0]=(uint16_t)(cram[0]+0x222u);for(unsigned i=16u;i<32u;i++)cram[i]=(uint16_t)((cram[i]+0x222u)&0xEEEu);
   if(countdown==7u)ram8w(0x500Au,0xCEu);if(!countdown)countdown=(ps4_rng2_next()&0x7Fu)+9u;
  }
  r45_scene_compose(0u,0u,ram16r(0xEF9Cu),ram16r(0xEF98u),1u,1u);r45_scene_present();
 }
}
__attribute__((noinline))
static int r45_cutscene_pd(unsigned* x,unsigned* y){
 ram8w(0x500Au,0xDEu);ram8w(0xED52u,1u);field_palette_all64_to_black(1u);
 r36_bitmap_begin();presentation_hide_field_oam();
 /* Snapshot the source field BG before loading the arena's foreground art. */
 unsigned bx=ram16r(0xEF9Cu)&~511u,by=ram16r(0xEF98u)&~255u;
 for(unsigned j=0u;j<32u;j++)for(unsigned i=0u;i<64u;i++)ram16w((uint16_t)(0x9000u+(j*64u+i)*2u),field_entry(0xB000u,bx+i*8u,by+j*8u));
 for(unsigned i=0u;i<0x1000u;i++)ram8w((uint16_t)(0x8000u+i),0u);
 ram32w(0xEF90u,0u);ram32w(0xEF94u,0u);(void)load_nem_vram_auto(0x1DD566u,0x20Eu);
 unsigned n=enigma_decode(0x1DE03Au,0x620Eu,panel_map_scratch,2048u);
 for(unsigned j=0u;j<16u;j++)for(unsigned i=0u;i<16u;i++){unsigned k=j*16u+i;if(k<n)ram16w((uint16_t)(0x8198u+j*128u+i*2u),panel_map_scratch[k]);}
 static const uint8_t xy[5][2]={{0xA0u,0xA0u},{0x90u,0xB0u},{0xB0u,0xB0u},{0x80u,0xC0u},{0xC0u,0xC0u}};
 for(unsigned i=0u;i<5u;i++){uint16_t b=(uint16_t)(0xC000u+i*64u);ram32w((uint16_t)(b+0x30u),(unsigned)xy[i][0]<<16);ram32w((uint16_t)(b+0x34u),(unsigned)xy[i][1]<<16);ram16w((uint16_t)(b+0x38u),xy[i][0]);ram16w((uint16_t)(b+0x3Au),xy[i][1]);ram16w((uint16_t)(b+6u),4u);}
 chaz.x=0xA0;chaz.y=0xA0;chaz.facing=1u;chaz.anim=0u;field_followers_sync(0);
 r45_scene_compose(0u,0u,ram16r(0xEF9Cu),ram16r(0xEF98u),0u,1u);r45_scene_present();field_palette_copy_saved_to_live();
 ram8w(0x500Au,0xDEu);ram8w(0xED52u,7u);field_palette_first32_from_black_to_current(7u);ram8w(0x500Au,0xFEu);r45_pd_reveal();
 ram32w(0xEF90u,0u);ram32w(0xEF94u,0u);ram32w(0xEF98u,0u);ram32w(0xEF9Cu,0u);
 ram8w(0x500Au,0xA8u);waitvb();field_cutscene_panels=1u;ram8w(0x500Au,0xF3u);ram8w(0xECFDu,0u);
 (void)field_dialogue_show2(3u,0u,0u);ps4_event_flag_set(0xE8u);ram8w(0xECECu,0xFEu);field_event_set_mapload_bit(7u);field_event_set_mapload_bit(3u);
 field_cutscene_panels=0u;return field_event_run_battle_request(0x1Au,x,y);
}
