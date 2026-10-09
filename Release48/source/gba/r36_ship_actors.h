/* Original ship/planet controllers. All counters live in source object RAM. */
static int r36_ship_type(unsigned t){return (t>=0x1B8u&&t<=0x1D0u&&(t&3u)==0u)||t==0x200u||t==0x204u||t==0x208u||t==0x210u;}
static void r36_ship_init(unsigned i,unsigned* duration,unsigned* timer){
 struct FieldObjLite* o=&field_objs[i];uint16_t b=(uint16_t)(0xC300u+i*64u);unsigned t=o->type,face=0u;
 if(t==0x1BCu){face=4u;*duration=16u;*timer=0x7Fu;}
 else if(t==0x1C0u){face=8u;*duration=16u;*timer=0x9Fu;}
 else if(t==0x1C4u){face=12u;*duration=2u;*timer=0xDFu;}
 else if(t==0x1C8u){face=20u;*duration=4u;r31_actor_hidden(i,1u);}
 else if(t==0x1CCu)face=24u;
 else if(t==0x1D0u){face=28u;*timer=24u;}
 else if(t==0x204u){face=4u;*duration=3u;}
 else if(t==0x208u){face=16u;*duration=3u;}
 else if(t==0x210u){face=o->facing;*duration=1u;}
 o->facing=(uint8_t)face;ram16w((uint16_t)(b+6u),(uint16_t)face);
 if(t!=0x210u)ram8w((uint16_t)(b+2u),(uint8_t)(ram8r((uint16_t)(b+2u))|16u));
}
/* Return zero only for a beam that is hidden and does not animate. */
static int r36_ship_before(unsigned i){
 struct FieldObjLite* o=&field_objs[i];uint16_t b=(uint16_t)(0xC300u+i*64u);unsigned t=o->type,timer=ram16r((uint16_t)(b+0x1Cu));
 if(t==0x210u)ram32w((uint16_t)(b+0x20u),ram32r((uint16_t)(b+0x20u))+0xC0u);
 if(t==0x204u){timer=(timer+1u)&65535u;if(timer==0xDFu||timer==0xE3u){o->facing=(uint8_t)(o->facing+4u);ram16w((uint16_t)(b+6u),o->facing);ram8w((uint16_t)(b+0x11u),4u);ram8w((uint16_t)(b+0x10u),0u);}}
 else if(t==0x1BCu||t==0x1C0u){timer=(timer-1u)&65535u;if(!timer){timer=1u;ram8w((uint16_t)(b+0x11u),0x7Fu);}}
 else if(t==0x1C4u){timer=(timer-1u)&65535u;if(!timer){timer=0x7FFFu;o->facing=16u;ram16w((uint16_t)(b+6u),16u);ram8w((uint16_t)(b+0x11u),8u);ram8w((uint16_t)(b+0x10u),0u);}if(ram8r((uint16_t)(b+0x10u))==6u&&ram8r((uint16_t)(b+0x11u))==1u)r31_actor_hidden(i,1u);}
 else if(t==0x1C8u){timer=(timer+1u)&65535u;if(timer==0xE3u)r31_actor_hidden(i,0u);if(ram8r((uint16_t)(b+0x10u))==8u&&ram8r((uint16_t)(b+0x11u))==1u)r31_actor_hidden(i,1u);}
 else if(t==0x1D0u){timer=(timer-1u)&65535u;if(!timer){timer=24u;r31_actor_hidden(i,!(ram8r((uint16_t)(b+2u))&2u));}}
 ram16w((uint16_t)(b+0x1Cu),(uint16_t)timer);
 return t!=0x1C8u||!(ram8r((uint16_t)(b+2u))&2u);
}
