/* Native source controllers $4D9EA..$4DBFC. Original 64-byte object state. */
__attribute__((noinline))
static void r33_chest_actor_update(unsigned i){
    struct FieldObjLite* o=&field_objs[i];uint16_t b=(uint16_t)(0xC300u+i*64u);
    unsigned flags=ram8r((uint16_t)(b+2u));
    if(o->type==0x1F8u){
        if(!o->facing){
            flags^=2u;unsigned y=ram16r(0xC034u);
            if(y<=0x170u||y>0x1D0u)flags|=2u;
        }else{
            unsigned timer=(uint16_t)(ram16r((uint16_t)(b+0x1Cu))-1u);
            ram16w((uint16_t)(b+0x1Cu),(uint16_t)timer);
            if(!timer){r31_actor_clear(i);return;}
            flags=(flags&~2u)|((timer&2u)?2u:0u);
        }
    }else if(o->type==0x1FCu){
        unsigned timer=(uint16_t)(ram16r((uint16_t)(b+0x1Cu))-1u);
        ram16w((uint16_t)(b+0x1Cu),(uint16_t)timer);
        if(!timer){r31_actor_clear(i);return;}
        flags=(flags&~2u)|((timer&2u)?2u:0u);
    }else if(!o->facing){
        unsigned timer=(uint8_t)(ram8r((uint16_t)(b+0x1Cu))-1u);
        ram8w((uint16_t)(b+0x1Cu),(uint8_t)timer);
        if(!timer){
            flags^=2u;unsigned phase=(uint8_t)(ram8r((uint16_t)(b+0x1Du))+1u);
            ram8w((uint16_t)(b+0x1Du),(uint8_t)phase);
            timer=rom8(0x4DBE6u+phase);ram8w((uint16_t)(b+0x1Cu),(uint8_t)timer);
            if(timer&0x80u){o->facing=4u;ram16w((uint16_t)(b+6u),4u);ram8w((uint16_t)(b+0x10u),0u);ram8w((uint16_t)(b+0x11u),3u);flags&=~6u;}
        }
    }else if(o->facing!=12u&&(flags&4u)){
        o->facing=(uint8_t)(o->facing+4u);ram16w((uint16_t)(b+6u),o->facing);
        ram8w((uint16_t)(b+0x11u),5u);flags&=~6u;
        if(o->facing==8u)ram8w(0x500Au,0xC2u); /* SFXID_FireBreath */
    }
    ram8w((uint16_t)(b+2u),(uint8_t)flags);
    ram32w((uint16_t)(b+0x30u),ram32r((uint16_t)(b+0x30u))+ram32r((uint16_t)(b+0x20u)));
    ram32w((uint16_t)(b+0x34u),ram32r((uint16_t)(b+0x34u))+ram32r((uint16_t)(b+0x24u)));
    o->x=ram16r((uint16_t)(b+0x30u));o->y=ram16r((uint16_t)(b+0x34u));r31_actor_animate(i);
}
