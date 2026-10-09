/* The original snow bank belongs only to Dezolis; Motavia's earlier
 * place effects retain their existing native path and accepted art. */
static int r37_place_type(unsigned t){return ram16r(0xEC28u)==1u&&(t==0x78u||t==0x7Cu||t==0x84u);}
static int r36_ship_type(unsigned t);
static void r36_ship_init(unsigned,unsigned*,unsigned*);
static int r36_ship_before(unsigned);
/* Source actors retain their original $C300+$40*slot state; no new EWRAM. */
#include "r31_field_poses.h"
__attribute__((noinline))
static int r41_torch_actor_type(unsigned t);
static int r31_actor_type(uint16_t t){return r41_torch_actor_type(t)||r37_place_type(t)||r36_ship_type(t)||t==0x24u||t==0x1F4u||t==0xCu||t==0x150u||t==0x214u||t==0x218u||t==0x21Cu||t==0x220u||t==0x1A4u||t==0x1A8u||t==0x1ACu||t==0x1B0u||t==0x1F8u||t==0x1FCu||t==0x20Cu||t==0x1A0u||t==0x8194u||t==0x298u||t==0x374u||t==0x334u;}
static uint32_t r31_actor_sequence(const struct FieldObjLite* o){
    uint32_t table=o->type==0x1F8u?0x4DEA4u:(o->type==0x1FCu?0x4DEACu:(o->type==0x20Cu?0x4DEF4u:(o->type==0x150u?0x4DE40u:(o->type>=0x214u?0x4DF04u:0x4DE4Cu))));
    if(o->type==0x24u)table=0x47EC4u;else if(o->type==0x1F4u)table=0x4DE98u;else if(o->type==0xCu)table=0x47E64u;else if(o->type==0x1A0u)table=0x4DE48u;else if(o->type==0x8194u)table=0x4A504u;else if(o->type==0x298u)table=0x5028Eu;
    if(r36_ship_type(o->type))table=o->type==0x210u?0x514D0u:((o->type>=0x200u)?0x4DE80u:0x4DE60u);
    if(o->type==0x374u)table=0x502A2u;
    if(o->type==0x334u)table=0x4C39Cu;
    if(o->type==0x248u)table=0x4A504u;
    if(r37_place_type(o->type))table=0x4DE24u;
    if(r41_torch_actor_type(o->type))table=o->type==0x338u?0x502C2u:0x502F6u;
    return rom32(table+o->facing);
}
static const struct R31FieldPose* r31_actor_pose(const struct FieldObjLite* o){
    uint16_t t=o->type==0x1B0u?0x1ACu:(o->type==0x294u?0x284u:(o->type==0x284u&&o->facing==0u?0x280u:o->type));
    unsigned face=o->facing+(o->type==0x8194u?(o->base_tile==0x327u?16u:(o->base_tile==0x326u?32u:(o->base_tile==0x42Bu?48u:(o->base_tile==0x3E3u?64u:(o->base_tile==0x39Bu?80u:0u))))):0u);
    for(unsigned i=0u;i<sizeof(r31_field_poses)/sizeof(r31_field_poses[0]);i++){const struct R31FieldPose* p=&r31_field_poses[i];if(p->type==t&&p->facing==face&&p->frame==o->anim)return p;}
    return 0;
}
static void r31_actor_init(unsigned i){
    struct FieldObjLite* o=&field_objs[i];uint16_t b=(uint16_t)(0xC300u+i*64u);
    for(unsigned q=0u;q<64u;q++)ram8w((uint16_t)(b+q),0u);
    ram16w(b,o->type);ram16w((uint16_t)(b+6u),o->facing);ram16w((uint16_t)(b+0x16u),o->base_tile);
    ram32w((uint16_t)(b+0x30u),(uint32_t)o->x<<16);ram32w((uint16_t)(b+0x34u),(uint32_t)o->y<<16);
    o->anim=0u;o->move_kind=0u;o->render_w=o->render_h=0u;
    unsigned duration=0u,timer=0u;
    if(r36_ship_type(o->type))r36_ship_init(i,&duration,&timer);
    if(r37_place_type(o->type)){duration=o->type==0x78u?15u:(o->type==0x7Cu?8u:24u);timer=o->type==0x78u?59u:(o->type==0x7Cu?31u:188u);ram8w((uint16_t)(b+2u),(uint8_t)(o->type==0x7Cu?0x50u:0x40u));}
    if(o->type==0x214u){duration=0x3Cu;timer=0x49u;}
    else if(o->type==0x218u){duration=5u;timer=0x32u;}
    else if(o->type==0x21Cu){duration=5u;timer=0x46u;}
    else if(o->type==0x220u){duration=5u;timer=0x13u;}
    else if(o->type==0x1A4u)timer=0xCDu;
    else if(o->type==0x1F8u)timer=0xBu;
    else if(o->type==0x1FCu){timer=0x27u;o->anim=0xAu;ram8w((uint16_t)(b+0x10u),0xAu);}
    else if(o->type==0x20Cu){timer=0x1400u;o->anim=0xAu;ram8w((uint16_t)(b+0x10u),0xAu);ram8w((uint16_t)(b+2u),2u);}
    else if(o->type==0x1A0u){duration=5u;timer=0x23u;}
    else if(o->type==0x1F4u){duration=8u;timer=3u;ram8w((uint16_t)(b+2u),16u);}
    else if(o->type==0x24u||o->type==0xCu||o->type==0x8194u){duration=o->type==0x24u?12u:(o->type==0xCu?7u:4u);ram8w((uint16_t)(b+2u),8u);ram16w((uint16_t)(b+0x38u),o->x);ram16w((uint16_t)(b+0x3Au),o->y);}
    else if(o->type==0x298u)ram8w((uint16_t)(b+2u),8u);
    ram8w((uint16_t)(b+0x11u),(uint8_t)duration);ram16w((uint16_t)(b+0x1Cu),(uint16_t)timer);
    field_obj_art_stamp[i].epoch=0u;
}
static int r31_actor_spawn(int requested,uint16_t type,uint16_t x,uint16_t y,uint8_t facing,uint16_t base){
    unsigned i=requested<0?0u:(unsigned)requested;
    if(requested<0){while(i<field_obj_count&&field_objs[i].type)i++;}
    if(i>=FIELD_MAX_OBJECTS)return -1;
    while(field_obj_count<=i){struct FieldObjLite* z=&field_objs[field_obj_count++];z->type=0u;z->active=0u;z->render_w=z->render_h=0u;}
    struct FieldObjLite* o=&field_objs[i];o->type=type;o->active=1u;o->x=x;o->y=y;o->facing=facing;o->base_tile=base;o->blocks=0u;o->dialog=0u;o->gba_tile=0u;
    field_obj_init_motion(o);r31_actor_init(i);return (int)i;
}
static void r31_actor_clear(unsigned i){
    if(i>=field_obj_count)return;field_objs[i].active=0u;field_objs[i].type=0u;ram16w((uint16_t)(0xC300u+i*64u),0u);
    for(unsigned k=0u;k<field_obj_count;k++)if(field_objs[k].active&&r31_actor_type(field_objs[k].type))return;
    for(unsigned oi=44u;oi<108u;oi++)hw_hide_obj(oi);
}
static void r31_actor_hidden(unsigned i,unsigned hide){
    if(i>=field_obj_count)return;uint16_t b=(uint16_t)(0xC302u+i*64u);uint8_t q=ram8r(b);ram8w(b,(uint8_t)(hide?q|2u:q&~2u));field_obj_art_stamp[i].epoch=0u;
}
static void r31_actor_animate(unsigned i){
    struct FieldObjLite* o=&field_objs[i];uint16_t b=(uint16_t)(0xC300u+i*64u);uint32_t seq=r31_actor_sequence(o);unsigned n=rom8(seq)&127u;
    uint8_t duration=(uint8_t)(ram8r((uint16_t)(b+0x11u))-1u),frame=ram8r((uint16_t)(b+0x10u));
    if(duration&0x80u){frame=(uint8_t)(frame+1u);if(frame>=n){frame=0u;ram8w((uint16_t)(b+2u),(uint8_t)(ram8r((uint16_t)(b+2u))|4u));}duration=(uint8_t)rom8(seq+1u+((rom8(seq)&0x80u)?frame:0u));}
    ram8w((uint16_t)(b+0x10u),frame);ram8w((uint16_t)(b+0x11u),duration);o->anim=frame;
}
#include "r33_chest_actors.h"
#include "r36_ship_actors.h"
#include "r41_torch_actors.h"
static void r31_actor_update_one(unsigned i){
    struct FieldObjLite* o=&field_objs[i];uint16_t b=(uint16_t)(0xC300u+i*64u);unsigned t=o->type,timer=ram16r((uint16_t)(b+0x1Cu));
#ifdef PS4_R44_SOURCE_EVENTS
    if(t==0x334u||r44_monsen_actor(o)){r44_event_actor_update(i);return;}
#endif
    if(r41_torch_actor_type(t)){r41_torch_actor_update(i);return;}
    if(r37_place_type(t)){timer=(uint16_t)(timer-1u);ram16w((uint16_t)(b+0x1Cu),(uint16_t)timer);if(!timer)ram16w(b,0u);}
    unsigned animate=1u;if(r36_ship_type(t))animate=(unsigned)r36_ship_before(i);
    if(t==0x1F8u||t==0x1FCu||t==0x20Cu){r33_chest_actor_update(i);return;}
    /* FieldObj_Snow tests the old frame/duration before position/animation. */
    if(t==0x1F4u&&ram8r((uint16_t)(b+0x10u))==4u&&ram8r((uint16_t)(b+0x11u))==1u){
        timer=(uint16_t)(timer-1u);ram16w((uint16_t)(b+0x1Cu),(uint16_t)timer);
        if(!timer)ram16w(b,0u);
        else {ram8w(0x500Au,0xBEu);ram16w((uint16_t)(b+0x30u),(uint16_t)(ram16r((uint16_t)(b+0x30u))+0x40u));ram16w((uint16_t)(b+0x34u),0x180u);}
    }
    if(t>=0x214u&&t<=0x220u){
        if(timer)timer--;ram16w((uint16_t)(b+0x1Cu),(uint16_t)timer);
        if(!timer){if(t==0x214u)ram16w((uint16_t)(b+0x1Cu),1u);else r31_actor_clear(i);return;}
        if(t==0x214u&&timer==3u){ram8w(0x500Au,0xBBu);r31_actor_spawn(1,0x218u,0x1F0u,0x240u,4u,0x191u);}
        if(t==0x218u&&timer==0x17u)ram8w(0xECF2u,1u);
        if(t==0x21Cu){ram32w((uint16_t)(b+0x24u),ram32r((uint16_t)(b+0x24u))+0x800u);if(timer<0x14u)r31_actor_hidden(i,!(ram8r((uint16_t)(b+2u))&2u));}
    }
    if(t==0x8194u&&o->base_tile==0x327u){
        /* Copied clinic Esper uses FieldObj_Move's original 16-pixel
         * step durations. Preserve the copied animation/palette state. */
        if(!ram16r((uint16_t)(b+0x28u))&&!ram16r((uint16_t)(b+0x2Au))){
            unsigned speed=1u<<(ram8r(0xECE0u)&3u);int dx=(int)ram16r((uint16_t)(b+0x38u))-(int)o->x,dy=(int)ram16r((uint16_t)(b+0x3Au))-(int)o->y;
            if((ram8r(0xECFEu)&2u)&&dy)dx=0;
            int vx=0,vy=0;if(dx){vx=dx>0?(int)speed:-(int)speed;o->facing=(uint8_t)(dx>0?8u:12u);ram16w((uint16_t)(b+0x28u),0x1000u);}
            else if(dy){vy=dy>0?(int)speed:-(int)speed;o->facing=(uint8_t)(dy>0?0u:4u);ram16w((uint16_t)(b+0x2Au),0x1000u);}
            else{ram8w((uint16_t)(b+0x10u),0u);ram8w((uint16_t)(b+0x11u),1u);}
            ram32w((uint16_t)(b+0x20u),(uint32_t)((int32_t)vx*65536));ram32w((uint16_t)(b+0x24u),(uint32_t)((int32_t)vy*65536));
            ram16w((uint16_t)(b+6u),o->facing);ram8w((uint16_t)(b+2u),(uint8_t)(ram8r((uint16_t)(b+2u))|64u));
        }
        for(unsigned axis=0u;axis<2u;axis++){
            int32_t v=(int32_t)ram32r((uint16_t)(b+0x20u+axis*4u));unsigned n=(unsigned)(v<0?-v:v)>>8;
            if(n)ram16w((uint16_t)(b+0x28u+axis*2u),(uint16_t)(ram16r((uint16_t)(b+0x28u+axis*2u))-n));
        }
    }else if(t==0x24u||t==0xCu||t==0x8194u){
        unsigned speed=1u<<(ram8r(0xECE0u)&3u);int dx=(int)ram16r((uint16_t)(b+0x38u))-(int)o->x,dy=(int)ram16r((uint16_t)(b+0x3Au))-(int)o->y;
        if((ram8r(0xECFEu)&2u)&&dy)dx=0;
        if(dx){if(dx>(int)speed)dx=(int)speed;if(dx<-(int)speed)dx=-(int)speed;dy=0;o->facing=(uint8_t)(dx>0?8u:12u);}
        else if(dy){if(dy>(int)speed)dy=(int)speed;if(dy<-(int)speed)dy=-(int)speed;o->facing=(uint8_t)(dy>0?0u:4u);}
        else {ram8w((uint16_t)(b+0x10u),0u);ram8w((uint16_t)(b+0x11u),1u);}
        ram16w((uint16_t)(b+6u),o->facing);ram32w((uint16_t)(b+0x20u),(uint32_t)((int32_t)dx*65536));ram32w((uint16_t)(b+0x24u),(uint32_t)((int32_t)dy*65536));
    }
    ram32w((uint16_t)(b+0x30u),ram32r((uint16_t)(b+0x30u))+ram32r((uint16_t)(b+0x20u)));
    ram32w((uint16_t)(b+0x34u),ram32r((uint16_t)(b+0x34u))+ram32r((uint16_t)(b+0x24u)));
    o->x=ram16r((uint16_t)(b+0x30u));o->y=ram16r((uint16_t)(b+0x34u));if(animate)r31_actor_animate(i);
    if(r37_place_type(t)&&!timer)r31_actor_clear(i);
    if(t==0x1F4u&&!timer)r31_actor_clear(i);
    if(t==0x150u){if(o->anim==4u){ram32w((uint16_t)(b+0x20u),0u);ram32w((uint16_t)(b+0x24u),0u);}timer=(uint16_t)(timer-1u);ram16w((uint16_t)(b+0x1Cu),(uint16_t)timer);if(!timer)r31_actor_clear(i);}
    if(t==0x1A0u){timer=(uint16_t)(timer-1u);ram16w((uint16_t)(b+0x1Cu),(uint16_t)timer);if(!timer)r31_actor_clear(i);}
}
__attribute__((noinline))
static void r31_actor_update(void){for(unsigned i=0u;i<field_obj_count;i++)if(field_objs[i].active&&r31_actor_type(field_objs[i].type))r31_actor_update_one(i);}
static void r31_actor_load(void){
    for(unsigned i=0u;i<5u;i++)ram8w((uint16_t)(0xC002u+i*64u),0u);
    for(unsigned i=0u;i<field_obj_count;i++){ram8w((uint16_t)(0xC302u+i*64u),0u);if(r31_actor_type(field_objs[i].type))r31_actor_init(i);}
}
static void r31_obj_piece(unsigned oi,int x,int y,unsigned w,unsigned h,unsigned tile,unsigned palette){
    static const uint8_t dims[12][2]={{8,8},{16,16},{32,32},{64,64},{16,8},{32,8},{32,16},{64,32},{8,16},{8,32},{16,32},{32,64}};
    unsigned k;for(k=0u;k<12u;k++)if(dims[k][0]==w&&dims[k][1]==h)break;
    if(k==12u||x<=-(int)w||x>=240||y<=-(int)h||y>=160){hw_hide_obj(oi);return;}
    OAM16[oi*4u]=(uint16_t)((y&255)|((k/4u)<<14));OAM16[oi*4u+1u]=(uint16_t)((x&511)|((k%4u)<<14));OAM16[oi*4u+2u]=(uint16_t)(tile|0x400u|(palette<<12));OAM16[oi*4u+3u]=0u;
}
__attribute__((noinline))
static void r31_actor_draw(unsigned camx,unsigned camy){
    unsigned present=0u;for(unsigned i=0u;i<field_obj_count;i++)if(field_objs[i].active&&r31_actor_type(field_objs[i].type)){present=1u;break;}if(!present)return;
    unsigned next=FIELD_OBJ_TILE_BASE+8u,oi=44u;const struct R31FieldPose* cached[FIELD_MAX_OBJECTS];uint16_t tile[FIELD_MAX_OBJECTS];unsigned used=0u;
    for(unsigned i=44u;i<108u;i++)hw_hide_obj(i);
    for(unsigned i=0u;i<field_obj_count;i++){const struct FieldObjLite* o=&field_objs[i];if(r31_actor_type(o->type))continue;unsigned end=o->gba_tile+(o->render_w/8u)*(o->render_h/8u);if(end>next&&end<=DIALOGUE_OBJ_TILE_BASE)next=end;}
    for(unsigned i=0u;i<field_obj_count;i++){
        struct FieldObjLite* o=&field_objs[i];if(!o->active||!r31_actor_type(o->type)||ram8r((uint16_t)(0xC302u+i*64u))&2u)continue;
        const struct R31FieldPose* p=r31_actor_pose(o);if(!p||p->w>64u)continue;
        /* Off-screen effects still simulate, but cannot consume visible OBJ
         * tiles before a later on-screen snow actor is allocated. */
        int x=(int)o->x-(int)camx+p->x,y=(int)o->y-(int)camy+p->y;
        if(r37_place_type(o->type)){x=(x+0x800)&0xFFF;x-=0x800;y=(y+0x800)&0xFFF;y-=0x800;}
        if(x<=-(int)p->w||x>=240||y<=-(int)p->h||y>=160)continue;
        unsigned n=p->w*p->h/64u,j;
        for(j=0u;j<used;j++)if(cached[j]->offset==p->offset)break;
        unsigned shared=j<used,dst;if(shared)dst=tile[j];else{if(next+n>DIALOGUE_OBJ_TILE_BASE)continue;dst=next;next+=n;cached[used]=p;tile[used++]=(uint16_t)dst;}
        uint32_t key=dst|(n<<16);struct FieldObjArtStamp* st=&field_obj_art_stamp[i];
        if(!shared&&(st->epoch!=field_obj_art_epoch||st->key0!=p->offset||st->key1!=key)){
            /* Reallocation can overwrite an absent actor's old destination.
             * Invalidate every overlapping pose stamp before reusing its tiles. */
            for(unsigned k=0u;k<field_obj_count;k++)if(r31_actor_type(field_objs[k].type)){
                struct FieldObjArtStamp* old=&field_obj_art_stamp[k];unsigned a=old->key1&65535u,len=old->key1>>16;
                if(old->epoch==field_obj_art_epoch&&old->key0!=p->offset&&a<dst+n&&dst<a+len)old->epoch=0u;
            }
            dma_copy32(r31_field_pose_data+p->offset,(void*)&OBJ_VRAM16[dst*16u],n*32u);
        }
        st->key0=p->offset;st->key1=key;st->epoch=field_obj_art_epoch;
        unsigned palette=r41_torch_actor_type(o->type)?(unsigned)(ram8r((uint16_t)(0xC313u+i*64u))>>5):r37_place_type(o->type)?0u:r36_ship_type(o->type)?(o->type==0x210u?0u:2u):(o->type==0x1F4u||o->type==0x150u||o->type==0x21Cu||o->type==0x220u)?0u:((o->type==0x24u||o->type==0x214u||o->type==0x1A4u||o->type==0x1A8u||o->type==0xCu||o->type==0x8194u||o->type==0x298u||o->type==0x374u)?2u:3u);
#ifdef PS4_R44_SOURCE_EVENTS
        if(r44_monsen_actor(o))palette=3u;else if(o->type==0x334u)palette=2u;
#endif
        unsigned h=p->h>64u?64u:p->h;if(oi<108u){r31_obj_piece(oi,x,y,p->w,h,dst,palette);if(r37_place_type(o->type)&&(ram8r((uint16_t)(0xC302u+i*64u))&16u))OAM16[oi*4u+2u]&=(uint16_t)~0xC00u;oi++;}
        if(p->h>64u&&oi<108u)r31_obj_piece(oi++,x,y+64,p->w,64u,dst+p->w*8u,palette);
    }
}
