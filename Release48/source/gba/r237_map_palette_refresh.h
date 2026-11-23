/* R237: source-level, palette-only replay of Genesis MapDataManager after
 * event-battle and same-map Garuberk RefreshMap.  Do NOT replay object, flag,
 * collision, scrolling or layout operations from field_apply_map_manager_load.
 * Included after field_reload_palette_from_map in title_runtime_v06.c. */
#ifndef PS4_R237_PALETTE_REFRESH_H
#define PS4_R237_PALETTE_REFRESH_H
static void r237_pal_copy(unsigned dst,uint32_t src,unsigned n){
    for(unsigned i=0u;i<n&&dst+i<64u;i++)cram[dst+i]=rom16(src+i*2u);
}
static void field_apply_map_manager_palette_only(unsigned id){
    switch(id){
    case 0x05u: if(ps4_event_flag_test(0x33u))r237_pal_copy(48u,0x133E20u,16u);break;
    case 0x0Du: case 0x0Fu: if(!ps4_event_flag_test(0x33u))r237_pal_copy(48u,0x12F570u,16u);break;
    case 0x13u: cram[10]=0x068Au;cram[11]=0x0468u;break;
    case 0x27u: cram[12]=0x0680u;cram[13]=0x0640u;cram[19]=0x068Au;cram[20]=0x0468u;cram[21]=0x0024u;break;
    case 0x28u: r237_pal_copy(17u,0x52608u,5u);break;
    case 0x29u:
        for(unsigned i=0u;i<16u;i++)cram[16u+i]=cram[i];
        r237_pal_copy(1u,0x52696u,9u);
        cram[13]=ps4_event_flag_test(0x40u)?0x0CA0u:0x0420u;break;
    case 0x2Au: r237_pal_copy(17u,0x526BEu,8u);break;
    case 0x2Bu: r237_pal_copy(17u,0x526E4u,13u);break;
    case 0x2Cu: r237_pal_copy(17u,0x52714u,13u);break;
    case 0x2Du: r237_pal_copy(17u,0x52744u,5u);break;
    case 0x2Eu: r237_pal_copy(17u,0x52764u,13u);break;
    case 0x2Fu: r237_pal_copy(17u,0x52794u,10u);break;
    case 0x30u: r237_pal_copy(18u,0x527BEu,6u);break;
    case 0x34u:{
        static const uint16_t p[14]={0x0000u,0x0EEEu,0x0000u,0x0226u,0x0448u,0x0AAEu,0x066Au,0x0EEEu,0x0040u,0x0262u,0x0484u,0x00AEu,0x04A8u,0x0EEEu};
        for(unsigned i=0u;i<14u;i++)cram[48u+i]=p[i];break;
    }
    case 0x37u: r237_pal_copy(17u,0x5289Au,5u);break;
    case 0x40u: r237_pal_copy(3u,0x52A42u,7u);cram[12]=0x0068u;cram[13]=0x0680u;break;
    case 0x41u: r237_pal_copy(17u,0x52A66u,9u);break;
    case 0x44u: if(ps4_event_flag_test(0x85u))cram[1]=0u;break;
    case 0x5Bu: if(!ps4_event_flag_test(0x2Fu))r237_pal_copy(49u,0x52DDEu,7u);break;
    case 0x6Cu: if(!ps4_event_flag_test(0x9Eu))r237_pal_copy(0u,field_map.palette+0x60u,32u);break;
    default: break;
    }
}
static void field_reload_palette_with_managers(void){
    field_reload_palette_from_map();
    const Ps4PscRuntimePlan* plan=ps4_psc_runtime_plan_get(field_map.map_id);
    if(plan){
        for(unsigned i=0u;i<plan->manager_count;i++)
            field_apply_map_manager_palette_only(ps4_psc_manager_ids[plan->manager_offset+i]);
    }else{
        uint32_t p=field_map.data_manager;
        for(unsigned g=0u;g<64u&&p&&p+1u<0x300000u;g++,p+=2u){
            unsigned id=rom16(p);if(id==0xFFFFu)break;
            field_apply_map_manager_palette_only(id);
        }
    }
    field_palette_mirror_live(64u);
}
#endif
