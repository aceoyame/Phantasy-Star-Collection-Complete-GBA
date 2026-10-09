/* Original $0D/$0E ship scenes. AD is a native continuation across real
 * RefreshMap calls; destination/option/camera/actors use original PSIV RAM. */
/* Bitmap pages alias the field tile rings. Stop the existing HBlank publisher
 * before borrowing those pages; real RefreshMap/hw_field_init rebuilds them. */
static void r36_bitmap_begin(void){REG_DMA0CNT=0u;field_hscroll_mode=0u;field_hscroll_dirty=0u;field_hscroll_prepared=0u;hw_last_cam_valid=0u;}
static void r36_field_camera_begin(unsigned camx,unsigned camy){
 REG_DMA0CNT=0u;field_hscroll_mode=4u;field_hscroll_dirty=0u;field_hscroll_prepared=0u;hw_hscroll_ring_valid=0u;
 REG_BG0CNT=(uint16_t)((16u<<8)|(1u<<14));REG_BG1CNT=(uint16_t)((18u<<8)|(1u<<14));REG_BG2CNT=(uint16_t)(2u|(20u<<8)|(1u<<14));REG_BG3CNT=(uint16_t)(3u|(22u<<8)|(1u<<14));
 ram32w(0xEF94u,camx<<16);ram32w(0xEF90u,camy<<16);ram32w(0xEF9Cu,camx<<16);ram32w(0xEF98u,camy<<16);ram32w(0xEC50u,0u);ram32w(0xEC58u,0u);
}
static unsigned r36_destinations(unsigned sabotage){
 unsigned mask=rom8(0x63EFFu);
 if(sabotage)mask=rom8(0x76587u);
 else for(uint32_t p=0x63EF2u;rom16(p)!=0xFFFFu;p+=2u)if(ps4_event_flag_test(rom8(p))){mask=rom8(p+1u);break;}
 ram8w(0xED42u,(uint8_t)mask);unsigned n=0u,current=ram8r(0xF400u);
 for(unsigned world=0u;world<6u;world++)if((mask&(0x80u>>world))&&world!=current)ram8w((uint16_t)(0xED43u+n++),(uint8_t)world);
 ram16w(0xED40u,(uint16_t)n);ram8w(0xEC9Eu,0u);return n;
}
static uint32_t r36_table_entry(uint32_t table,unsigned stride,unsigned map){
 for(unsigned i=0u;i<6u;i++)if(rom16(table+i*stride)==map)return table+i*stride;
 return 0u;
}
/* Presentation-only tile planes borrow the field layout RAM until RefreshMap
 * reloads it. Nemesis art remains in the existing virtual VRAM. */
static void r36_scene_map(unsigned plane,uint32_t address,unsigned base,unsigned w,unsigned h,unsigned x,unsigned y,unsigned raw){
 unsigned n=raw?w*h:enigma_decode(address,(uint16_t)base,panel_map_scratch,2048u);
 for(unsigned j=0u;j<h;j++)for(unsigned i=0u;i<w;i++){unsigned k=j*w+i;if(k<n)ram16w((uint16_t)((plane?0xB000u:0xA000u)+((y+j)*64u+x+i)*2u),(uint16_t)(raw?rom16(address+k*2u):panel_map_scratch[k]));}
}
static void r36_scene_planes_clear(void){for(unsigned i=0u;i<0x2000u;i++)ram8w((uint16_t)(0xA000u+i),0u);}
/* One global source projection, matching manga presentation. The original
 * foreground scroll affects only Plane A; stars on Plane B stay stationary. */
