/* ChkCharStatsUpdate / DoCharStatsUpdate. Source counters advance once per
 * completed walking tile; menus, cutscene waits and vehicles do not heal. */
__attribute__((noinline))
static void r30_field_status_restore(void){
    if(cram[0]!=0x000Eu)return;
    for(unsigned i=0u;i<64u;i++)field_palette_live_word(i,ram16r((uint16_t)(0xFB80u+i*2u)));
}
__attribute__((noinline))
static void r30_field_status_update(unsigned camx,unsigned camy){
    if(ram16r(0xF43Cu)||!field_step_completed||ram8r(0xEC80u)==1u||!(ram8r(0xECE5u)&2u))return;
    ram8w(0xECE5u,(uint8_t)(ram8r(0xECE5u)&~2u));
    uint8_t poison=(uint8_t)((ram8r(0xECE2u)+1u)&3u),para=(uint8_t)(ram8r(0xECE3u)+1u);
    ram8w(0xECE2u,poison);ram8w(0xECE3u,para);unsigned dead=0u,count=0u,flash=0u;
    for(unsigned slot=0u;slot<5u;slot++){
        unsigned id=ram8r((uint16_t)(0xF40Au+slot));if(id&0x80u)break;++count;
        uint16_t d=(uint16_t)(PS4_CHAR_BASE+id*0x80u);uint8_t status=ram8r((uint16_t)(d+0x16u));
        unsigned hp=ram16r((uint16_t)(d+0x0Eu));
        if(ram8r(0xECE6u)&&(status&1u)&&!poison){
            if(hp)--hp;
            ram16w((uint16_t)(d+0x0Eu),(uint16_t)hp);
            if(!hp){status=(uint8_t)((status&~3u)|4u);ram8w((uint16_t)(d+0x16u),status);
                ps4_field_popup_begin();unsigned x=24u;for(unsigned n=0u;n<8u;n++){uint8_t c=ram8r((uint16_t)(d+n));if(c>=0xFEu)break;dialogue_obj_window_source_glyph6(x,8u,c);x+=6u;}
                dialogue_obj_window_ascii_text6(x,8u," IS DEAD.");ps4_field_popup_wait();hw_field_init(camx,camy);
            }else flash=1u;
        }
        if((status&2u)&&para>5u&&(para>=25u||!(field_npc_rng_next()&7u)))status=(uint8_t)(status&~2u);
        ram8w((uint16_t)(d+0x16u),status);if(status&4u)++dead;
        /* Retail android regeneration does not clear an existing dead bit. */
        if((id==6u||id==7u)&&hp<ram16r((uint16_t)(d+0x10u)))ram16w((uint16_t)(d+0x0Eu),(uint16_t)(hp+1u));
    }
    if(count&&dead==count){ps4_field_popup_text("THE PARTY HAS PERISHED.",0);field_palette_fade_out();ram16w(0xEC20u,0u);ram16w(0xEF00u,0u);ps4_audio_stop();ps4_return_to_title=1u;return;}
    if(flash){field_palette_save(64u);for(unsigned i=0u;i<64u;i++)field_palette_live_word(i,0x000Eu);}
}
