/* New source poses append to the complete accepted R43 bank. */
static int r44_monsen_actor(const struct FieldObjLite*o){return o->type==0x8194u&&(o->base_tile==0x42Bu||o->base_tile==0x3E3u||o->base_tile==0x39Bu);}
static void r44_event_actor_update(unsigned i){
 struct FieldObjLite*o=&field_objs[i];uint16_t b=(uint16_t)(0xC300u+i*64u);
 if(o->type==0x334u){
  if(!(ram16r(b)&0x8000u)){
   ram16w(b,0x8334u);ram32w((uint16_t)(b+8u),0x4C39Cu);ram32w((uint16_t)(b+12u),0x4CF5Au);ram8w((uint16_t)(b+0x13u),0x40u);ram16w((uint16_t)(b+0x1Cu),0u);
   ram16w((uint16_t)(b+0x30u),0x1D0u);ram16w((uint16_t)(b+0x34u),0x1B8u);ram16w((uint16_t)(b+0x16u),0x43Cu);o->base_tile=0x43Cu;ram8w((uint16_t)(b+2u),(uint8_t)(ram8r((uint16_t)(b+2u))&~8u));
  }
  ram32w((uint16_t)(b+0x30u),ram32r((uint16_t)(b+0x30u))+ram32r((uint16_t)(b+0x20u)));ram32w((uint16_t)(b+0x34u),ram32r((uint16_t)(b+0x34u))+ram32r((uint16_t)(b+0x24u)));
 }else{(void)r44_record_move(b,0u);ram8w((uint16_t)(b+0x13u),0x60u);}
 o->x=ram16r((uint16_t)(b+0x30u));o->y=ram16r((uint16_t)(b+0x34u));o->facing=(uint8_t)ram16r((uint16_t)(b+6u));r31_actor_animate(i);
 if(o->type==0x334u&&ram8r((uint16_t)(b+0x10u))==9u&&ram8r((uint16_t)(b+0x11u))==1u)ram16w((uint16_t)(b+0x1Cu),0xFFFFu);
}
