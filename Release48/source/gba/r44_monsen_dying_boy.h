/* Last two ordinary EventPtrs: earthquake $2C and dying boy $86. */
static void r44_npc_face(unsigned slot,uint8_t face,unsigned x,unsigned y){
 if(slot>=field_obj_count||!field_objs[slot].active)return;uint16_t b=(uint16_t)(0xC300u+slot*64u);struct FieldObjLite*o=&field_objs[slot];o->facing=face;ram16w((uint16_t)(b+6u),face);ram8w((uint16_t)(b+0x10u),0xFFu);ram8w((uint16_t)(b+0x11u),0u);
 if(r31_actor_type(o->type))r31_actor_animate(slot);else{o->anim=0u;hw_refresh_field_object_art(slot);}field_event_redraw_explicit(x,y);waitvb();
}
static void r44_party_faces(unsigned face,unsigned x,unsigned y,unsigned skip_rune){for(unsigned i=0u;i<5u;i++){unsigned id=ram8r((uint16_t)(0xF40Au+i));if(id==0xFFu)break;if(skip_rune&&id==3u)continue;r38_party_face((uint8_t)id,(uint8_t)face,x,y,1u);}}
static void r44_dialogue(unsigned entry,unsigned x,unsigned y){ram8w(0x500Au,0xF3u);ram8w(0xECFDu,1u);(void)field_dialogue_show(entry,x,y);}
static void r44_resume(unsigned x,unsigned y){ram8w(0x500Au,0xF3u);ram8w(0xECFDu,1u);(void)field_dialogue_resume_saved(x,y);}
static void r44_camera_update(unsigned* x,unsigned* y){
 r35_camera_axis(0xEF94u,0xEC50u,field_map.row_fg+1u,320u);r35_camera_axis(0xEF90u,0xEC54u,field_map.col_fg+1u,224u);r35_camera_axis(0xEF9Cu,0xEC58u,field_map.row_bg+1u,320u);r35_camera_axis(0xEF98u,0xEC5Cu,field_map.col_bg+1u,224u);*x=ram16r(0xEF94u);*y=ram16r(0xEF90u);
}
static void r44_quake(unsigned frames,unsigned diagonal,unsigned* x,unsigned* y){
 ram16w(0xECD0u,0u);
 for(unsigned f=0u;f<frames;f++){
  unsigned table=diagonal&&f<2u?0x6E73Au:0x6E6B6u;unsigned p=table+ram16r(0xECD0u)*16u;
  for(unsigned a=0u;a<4u;a++)ram32w((uint16_t)(0xEC50u+a*4u),rom32(p+a*4u));ram16w(0xEF02u,0u);r44_objects(*x,*y);r44_camera_update(x,y);field_run_map_updates();r35_camera_present(*x,*y);hw_update_objects(*x,*y);
  if(!(ram8r(0xECF2u)&3u))ram8w(0x500Au,0xDDu);
  /* loc_6E634 advances both counters before VBlank; the first loop does so after. */
  if(diagonal){ram16w(0xECD0u,(uint16_t)((ram16r(0xECD0u)+1u)&1u));ram8w(0xECF2u,(uint8_t)(ram8r(0xECF2u)+1u));}waitvb();
  if(!diagonal){ram16w(0xECD0u,(uint16_t)((ram16r(0xECD0u)+1u)&1u));ram8w(0xECF2u,(uint8_t)(ram8r(0xECF2u)+1u));}
 }
}
static void r44_villager(unsigned i,unsigned base,unsigned x,unsigned y,unsigned camx,unsigned camy){
 r31_actor_spawn((int)i,0x8194u,(uint16_t)x,(uint16_t)y,0u,(uint16_t)base);uint16_t b=(uint16_t)(0xC300u+i*64u);ram8w((uint16_t)(b+0x13u),0x60u);/* NPC initialization overwrites the source spawn's mistaken Y destination. */ram16w((uint16_t)(b+0x3Au),(uint16_t)y);r44_event_actor_update(i);field_event_redraw_explicit(camx,camy);waitvb();
}
static void r44_npc_move(unsigned i,unsigned tx,unsigned ty,unsigned x,unsigned y){
 uint16_t b=(uint16_t)(0xC300u+i*64u);ram16w((uint16_t)(b+0x38u),(uint16_t)tx);ram16w((uint16_t)(b+0x3Au),(uint16_t)ty);
 do{ram16w(0xEF02u,0u);r44_event_actor_update(i);field_run_map_updates();field_event_redraw_explicit(x,y);waitvb();}while(ram16r((uint16_t)(b+0x30u))!=tx||ram16r((uint16_t)(b+0x34u))!=ty);
 ram32w((uint16_t)(b+0x20u),0u);ram32w((uint16_t)(b+0x24u),0u);
}
/* Event_MoveCamera: map updates, both camera planes, then ALL field objects. */
static void r44_camera(uint16_t wx,uint16_t wy,unsigned*x,unsigned*y){
 unsigned target[2]={((unsigned)wx-0x98u)&0xFFFu,((unsigned)wy-0x58u)&0xFFFu};
 for(unsigned axis=0u;axis<2u;axis++){
  uint16_t pos=axis?0xEF90u:0xEF94u,bg=axis?0xEF98u:0xEF9Cu,step=axis?0xEC54u:0xEC50u,bgstep=axis?0xEC5Cu:0xEC58u;
  unsigned current=ram16r(pos),delta=(target[axis]-current)&0xFFFu;int direction=delta<=0x7FFu?1:-1;unsigned count=direction>0?delta:0x1000u-delta;
  for(unsigned f=0u;f<count;f++){
   ram16w(0xEF02u,0u);ram16w(step,(uint16_t)direction);ram16w(bgstep,(uint16_t)direction);field_run_map_updates();
   r35_camera_axis(pos,step,(axis?field_map.col_fg:field_map.row_fg)+1u,axis?224u:320u);r35_camera_axis(bg,bgstep,(axis?field_map.col_bg:field_map.row_bg)+1u,axis?224u:320u);
   *x=ram16r(0xEF94u);*y=ram16r(0xEF90u);r44_objects(*x,*y);r35_camera_present(*x,*y);hw_update_objects(*x,*y);waitvb();
  }
 }
}
__attribute__((noinline))
static int r44_event_monsen(unsigned* x,unsigned* y){
 r44_npc_face(6u,(uint8_t)(ram16r(0xC006u)^4u),*x,*y);r44_dialogue(0x40u,*x,*y);ram8w(0xECFEu,(uint8_t)(ram8r(0xECFEu)|4u));ram8w(0xECF2u,0u);r44_quake(120u,0u,x,y);
 r44_npc_face(6u,0u,*x,*y);field_event_set_party_dest(0u,0x2E0u,0x220u);field_event_set_party_dest(4u,0x2F0u,0x220u);field_event_set_party_dest(5u,0x300u,0x220u);field_event_set_party_dest(6u,0x2F0u,0x230u);field_event_set_party_dest(3u,0x2A0u,0x210u);ram8w(0xECFEu,(uint8_t)(ram8r(0xECFEu)|3u));r44_quake(60u,1u,x,y);
 for(unsigned a=0u;a<4u;a++)ram32w((uint16_t)(0xEC50u+a*4u),0u);ram16w(0xEF02u,0u);r44_objects(*x,*y);r44_camera_update(x,y);field_run_map_updates();r35_camera_present(*x,*y);hw_update_objects(*x,*y);waitvb();
 r38_party_face(0u,8u,*x,*y,1u);r38_party_face(4u,0u,*x,*y,1u);r38_party_face(5u,12u,*x,*y,1u);r38_party_face(6u,4u,*x,*y,1u);r38_party_face(3u,12u,*x,*y,1u);ram8w(0xECFEu,(uint8_t)(ram8r(0xECFEu)&~2u));r44_resume(*x,*y);
 for(unsigned f=0u;f<20u;f++){r38_party_face(4u,(uint8_t)((f&3u)*4u),*x,*y,1u);r34_vint_wait(1u);}r44_resume(*x,*y);r38_party_face(4u,0u,*x,*y,1u);r36_map_wait(0x3Bu);r44_resume(*x,*y);
 r38_party_face(0u,0u,*x,*y,1u);r38_party_face(5u,0u,*x,*y,1u);r44_resume(*x,*y);r38_party_face(4u,12u,*x,*y,1u);r44_resume(*x,*y);
 r44_villager(8u,0x42Bu,0x230u,0x220u,*x,*y);r44_npc_move(8u,0x2D0u,0x220u,*x,*y);r44_party_faces(12u,*x,*y,1u);r44_resume(*x,*y);
 r44_villager(9u,0x3E3u,0x240u,0x220u,*x,*y);r44_npc_move(9u,0x2E0u,0x230u,*x,*y);r44_npc_face(9u,4u,*x,*y);r44_party_faces(0u,*x,*y,1u);r44_resume(*x,*y);
 r38_party_face(0u,12u,*x,*y,1u);r36_map_wait(0x1Du);r38_party_face(0u,0u,*x,*y,1u);r36_map_wait(0x1Du);r38_party_face(0u,12u,*x,*y,1u);r36_map_wait(0x1Du);r38_party_face(0u,0u,*x,*y,1u);r44_resume(*x,*y);
 r44_villager(10u,0x42Bu,0x3A0u,0x210u,*x,*y);ram16w(0xC5B8u,0x310u);ram16w(0xC5BAu,0x210u);r44_villager(11u,0x39Bu,0x3A0u,0x220u,*x,*y);ram16w(0xC5F8u,0x310u);ram16w(0xC5FAu,0x220u);r44_main_wait(120u,*x,*y,0u,0u);r44_resume(*x,*y);
 static const uint16_t tx[4]={0x230u,0x230u,0x3D0u,0x3D0u},ty[4]={0x220u,0x220u,0x210u,0x220u};for(unsigned i=0u;i<4u;i++){ram16w((uint16_t)(0xC538u+i*64u),tx[i]);ram16w((uint16_t)(0xC53Au+i*64u),ty[i]);}ram8w(0xECFEu,(uint8_t)(ram8r(0xECFEu)|2u));r44_main_wait(120u,*x,*y,0u,0u);ram8w(0xECFEu,(uint8_t)(ram8r(0xECFEu)&~2u));for(unsigned i=8u;i<12u;i++)r31_actor_clear(i);
 ram8w(0xECFEu,(uint8_t)(ram8r(0xECFEu)&~5u));r44_party_move(lead_char_id,0x310u,0x220u,0u,1u,*x,*y);r44_camera(ram16r(0xC030u),ram16r(0xC034u),x,y);ram8w(0xECF2u,0u);ps4_event_flag_set(0x60u);return 1;
}
/* GetItem counts occupied bytes, searches that many initial slots, and
 * ReorderInventory shifts from the first zero including the padding byte. */
