/* Original GetAutoInput/FieldObj_Move/step counters in existing 64-byte
 * records. Fixed-count scenes need simultaneous destinations across ticks. */
static unsigned r44_record_move(uint16_t b,unsigned party){
 unsigned moved=0u;
 if(!ram16r((uint16_t)(b+0x28u))&&!ram16r((uint16_t)(b+0x2Au))){
  int xd=field_event_wrapped_dir(ram16r((uint16_t)(b+0x30u)),ram16r((uint16_t)(b+0x38u)),field_event_axis_extent(0u));
  int yd=field_event_wrapped_dir(ram16r((uint16_t)(b+0x34u)),ram16r((uint16_t)(b+0x3Au)),field_event_axis_extent(1u));
  int dx=0,dy=0;if((ram8r(0xECFEu)&2u)&&yd)dy=yd;else if(xd)dx=xd;else if(yd)dy=yd;
  ram16w((uint16_t)(b+0x28u),dx?0x1000u:0u);ram16w((uint16_t)(b+0x2Au),dy?0x1000u:0u);
  unsigned speed=field_event_source_speed();ram32w((uint16_t)(b+0x20u),(uint32_t)(dx*(int)speed*65536));ram32w((uint16_t)(b+0x24u),(uint32_t)(dy*(int)speed*65536));
  if(dx||dy){ram16w((uint16_t)(b+6u),(uint16_t)(dx>0?8u:(dx<0?12u:(dy>0?0u:4u))));
   if(party&&!(ram8r(0xECFEu)&1u)&&(unsigned)b+0x40u<0xC180u){ram16w((uint16_t)(b+0x78u),ram16r((uint16_t)(b+0x30u)));ram16w((uint16_t)(b+0x7Au),ram16r((uint16_t)(b+0x34u)));}
  }else{ram8w((uint16_t)(b+0x10u),0u);ram8w((uint16_t)(b+0x11u),1u);}
  ram8w((uint16_t)(b+2u),(uint8_t)(ram8r((uint16_t)(b+2u))|64u));
 }
 for(unsigned a=0u;a<2u;a++){
  uint16_t v=(uint16_t)(b+0x20u+a*4u),d=(uint16_t)(b+0x28u+a*2u);int32_t velocity=(int32_t)ram32r(v);
  if(velocity){unsigned n=(unsigned)(velocity<0?-velocity:velocity)>>8;uint16_t left=(uint16_t)(ram16r(d)-n);ram16w(d,left);
   if(left&0x8000u){int32_t correction=(int32_t)(int16_t)left*256;if(velocity<0)correction=-correction;velocity+=correction;ram32w(v,(uint32_t)velocity);ram32w((uint16_t)(b+0x28u),0u);}
   moved=1u;
  }
  uint16_t p=(uint16_t)(b+0x30u+a*4u);ram32w(p,ram32r(p)+ram32r(v));
 }
 return moved;
}
static void r44_party_record(unsigned slot){
 uint8_t id=ram8r((uint16_t)(0xF40Au+slot));if(id==0xFFu)return;uint16_t b=(uint16_t)(0xC000u+slot*64u);unsigned moved=r44_record_move(b,1u);
 field_event_sync_party_carrier(slot,id,(uint8_t)moved);
 if(!moved){if(id==lead_char_id){chaz.anim=chaz.anim_clock=0u;}else{struct PartyFollowerLite*f=field_event_follower_for_id(id);if(f)f->anim=f->anim_clock=0u;}}
}
static void r44_objects(unsigned x,unsigned y){for(unsigned i=0u;i<5u&&ram8r((uint16_t)(0xF40Au+i))!=0xFFu;i++)r44_party_record(i);field_update_npcs(x,y);}
static void r44_main_wait(unsigned frames,unsigned x,unsigned y,unsigned map,unsigned draw){
 for(unsigned f=0u;f<frames;f++){ram16w(0xEF02u,0u);r44_objects(x,y);if(map)field_run_map_updates();if(draw)field_event_redraw_explicit(x,y);waitvb();}
}
static void r44_party_move(uint8_t id,unsigned tx,unsigned ty,unsigned all,unsigned draw,unsigned x,unsigned y){
 int slot=field_event_party_slot_for_id(id);if(slot<0)return;uint16_t b=(uint16_t)(0xC000u+(unsigned)slot*64u);field_event_set_party_dest(id,(uint16_t)tx,(uint16_t)ty);
 do{ram16w(0xEF02u,0u);if(all)r44_objects(x,y);else r44_party_record((unsigned)slot);field_run_map_updates();if(draw)field_event_redraw_explicit(x,y);waitvb();}while(ram16r((uint16_t)(b+0x30u))!=tx||ram16r((uint16_t)(b+0x34u))!=ty);
 ram32w((uint16_t)(b+0x20u),0u);ram32w((uint16_t)(b+0x24u),0u);
}
