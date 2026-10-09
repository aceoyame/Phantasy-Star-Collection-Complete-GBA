/* Original character-graphics NPCs with GetAutoInput, 16-pixel steps and
 * Char_Move_Flags axis ordering. Existing source records hold the step state. */
__attribute__((noinline)) static int r43_npc_char(unsigned t){switch(t){case 0x70u:return 3;case 0x264u:return 5;case 0x2C8u:return 1;case 0x2CCu:return 4;case 0x2D0u:return 6;case 0x2D4u:return 8;case 0x2D8u:return 9;default:return -1;}}
static void r43_npc_init(unsigned i){struct FieldObjLite*o=&field_objs[i];uint16_t b=(uint16_t)(0xC300u+i*64u);
 if(!(ram16r(b)&0x8000u)){ram16w(b,(uint16_t)(o->type|0x8000u));ram16w((uint16_t)(b+0x28u),0u);ram16w((uint16_t)(b+0x2Au),0u);ram16w((uint16_t)(b+0x38u),o->x);ram16w((uint16_t)(b+0x3Au),o->y);o->timer=0u;}
}
__attribute__((noinline)) static int r43_npc_update(unsigned i){
 struct FieldObjLite*o=&field_objs[i];int id=r43_npc_char(o->type);if(id<0)return 0;uint16_t b=(uint16_t)(0xC300u+i*64u);
 r43_npc_init(i);
 unsigned left=ram16r((uint16_t)(b+0x28u))|ram16r((uint16_t)(b+0x2Au));
 if(!left){int xd=field_event_wrapped_dir(o->x,ram16r((uint16_t)(b+0x38u)),field_event_axis_extent(0u)),yd=field_event_wrapped_dir(o->y,ram16r((uint16_t)(b+0x3Au)),field_event_axis_extent(1u));
  int dx=0,dy=0;if((ram8r(0xECFEu)&2u)&&yd)dy=yd;else if(xd)dx=xd;else if(yd)dy=yd;
  o->move_dir=(uint8_t)(dx>0?4u:(dx<0?3u:(dy>0?2u:(dy<0?1u:0u))));
  ram16w((uint16_t)(b+0x28u),dx?0x1000u:0u);ram16w((uint16_t)(b+0x2Au),dy?0x1000u:0u);left=(dx||dy)?0x1000u:0u;
  if(left){o->facing=(uint8_t)(dx>0?8u:(dx<0?12u:(dy>0?0u:4u)));ram16w((uint16_t)(b+6u),o->facing);}
 }
 if(left){unsigned n=field_event_source_speed();unsigned axis=o->move_dir>=3u?0x28u:0x2Au;unsigned rem=ram16r((uint16_t)(b+axis));if(n>rem/256u)n=rem/256u;
  int dx=o->move_dir==4u?1:(o->move_dir==3u?-1:0),dy=o->move_dir==2u?1:(o->move_dir==1u?-1:0);unsigned span=field_event_axis_extent(axis==0x2Au);int p=(axis==0x28u?o->x:o->y)+(dx+dy)*(int)n;if(span){if(p<0)p+=(int)span;else if(p>=(int)span)p-=(int)span;}
  if(axis==0x28u)o->x=(uint16_t)p;else o->y=(uint16_t)p;ram16w((uint16_t)(b+axis),(uint16_t)(rem-n*256u));
  ram32w((uint16_t)(b+0x20u),(uint32_t)(dx*(int)n*65536));ram32w((uint16_t)(b+0x24u),(uint32_t)(dy*(int)n*65536));
  if(++o->timer>=field_party_anim_period((uint8_t)id)){o->timer=0u;o->anim=(uint8_t)((o->anim+1u)&3u);}
 }else{o->anim=0u;o->timer=0u;ram32w((uint16_t)(b+0x20u),0u);ram32w((uint16_t)(b+0x24u),0u);}
 ram16w((uint16_t)(b+0x30u),o->x);ram16w((uint16_t)(b+0x34u),o->y);hw_refresh_field_object_art(i);return 1;
}

__attribute__((noinline)) static int r43_npc_art(unsigned i){
 const struct FieldObjLite*o=&field_objs[i];int id=o->type==0x68u?1:(o->type==0x6Cu?2:r43_npc_char(o->type));if(id<0)return 0;
 hw_copy_frame_rom(o->gba_tile,char_field_art_address((unsigned)id),sprite_frame_offset((uint8_t)((o->facing>>2)&3u),o->anim));return 1;
}