static int r44_take_alis_sword(void){
 unsigned count=0u;for(unsigned i=0u;i<40u;i++)if(ram8r((uint16_t)(PS4_INV_BASE+i)))count++;
 unsigned found=0u;for(unsigned i=0u;i<count;i++)if(ram8r((uint16_t)(PS4_INV_BASE+i))==0x8Au){ram8w((uint16_t)(PS4_INV_BASE+i),0u);found=1u;break;}
 if(!found)return 0;for(unsigned i=0u;i<40u;i++)if(!ram8r((uint16_t)(PS4_INV_BASE+i))){for(unsigned q=i;q<40u;q++)ram8w((uint16_t)(PS4_INV_BASE+q),ram8r((uint16_t)(PS4_INV_BASE+q+1u)));break;}return 1;
}
__attribute__((noinline))
static int r44_event_dying_boy(unsigned* x,unsigned* y){
 r44_npc_face(2u,(uint8_t)(ram16r(0xC006u)^4u),*x,*y);
 if(ps4_event_flag_test(0xAAu)){r44_dialogue(0x3Eu,*x,*y);return 1;}
 if(!ps4_event_flag_test(0xA9u)||!r44_take_alis_sword()){r44_dialogue(0x3Bu,*x,*y);return 1;}
 r44_dialogue(0x33u,*x,*y);ram8w(0xECFEu,(uint8_t)(ram8r(0xECFEu)|4u));
 field_event_set_party_dest(0u,0x1F0u,0x1C0u);field_event_set_party_dest(3u,0x1E0u,0x1A0u);field_event_set_party_dest(5u,0x1F0u,0x1A0u);field_event_set_party_dest(7u,0x200u,0x1A0u);
 static const uint8_t candidates[6]={1u,4u,6u,8u,9u,10u};for(unsigned i=0u;i<6u;i++)if(field_event_party_slot_for_id(candidates[i])>=0){field_event_set_party_dest(candidates[i],0x210u,0x1A0u);break;}
 ram8w(0xECE0u,0u);ram8w(0xECFEu,(uint8_t)(ram8r(0xECFEu)|1u));r44_main_wait(150u,*x,*y,1u,0u);r44_party_faces(0u,*x,*y,0u);r38_party_face(0u,12u,*x,*y,1u);r44_resume(*x,*y);
 r31_party_hide(0u,1u);r31_actor_spawn(6,0x334u,0x1D0u,0x1B8u,0u,0x43Cu);r44_event_actor_update(6u);field_event_redraw_explicit(*x,*y);waitvb();
 do{r44_event_actor_update(6u);field_event_redraw_explicit(*x,*y);waitvb();}while(ram16r(0xC49Cu)!=0xFFFFu);
 r31_party_hide(0u,0u);r31_actor_clear(6u);field_event_redraw_explicit(*x,*y);waitvb();r44_resume(*x,*y);r44_party_move(0u,0x1A0u,0x1C0u,0u,1u,*x,*y);ram8w(0xECFEu,(uint8_t)(ram8r(0xECFEu)&~1u));r44_resume(*x,*y);r44_party_move(lead_char_id,0x1A0u,0x1D0u,1u,0u,*x,*y);
 ram8w(0xECE0u,1u);ram8w(0xECFEu,(uint8_t)(ram8r(0xECFEu)&~4u));ps4_event_flag_set(0xAAu);return 1;
}