__attribute__((noinline))
static void r36_scene_compose(unsigned fgx){
 for(unsigned y=0u;y<160u;y++){unsigned sy=src_y[y],iy=sy&7u;unsigned row=(sy>>3)*64u;
  for(unsigned x=0u;x<240u;x++){unsigned sx=src_x[x],ax=(sx+fgx)&511u;uint16_t a=ram16r((uint16_t)(0xA000u+(row+(ax>>3))*2u)),b=ram16r((uint16_t)(0xB000u+(row+(sx>>3))*2u));unsigned ca=tile_px(a,ax&7u,iy),cb=tile_px(b,sx&7u,iy);field_pause_fb[y*240u+x]=(uint8_t)(ca?((a>>13)&3u)*16u+ca:(cb?((b>>13)&3u)*16u+cb:0u));}
 }
}
static void r36_scene_palette(void){for(unsigned i=0u;i<64u;i++)field_pause_pal[i]=md2gba(cram[i]);ps4_ui_colors();}
static void r36_selector_base(void){
 r36_bitmap_begin();r36_scene_planes_clear();(void)load_nem_vram_auto(0x1D7FAEu,0x100u);(void)load_nem_vram_auto(0x1D8A0Eu,0x1EBu);
 r36_scene_map(0u,0x1DDB88u,0x100u,26u,21u,2u,2u,0u);r36_scene_map(1u,0x1DDC2Au,0x2100u,26u,21u,2u,2u,0u);
 unsigned mask=ram8r(0xED42u);for(unsigned i=0u;i<32u;i++)cram[i]=rom16(0x1DE158u+i*2u);
 if(!ps4_event_flag_test(0xD0u)||!(mask&32u)){
  r36_scene_map(0u,0x1DDBFEu,0x100u,1u,2u,11u,8u,0u);r36_scene_map(0u,0x1DDC06u,0x100u,2u,2u,19u,13u,0u);r36_scene_map(0u,0x1DDC20u,0x100u,2u,2u,12u,5u,0u);
  static const uint8_t xy[7][2]={{13u,4u},{11u,7u},{11u,10u},{25u,7u},{23u,10u},{26u,6u},{22u,11u}};
  for(unsigned i=0u;i<7u;i++)ram16w((uint16_t)(0xA000u+(xy[i][1]*64u+xy[i][0])*2u),(uint16_t)(i<5u?0x21EBu:0x21ECu));cram[14u]=cram[15u]=0x400u;
 }
 if(!(mask&8u))r36_scene_map(0u,0x1DDC12u,0x100u,3u,2u,6u,12u,0u);
 if(!(mask&4u))r36_scene_map(0u,0x1DDBF2u,0x100u,2u,2u,15u,12u,0u);
 r36_scene_compose(0u);waitvb();REG_DISPCNT=FORCE_BLANK;dma_copy32(field_pause_fb,(void*)MODE4_PAGE1,38400u);
}
__attribute__((noinline))
static int r36_select_destination(unsigned n){
 r36_selector_base();ram8w(0x500Au,0xF8u);ps4_ui_wait_buttons_up();uint8_t previous=0u;unsigned sel=0u;
 for(;;){
  dma_copy32((const void*)MODE4_PAGE1,field_pause_fb,38400u);
  for(unsigned i=0u;i<32u;i++)cram[i]=rom16(0x1DE158u+i*2u);
  if(!ps4_event_flag_test(0xD0u)||!(ram8r(0xED42u)&32u))cram[14u]=cram[15u]=0x400u;
  if(ram16r(0xEF1Cu)&8u)cram[7u+ram8r((uint16_t)(0xED43u+sel))]=0xEEEu;
  r36_scene_palette();ps4_ui_box(8u,2u,232u,30u);ps4_ui_draw_rom_name(16u,10u,0x2AAA46u,0u,0xFEu,80u);ps4_ui_box(144u,34u,236u,40u+n*18u);
  for(unsigned i=0u;i<n;i++){unsigned y=44u+i*18u;if(i==sel)pause_draw_cursor(150u,y,83u);ps4_ui_draw_rom_name(162u,y,0x2AAA7Au,ram8r((uint16_t)(0xED43u+i)),0xFEu,i==sel?83u:80u);}
  ps4_ui_present();uint8_t held=md_io_read_psiv_pad(),edge=(uint8_t)(held&~previous);previous=held;
  if(edge&0x70u){ps4_ui_wait_buttons_up();ram8w(0xEC9Eu,(uint8_t)sel);return edge&0x10u?-1:(int)ram8r((uint16_t)(0xED43u+sel));}
  if(edge&1u)sel=sel?sel-1u:n-1u;else if(edge&2u)sel=sel+1u<n?sel+1u:0u;else continue;
  ram8w(0xEC9Eu,(uint8_t)sel);ram8w(0x500Au,0xF2u);
 }
}
static void r36_selector_confirm(unsigned world){
 dma_copy32((const void*)MODE4_PAGE1,field_pause_fb,38400u);r36_scene_palette();ps4_ui_box(8u,112u,232u,158u);
 uint32_t name=0x2AAA7Au;for(unsigned i=0u;i<world;i++){while(rom8(name)!=0xFEu)name++;name++;}
 ps4_ui_draw_rom_name(16u,120u,name,0u,0xFEu,80u);unsigned x=16u,y=120u;while(rom8(name++)!=0xFEu)x+=8u;
 for(uint32_t p=0x2AAA60u;rom8(p)!=0xFFu;p++){unsigned q=rom8(p);if(q==0xFCu){x=16u;y+=12u;}else if(q<87u){pause_draw_window_glyph(x,y,(uint8_t)q,80u);x+=8u;}}
 ps4_ui_present();
}
static void r36_map_wait(unsigned frames){for(unsigned f=0u;f<=frames;f++){field_run_map_updates();waitvb();}}
static void r36_clear_custom(unsigned bytes){
 for(unsigned i=0u;i<bytes;i++)ram8w((uint16_t)(0xC300u+i),0u);
 for(unsigned i=0u;i<field_obj_count&&i<bytes/64u;i++){field_objs[i].active=0u;field_objs[i].type=0u;field_obj_art_stamp[i].epoch=0u;}
}
static void r36_hide_party(void){
 for(unsigned i=0u;i<5u;i++){uint16_t b=(uint16_t)(0xC002u+i*64u);ram8w(b,(uint8_t)(ram8r(b)|2u));}
 unsigned landale=ps4_event_flag_test(0x80u);(void)load_nem_vram_auto(landale?0x1D79B4u:0x1D7E08u,0x580u);
 for(unsigned i=0u;i<16u;i++)cram[32u+i]=rom16((landale?0x1DE0D8u:0x1DE0F8u)+i*2u);field_palette_mirror_live(64u);hw_load_palettes();
}
static void r36_spawn_ship(unsigned landing,unsigned* camx,unsigned* camy){
 uint16_t x=ram16r(0xC030u),y=ram16r(0xC034u);r36_clear_custom(0x300u);
 if(!ps4_event_flag_test(0x80u)){
  r31_actor_spawn(0,landing?0x208u:0x200u,x,(uint16_t)(landing?y-0x100u:y),0u,0x580u);
  if(!landing)r31_actor_spawn(1,0x204u,(uint16_t)(x+12u),(uint16_t)(y+28u),4u,0x580u);
 }else if(landing){r31_actor_spawn(0,0x1CCu,x,(uint16_t)(y-0x120u),24u,0x580u);r31_actor_spawn(1,0x1D0u,x,(uint16_t)(y-0x100u),28u,0x580u);}
 else{
  r31_actor_spawn(0,0x1C0u,(uint16_t)(x+4u),(uint16_t)(y+48u),8u,0x580u);r31_actor_spawn(1,0x1B8u,x,y,0u,0x580u);
  r31_actor_spawn(2,0x1BCu,(uint16_t)(x-4u),(uint16_t)(y+32u),4u,0x580u);r31_actor_spawn(3,0x1C4u,x,(uint16_t)(y+48u),12u,0x580u);
  for(unsigned i=4u;i<12u;i++)r31_actor_spawn((int)i,0x1C8u,x,(uint16_t)(y+64u+(i-4u)*32u),20u,0x580u);
 }
 field_update_npcs(*camx,*camy);r35_camera_present(*camx,*camy);waitvb();
}
/* Wrap only the two overworld maps, as UpdateCameraYPosFG/BG does. */
static void r36_camera_y(unsigned plane){
 uint16_t pos=(uint16_t)(plane?0xEF98u:0xEF90u),step=(uint16_t)(plane?0xEC5Cu:0xEC54u);unsigned cells=(plane?field_map.col_bg:field_map.col_fg)+1u;
 if((ram16r(0xEC28u)&0xFFFEu)==0u){int32_t q=(int32_t)ram32r(pos)+(int32_t)ram32r(step),span=(int32_t)(cells*32u*65536u);if(q<0)q+=span;else if(q>=span)q-=span;ram32w(pos,(uint32_t)q);}
 else r35_camera_axis(pos,step,cells,224u);
}
static void r36_flight_tick(int32_t camera,unsigned updates,unsigned* camx,unsigned* camy){
 ram32w(0xEC54u,(uint32_t)camera);ram32w(0xEC5Cu,(uint32_t)camera);r36_camera_y(0u);r36_camera_y(1u);
 *camx=ram16r(0xEF94u);*camy=ram16r(0xEF90u);field_update_npcs(*camx,*camy);if(updates)field_run_map_updates();r35_camera_present(*camx,*camy);waitvb();
}
static void r36_velocity(unsigned n,uint32_t x,uint32_t y,unsigned setx){
 for(unsigned i=0u;i<n;i++){uint16_t b=(uint16_t)(0xC300u+i*64u);if(setx)ram32w((uint16_t)(b+0x20u),x);ram32w((uint16_t)(b+0x24u),y);}
}
static unsigned r36_pressed(uint8_t* previous){uint8_t held=md_io_read_psiv_pad(),edge=(uint8_t)(held&~*previous);*previous=held;return edge;}
__attribute__((noinline))
static void r36_ascent(unsigned mode,unsigned* camx,unsigned* camy){
 int32_t vy=-65536,camera=-65536;uint8_t previous=md_io_read_psiv_pad();unsigned limit=mode==2u?112u:(mode?228u:419u),skipped=0u;
 for(unsigned f=0u;f<limit;f++){
  if(mode==2u){ram8w(0x500Au,0xD1u);r36_velocity(12u,f,(uint32_t)vy,1u);vy-=0x600;camera-=0x800;}
  else if(f<224u){r36_velocity(12u,0u,(uint32_t)vy,0u);vy-=0x800;camera-=0x800;}
  else if(f<228u){if(!mode&&f>224u)ram8w(0x500Au,0xE3u);r36_velocity(12u,0u,(uint32_t)vy,0u);vy-=0x18000;camera-=0x20000;}
  else if(f<245u){r36_velocity(12u,0u,(uint32_t)vy,0u);vy-=0x18000;}
  else{vy=camera;r36_velocity(12u,0u,(uint32_t)vy,0u);ram16w(0xC42Eu,(uint16_t)(ram16r(0xC42Eu)&0xFFFu));}
  r36_flight_tick(camera,1u,camx,camy);
  if(mode!=2u&&(r36_pressed(&previous)&0x20u)){skipped=1u;break;}
 }
 if(!mode&&!skipped){r36_clear_custom(0x600u);for(unsigned f=0u;f<60u;f++){if((ram16r(0xEF1Cu)&3u)==0u)field_palette_increase_tone_once();field_palette_mirror_live(64u);hw_load_palettes();r36_flight_tick(camera,0u,camx,camy);}}
}
/* Source plane and planet projection share the global 320x224 screen. Wide
 * planet mappings use a bitmap pass instead of exceeding GBA OBJ dimensions. */
