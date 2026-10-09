/* Source FieldObj_XeAThoulAppearing..Moving and loc_4FAE0 (tree ash).
 * Original 64-byte object records retain initialization, timers and 16.16
 * movement. These actors never allocate additional EWRAM. */
static int r41_torch_actor_type(unsigned t){return (t>=0x280u&&t<=0x294u&&(t&3u)==0u)||t==0x338u;}
static void r41_torch_actor_update(unsigned i){
    struct FieldObjLite* o=&field_objs[i];uint16_t b=(uint16_t)(0xC300u+i*64u);unsigned t=o->type;
    if(!(ram16r(b)&0x8000u)){
        ram16w(b,(uint16_t)(t|0x8000u));ram32w((uint16_t)(b+8u),t==0x338u?0x502C2u:0x502F6u);
        ram32w((uint16_t)(b+12u),t==0x338u?0x5132Au:(t==0x280u?0x5091Eu:((t==0x28Cu||t==0x290u)?0x50892u:0x50966u)));
        ram8w((uint16_t)(b+2u),(uint8_t)(ram8r((uint16_t)(b+2u))&~8u));
        ram8w((uint16_t)(b+0x13u),(uint8_t)(t==0x338u?0u:((t==0x28Cu||t==0x290u)?0x40u:0x60u)));
        if(t!=0x338u){
            o->facing=(uint8_t)(t==0x280u?0u:(t==0x288u?12u:(t==0x28Cu?4u:(t==0x290u?16u:8u))));
            ram16w((uint16_t)(b+6u),o->facing);ram8w((uint16_t)(b+0x10u),0u);
            ram8w((uint16_t)(b+0x11u),(uint8_t)((t==0x288u||t==0x28Cu||t==0x290u)?4u:6u));
            if(t==0x280u||t==0x288u||t==0x28Cu||t==0x290u)
                ram16w((uint16_t)(b+0x1Cu),(uint16_t)(t==0x280u?0x20u:(t==0x288u?0x1Fu:(t==0x28Cu?0x61u:0x55u))));
        }
    }
    if(t==0x280u||t==0x288u){
        r31_actor_hidden(i,0u);uint16_t timer=(uint16_t)(ram16r((uint16_t)(b+0x1Cu))-1u);
        ram16w((uint16_t)(b+0x1Cu),timer);
        if(timer){if(t==0x280u)r31_actor_hidden(i,1u);return;}
        ram16w((uint16_t)(b+0x1Cu),1u);
        if(ram8r((uint16_t)(b+0x10u))==6u&&ram8r((uint16_t)(b+0x11u))==1u){
            if(t==0x288u){r31_actor_clear(i);return;}
            o->type=0x284u;ram16w(b,0x284u);
        }
        r31_actor_animate(i);return;
    }
    if(t==0x28Cu||t==0x290u){
        uint16_t timer=(uint16_t)(ram16r((uint16_t)(b+0x1Cu))-1u);ram16w((uint16_t)(b+0x1Cu),timer);
        if(!timer){r31_actor_clear(i);return;}
    }
    if(t==0x294u){
        r31_actor_hidden(i,!(ram8r((uint16_t)(b+2u))&2u));
        if(ram16r((uint16_t)(b+0x38u))==ram16r((uint16_t)(b+0x30u))){r31_actor_clear(i);return;}
    }
    if(t==0x338u&&ram8r((uint16_t)(b+0x10u))==3u&&!ram8r((uint16_t)(b+0x11u))){r31_actor_clear(i);return;}
    ram32w((uint16_t)(b+0x30u),ram32r((uint16_t)(b+0x30u))+ram32r((uint16_t)(b+0x20u)));
    ram32w((uint16_t)(b+0x34u),ram32r((uint16_t)(b+0x34u))+ram32r((uint16_t)(b+0x24u)));
    o->x=ram16r((uint16_t)(b+0x30u));o->y=ram16r((uint16_t)(b+0x34u));r31_actor_animate(i);
    if(t==0x284u&&ram16r((uint16_t)(b+0x38u))==o->x)ram32w((uint16_t)(b+0x20u),0u);
}
