/* Original RunMapUpdates $05/$07/$0D/$0F/$10/$22. No new RAM arena. */
static unsigned r32_rate_shift(unsigned id){
 switch(id){case 5u:case 7u:return 4u;case 13u:return 2u;case 15u:return 3u;case 16u:return 1u;default:return 0u;}
}
static void r32_map_rate(unsigned id){
 unsigned sx=r32_rate_shift(id);if(!sx)return;
 unsigned sy=id==7u?2u:sx;
 int32_t x=(int32_t)ram32r(0xEC58u),y=(int32_t)ram32r(0xEC5Cu);
 ram32w(0xEC58u,(uint32_t)x-(uint32_t)(x>>sx));
 ram32w(0xEC5Cu,(uint32_t)y-(uint32_t)(y>>sy));
}
static void hw_publish_canonical_tiles(unsigned source_tile,unsigned tiles);
__attribute__((noinline))
static void r32_garuberk_update(void){
 if(ps4_flag_test_type(2u,0x15u)||ps4_flag_test_type(2u,0x17u)||(native_main_frame_count&7u))return;
 uint8_t frame=ram8r(0xECF2u),dir=ram8r(0xECF3u);
 if(!dir){frame=(uint8_t)(frame+1u);if(frame>2u){frame=2u;dir=(uint8_t)((dir+1u)&1u);}}
 else{frame=(uint8_t)(frame-1u);if((int8_t)frame<0){frame=0u;dir=(uint8_t)((dir+1u)&1u);}}
 ram8w(0xECF2u,frame);ram8w(0xECF3u,dir);
 dma_copy32(&work_ram[(unsigned)frame*0x9E0u],&md_vram[0x4060u],0x9E0u);
 field_gba_ready_invalidate(0x203u,0x4Fu);hw_publish_canonical_tiles(0x203u,0x4Fu);
}
static unsigned r32_map_has_rate(void){
 for(unsigned i=0u;i<field_update_plan_count;i++)if(r32_rate_shift(field_update_plan_ids[i]))return 1u;
 if(!field_runtime_plan_valid){uint32_t p=field_map.map_updates;for(unsigned i=0u;i<64u;i++){unsigned id=rom8(p++);if(id==255u)break;if(r32_rate_shift(id))return 1u;}}
 return 0u;
}
/* Camera delta is measured at the existing native presentation boundary.
 * Rates act on source 16.16 step counters in authored order; fractional BG
 * movement survives in the original Camera_*_Pos_BG records. */
static int32_t r32_bg_axis(int32_t value,int32_t delta,unsigned cells,unsigned viewport){
 int32_t limit=(int32_t)(cells*32u);limit=limit>(int32_t)viewport?limit-(int32_t)viewport:0;
 int32_t q=value+delta;if(q<0)q=0;if(q>limit*65536)q=limit*65536;return q;
}
__attribute__((noinline))
static void r32_camera_delta(unsigned camx,unsigned camy,unsigned valid){
 if(!r32_map_has_rate())return;
 if(!valid){
  ram32w(0xEF9Cu,(uint32_t)r32_bg_axis(0,(int32_t)camx*65536,(unsigned)field_map.row_bg+1u,240u));
  ram32w(0xEF98u,(uint32_t)r32_bg_axis(0,(int32_t)camy*65536,(unsigned)field_map.col_bg+1u,160u));
  ram32w(0xEC58u,0u);ram32w(0xEC5Cu,0u);
 }else{
  ram32w(0xEC58u,(uint32_t)((int32_t)(int16_t)(camx-ram16r(0xEF94u))*65536));
  ram32w(0xEC5Cu,(uint32_t)((int32_t)(int16_t)(camy-ram16r(0xEF90u))*65536));
  if(field_runtime_plan_valid){for(unsigned i=0u;i<field_update_plan_count;i++)r32_map_rate(field_update_plan_ids[i]);}
  else{uint32_t p=field_map.map_updates;for(unsigned i=0u;i<64u;i++){unsigned id=rom8(p++);if(id==255u)break;r32_map_rate(id);}}
  ram32w(0xEF9Cu,(uint32_t)r32_bg_axis((int32_t)ram32r(0xEF9Cu),(int32_t)ram32r(0xEC58u),(unsigned)field_map.row_bg+1u,240u));
  ram32w(0xEF98u,(uint32_t)r32_bg_axis((int32_t)ram32r(0xEF98u),(int32_t)ram32r(0xEC5Cu),(unsigned)field_map.col_bg+1u,160u));
 }
 if(!field_hscroll_mode)field_hscroll_mode=4u;
}