static void r36_planet_draw(void){
 const struct FieldObjLite* o=&field_objs[0];const struct R31FieldPose* p=r31_actor_pose(o);if(!p)return;int px=(int)o->x-(int)ram16r(0xEF9Cu)+p->x,py=(int)o->y-(int)ram16r(0xEF98u)+p->y;
 for(unsigned y=0u;y<160u;y++){int yy=(int)src_y[y]-py;if(yy<0||yy>=p->h)continue;for(unsigned x=0u;x<240u;x++){int xx=(int)src_x[x]-px;if(xx<0||xx>=p->w)continue;unsigned at=p->offset+(((unsigned)yy>>3)*(p->w>>3)+((unsigned)xx>>3))*32u+((unsigned)yy&7u)*4u+(((unsigned)xx&7u)>>1);unsigned q=r31_field_pose_data[at],ci=xx&1?q>>4:q&15u;if(ci)field_pause_fb[y*240u+x]=(uint8_t)ci;}}
}
/* Cache the fixed stars and flattened source ship in already-owned scratch.
 * Per-frame work then samples only the moving ship rectangle and planet. */
static void r36_space_cache(unsigned landale){
 unsigned w=landale?128u:96u,h=landale?80u:40u,py=landale?80u:112u;
 for(unsigned y=0u;y<160u;y++){unsigned sy=src_y[y];for(unsigned x=0u;x<240u;x++){unsigned sx=src_x[x];uint16_t e=ram16r((uint16_t)(0xB000u+((sy>>3)*64u+(sx>>3))*2u));unsigned q=tile_px(e,sx&7u,sy&7u);field_pause_fb[y*240u+x]=(uint8_t)(q?((e>>13)&3u)*16u+q:0u);}}
 REG_DISPCNT=FORCE_BLANK;dma_copy32(field_pause_fb,(void*)MODE4_PAGE1,38400u);
 for(unsigned y=0u;y<h;y++)for(unsigned x=0u;x<w;x++){unsigned sx=x+48u,sy=y+py;uint16_t e=ram16r((uint16_t)(0xA000u+((sy>>3)*64u+(sx>>3))*2u));unsigned q=tile_px(e,sx&7u,sy&7u);panel_art_scratch[y*w+x]=(uint8_t)(q?((e>>13)&3u)*16u+q:0u);}
}
static void r36_space_ship_draw(unsigned landale,unsigned fgx){
 unsigned w=landale?128u:96u,h=landale?80u:40u,py=landale?80u:112u;
 dma_copy32((const void*)MODE4_PAGE1,field_pause_fb,38400u);
 for(unsigned y=0u;y<160u;y++){unsigned sy=src_y[y];if(sy<py||sy>=py+h)continue;const uint8_t* row=panel_art_scratch+(sy-py)*w;
  for(unsigned x=0u;x<240u;x++){unsigned sx=(src_x[x]+fgx)&511u;if(sx<48u||sx>=48u+w)continue;unsigned q=row[sx-48u];if(q)field_pause_fb[y*240u+x]=(uint8_t)q;}
 }
}
static void r36_space_caption(unsigned world){
 uint32_t p=rom32(0x2AAADEu+world*4u);
 for(unsigned y=0u;y<160u;y++){unsigned sy=src_y[y];if(sy<184u||sy>=200u)continue;
  for(unsigned x=0u;x<240u;x++){unsigned sx=src_x[x];if(sx<32u)continue;unsigned q=rom8(p+(sx-32u)/8u);if(q==0xFFu)break;if(q<0x80u&&(rom8(0x2A3542u+q*16u+sy-184u)&(0x80u>>((sx-32u)&7u))))field_pause_fb[y*240u+x]=47u;}
 }
}
static void r36_space_palette(void){
 unsigned f=(uint8_t)(ram8r(0xECF3u)+1u);ram8w(0xECF3u,(uint8_t)f);
 if(!(f&7u)){unsigned q=(f&0x38u)>>2;for(unsigned i=0u;i<4u;i++)cram[57u+i]=rom16(0x64BDEu+q+i*16u);}
 cram[28u]=f&1u?0xEEEu:0xEE0u;cram[29u]=f&1u?0xEE0u:0x660u;
 unsigned world=ram8r(0xF400u);if((world==3u||world==4u)&&!(ram16r(0xEF1Cu)&7u)){unsigned q=ram8r(0xECF2u);if(q>=6u)q=0u;static const uint16_t cycle[12]={0x68u,0x28Au,0x4ACu,0x6CEu,0x4ACu,0x28Au,0xCE0u,0xAC0u,0x8A0u,0x680u,0x8A0u,0xAC0u};cram[12u]=cycle[q];cram[13u]=cycle[q+6u];ram8w(0xECF2u,(uint8_t)(q+1u));}
}
__attribute__((noinline))
static void r36_space_pass(void){
 unsigned world=ram8r(0xF400u),landale=ps4_event_flag_test(0x80u);r36_bitmap_begin();r36_scene_planes_clear();r36_clear_custom(0x600u);
 ram8w(0xEC24u,1u);(void)load_nem_vram_auto(0x1D932Cu,0x400u);r36_scene_map(1u,0x1DD922u,0x6400u,40u,18u,0u,5u,0u);
 (void)load_nem_vram_auto(landale?0x1D8A1Eu:0x1D900Cu,0x100u);r36_scene_map(0u,landale?0x1DD76Au:0x1DD8AAu,0u,landale?16u:12u,landale?10u:5u,6u,landale?10u:14u,1u);
 for(unsigned i=0u;i<16u;i++){cram[16u+i]=rom16(0x1DE118u+i*2u);cram[48u+i]=rom16(0x1DE138u+i*2u);cram[i]=rom16(rom32(0x64550u+world*4u)+i*2u);}
 r31_actor_spawn(0,0x210u,0x160u,0x30u,(uint8_t)(world*4u),0x200u);ram32w(0xC320u,0xFFFF2000u);r31_actor_update_one(0u);
 ram32w(0xEF94u,0x600000u);ram32w(0xEF9Cu,0u);ram32w(0xEF98u,0u);cram[47u]=0u;int32_t velocity=-65536;unsigned freeze=0u;uint8_t previous=md_io_read_psiv_pad();
 r36_space_cache(landale);r36_space_ship_draw(landale,ram16r(0xEF94u));r36_planet_draw();r36_scene_palette();r36_space_caption(world);ps4_ui_present();field_palette_fade_in();
 for(unsigned count=300u;count>0u;count--){
  if(r36_pressed(&previous)&0x20u)break;r31_actor_update();r36_space_palette();r36_space_ship_draw(landale,ram16r(0xEF94u));r36_planet_draw();r36_scene_palette();
  r36_space_caption(world);ps4_ui_present();
  if(!freeze){ram32w(0xEF94u,ram32r(0xEF94u)+(uint32_t)velocity);velocity+=0x170;if(count==110u){cram[47u]=0xEEEu;freeze=1u;}}
 }
 field_palette_fade_out();field_cutscene_panels=0u;field_active_panel_count=0u;
}
/* $5ABDC moves Y only, takes the shortest wrapped route, and permits Start. */
static unsigned r36_landing_camera(unsigned* camx,unsigned* camy){
 unsigned target=(((unsigned)ram16r(0xEC4Au)*8u+0x100u)&0xFF8u)-0x58u;target&=0xFFFu;
 int delta=(int)target-(int)ram16r(0xEF90u);if(delta>0x7FF)delta-=0x1000;else if(delta<-(int)0x7FF)delta+=0x1000;int step=delta<0?-2:2;unsigned remaining=(unsigned)(delta<0?-delta:delta);uint8_t previous=md_io_read_psiv_pad();
 while(remaining){if(r36_pressed(&previous)&0x80u)return 1u;field_run_map_updates();r36_flight_tick(step*65536,0u,camx,camy);remaining=remaining>2u?remaining-2u:0u;}return 0u;
}
static void r36_land(unsigned* camx,unsigned* camy){
 r36_hide_party();r36_spawn_ship(1u,camx,camy);field_palette_fade_in();if(r36_landing_camera(camx,camy))return;
 for(unsigned i=0u;i<2u;i++){uint16_t b=(uint16_t)(0xC334u+i*64u);ram16w(b,(uint16_t)(ram16r(b)+0x100u));}
 field_update_npcs(*camx,*camy);r35_camera_present(*camx,*camy);waitvb();uint8_t previous=md_io_read_psiv_pad();
 for(int32_t v=0x20000;v>=0;v-=0x200){r36_velocity(2u,0u,(uint32_t)v,0u);field_update_npcs(*camx,*camy);field_run_map_updates();r35_camera_present(*camx,*camy);waitvb();if(r36_pressed(&previous)&0x20u)break;}
}
static int r36_sabotage_dialogue(unsigned* camx,unsigned* camy){
 r36_bitmap_begin();r33_party_position(lead_char_id,0x80u,0x80u,0u);ram8w(0x500Au,0x9Fu);r34_panels_begin();r34_panel(0x7Au,0x59u);r34_panel(0x7Cu,0x3Bu);
 r33_dialogue_tree(0x1F81B0u);ram8w(0x500Au,0xF3u);field_dialogue_show_keep_panels(2u,*camx,*camy);
 for(unsigned i=0u;i<3u;i++)pause_panel_action(1u,0u);r34_vint_wait(0x13u);r34_panel(0x7Du,0x3Bu);ram8w(0x500Au,0xA9u);waitvb();r34_panel(0x7Eu,9u);r31_resume_dialogue(camx,camy);
 ps4_event_flag_set(0x71u);ram8w(0xECF4u,0u);ram8w(0xEC9Eu,0u);ram8w(0xECECu,0xA9u);field_event_set_mapload_bit(7u);field_event_set_mapload_bit(3u);field_custom_resume_event=0xFFu;field_custom_resume_phase=0u;
 /* RunEvent $29 on the takeoff map enters $800F only after the real battle
  * returns successfully; defeat requests the title in the existing runner. */
 return field_event_run_battle_request(8u,camx,camy);
}
__attribute__((noinline))
static int r36_spaceship_resume(unsigned* camx,unsigned* camy){
 if(field_custom_resume_event!=0xADu)return 0;unsigned phase=field_custom_resume_phase;
 if(phase<=2u){
  r36_field_camera_begin(*camx,*camy);
  r36_hide_party();r36_spawn_ship(0u,camx,camy);ram8w(0xECF4u,1u);field_palette_fade_in();r36_ascent(phase,camx,camy);field_palette_fade_out();
  if(phase==2u)return r36_sabotage_dialogue(camx,camy);
  r36_clear_custom(0x600u);r35_camera_present(*camx,*camy);waitvb();r36_map_wait(0x3Bu);
  /* First Rykros arrival runs original cutscene $1B before the space pass. */
  if(ram8r(0xF400u)==2u&&!ps4_event_flag_test(0xD0u)&&ps4_event_flag_test(rom8(0x63EF2u))){
   field_custom_resume_event=0xFFu;if(!field_dispatch_event_index(0x801Bu,camx,camy))return 0;
   /* The original JSR caller continues its space pass after Rykros returns. */
  }
  r36_space_pass();r36_map_wait(0x3Bu);unsigned world=ram8r(0xF400u);uint32_t p=0x64B34u+world*6u;
  r34_queue(0xADu,3u,(uint16_t)rom16(p),ram16r(0xEC28u),(uint16_t)rom16(p+2u),(uint16_t)rom16(p+4u),0u,0u);return 1;
 }
 if(phase==3u){
  r36_field_camera_begin(*camx,*camy);ram8w(0xEC24u,0u);
  r36_land(camx,camy);field_palette_fade_out();uint32_t p=0x64B5Au+ram8r(0xF400u)*6u;
  r34_queue(0xADu,4u,(uint16_t)rom16(p),ram16r(0xEC28u),(uint16_t)rom16(p+2u),(uint16_t)rom16(p+4u),0u,0u);return 1;
 }
 if(phase>=4u&&phase<=6u){
  ram8w(0xECF4u,0u);ram8w(0xEC9Eu,0u);ram8w(0xEC4Eu,(uint8_t)(ram8r(0xEC4Eu)&~8u));if(phase==5u)ram16w(0xEC2Au,0xFFFFu);if(phase!=4u)ram8w(0x500Au,0xFDu);
  field_custom_resume_event=0xFFu;field_custom_resume_phase=0u;field_palette_fade_in();return 1;
 }
 return 0;
}
__attribute__((noinline))
static int r36_spaceship_begin(unsigned sabotage,unsigned* camx,unsigned* camy){
 for(unsigned i=0u;i<16u;i++)ram8w((uint16_t)(0xE200u+i),0u);for(unsigned i=0u;i<0x800u;i++)ram8w((uint16_t)(0xC000u+i),0u);
 unsigned n=r36_destinations(sabotage);if(!n)return 0;int selected=r36_select_destination(n);uint16_t map=ram16r(0xEC28u);uint32_t p;
 if(selected<0){field_palette_fade_out();p=r36_table_entry(0x64B5Au,6u,map);if(!p)return 0;r34_queue(0xADu,sabotage?6u:5u,map,ram16r(0xEC2Au),(uint16_t)rom16(p+2u),(uint16_t)rom16(p+4u),0u,0u);return 1;}
 ram8w(0x500Au,0xF3u);r36_selector_confirm((unsigned)selected);ram8w(0x500Au,0xFDu);if(sabotage)r36_map_wait(0x3Bu);else r34_vint_wait(0x3Bu);
 field_palette_fade_out();field_cutscene_panels=0u;field_active_panel_count=0u;ram8w(0xF400u,(uint8_t)selected);p=r36_table_entry(0x64B02u,8u,map);if(!p)return 0;
 if(sabotage){ram8w(0x500Au,0xFBu);waitvb();}else if(ram8r(0xECECu)!=0x9Bu){ram8w(0x500Au,0x9Bu);ram8w(0xECECu,0x9Bu);}
 r34_queue(0xADu,sabotage?2u:((map==0xBFu||map==0xD4u)?0u:1u),(uint16_t)rom16(p+2u),sabotage?0xFFFFu:map,(uint16_t)rom16(p+4u),(uint16_t)rom16(p+6u),0u,0u);return 1;
}
