/* Direct lowering of EnemyAttack_Zio3 and its four original root objects.
 * Each call is ONE enemy turn. Source stage 2 is a no-op; stage 4 returns to
 * field mode without Battle_DoDamage, enemy death, or Battle_CheckResults. */
static int r46_zio_frame_at(unsigned anim,unsigned age,unsigned loops){
    const Ps4SourceEnemyAnim* a=&r46_zio_anims[anim];unsigned duration=0u;
    for(unsigned i=0u;i<a->frame_count;i++)duration+=r46_zio_bank[a->holds_off+i];
    if(age>=duration*loops)return -1;while(age>=duration)age-=duration;
    for(unsigned i=0u;i<a->frame_count;i++){unsigned h=r46_zio_bank[a->holds_off+i];if(age<h)return (int)i;age-=h;}return -1;
}
static unsigned r46_zio_draw(unsigned anim,int frame,int x,int y,unsigned oi,unsigned tile){
    if(frame<0)return oi;const Ps4SourceEnemyAnim* a=&r46_zio_anims[anim];
    for(unsigned i=0u;i<a->part_count;i++){
        const Ps4SourceEnemyPart* p=&r46_zio_parts[a->first_part+i];
        (void)battle_effect_upload(r46_zio_bank+p->off+(unsigned)frame*p->frame_bytes,p->frame_bytes,tile);
        battle_source_obj_prebound4(oi,x,y,p->ox,p->oy,p->geom,tile);
        /* Source Black Wave adds mapping palette bits to root $20: its small
         * ball uses line 1 and the large wave uses line 2. */
        unsigned bank=p->reserved==1u?14u:15u;OAM16[oi*4u+2u]=(uint16_t)((OAM16[oi*4u+2u]&0x0FFFu)|(bank<<12));
        tile+=p->frame_bytes>>5;oi++;
    }return oi;
}
static void r46_zio_effect_palettes(void){
    for(unsigned j=0u;j<16u;j++){OBJ_PALETTE[14u*16u+j]=md2gba(ram16r((uint16_t)(0xFB20u+j*2u)));OBJ_PALETTE[15u*16u+j]=md2gba(ram16r((uint16_t)(0xFB40u+j*2u)));}
}
static void r46_zio_present_objects(unsigned root_anim,int root_frame,unsigned child_anim,int child_frame,int x,unsigned source_y){
    unsigned oi=64u;int y=battle_source_y_to_gba(source_y);
    oi=r46_zio_draw(root_anim,root_frame,x,y,oi,512u);
    if(child_frame>=0)oi=r46_zio_draw(child_anim,child_frame,x,y,oi,640u);
    for(unsigned i=oi;i<112u;i++)hw_hide_obj(i);battle_source_effect_active_oam=oi-64u;r46_zio_effect_palettes();
}
static void r46_zio_wait(struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES]){waitvb();battle_action_idle_after_wait(enemies);}
static void r46_zio_restore_plane(struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned ei){
    ram8w(0xD014u,0u);battle_generic_pending_mask|=1u<<ei;
}
static void r46_zio_fade_line1(unsigned mode){
    for(unsigned j=1u;j<=13u;j++){
        uint16_t v=ram16r((uint16_t)(0xFB20u+j*2u)),target=ram16r((uint16_t)(0xFBA0u+j*2u));
        for(unsigned shift=0u;shift<=8u;shift+=4u){unsigned a=(v>>shift)&15u,b=(target>>shift)&15u;if(a!=b){unsigned next=(a+(mode?2u:14u))&15u;v=(uint16_t)((v&~(15u<<shift))|(next<<shift));}}
        ram16w((uint16_t)(0xFB20u+j*2u),v);
    }
}
static int r46_zio_story_action(struct BattleLiteActor party[5],unsigned pn,struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned ei){
    if(ei>=battle_enemy_count||enemies[ei].id!=0x98u)return 0;
    unsigned stage=ram8r(0xEE98u);ram16w(0x4144u,0u);
    if(stage>=5u)return 1;
    ram8w(0xEE98u,(uint8_t)(stage+1u));ram8w(0xD01Fu,(uint8_t)ei);ram8w(0xD011u,0u);ram8w(0xD014u,0u);
    if(stage==2u)return 1; /* loc_D264: finish turn; no attack, cue or damage */
    unsigned ability=stage==0u?0x6Bu:(stage==3u?0x53u:(stage==4u?0x70u:0u));
    unsigned victim=0u;if(stage==4u){for(unsigned i=0u;i<pn&&i<5u;i++)if(party[i].id==1u){victim=i;break;}ram16w(0x4144u,(uint16_t)(victim*2u));}
    struct BattleCommandLite c={3u,(uint8_t)ability,(uint8_t)victim,0xFFu};
    ram16w(0xD000u,(uint16_t)(stage==0u?0x908u:(stage==1u?0x90Cu:(stage==3u?0x910u:0x914u))));
    if(stage==0u){
        for(unsigned j=0u;j<48u;j++){uint16_t q=rom16(0x3438Au+j*2u);ram16w((uint16_t)(0xFB00u+j*2u),q);ram16w((uint16_t)(0xFB80u+j*2u),q);}
        for(unsigned line=0u;line<3u;line++)for(unsigned j=0u;j<14u;j++)ram16w((uint16_t)(0xFB00u+line*32u+j*2u),0xFFFu);
    }else if(stage==3u){for(unsigned j=0u;j<16u;j++)ram16w((uint16_t)(0xFBA0u+j*2u),rom16(0x3400Au+j*2u));}
    else if(stage==4u){ram16w(0xFB54u,0xAu);ram16w(0xFB56u,4u);ram16w(0xFB5Au,0u);}
    battle_present_action(party,pn,enemies,ei,1u,ei,&c,0u);r46_zio_palette_sync();
    if(stage==0u){
        /* loc_2540E: fifteen authored VInts, each nibble moves one step from
         * white to the palette installed by loc_34352. No frame skipping. */
        for(unsigned f=0u;f<15u;f++){
            for(unsigned j=0u;j<48u;j++){uint16_t v=ram16r((uint16_t)(0xFB00u+j*2u)),q=ram16r((uint16_t)(0xFB80u+j*2u));for(unsigned shift=0u;shift<=8u;shift+=4u)if(((v>>shift)&15u)!=((q>>shift)&15u))v=(uint16_t)(v-(1u<<shift));ram16w((uint16_t)(0xFB00u+j*2u),v);}
            r46_zio_wait(enemies);r46_zio_palette_sync();
        }
        ram16w(0x2A90u,0x2Cu);ram8w(0x2A92u,0u);ram8w(0x2A93u,0u);
    }
    for(unsigned t=0u;t<10u;t++)r46_zio_wait(enemies); /* loc_34176 */
    int x=battle_enemy_source_center_x(ei);
    if(stage==0u){
        ram8w(0xD014u,1u);battle_generic_pending_mask|=1u<<ei;
        for(unsigned t=0u;t<100u;t++){
            if(t==3u)ram8w(0x500Au,0xC4u);if(t==37u)ram8w(0x500Au,0xCDu);
            if(t==98u)r46_zio_restore_plane(enemies,ei);
            r46_zio_wait(enemies);int child=-1;unsigned anim=1u;
            if(t>=3u&&t<27u)child=r46_zio_frame_at(1u,t-3u,1u);
            if(t>=37u){anim=2u;child=r46_zio_frame_at(2u,t-37u,2u);}
            r46_zio_present_objects(0u,r46_zio_frame_at(0u,t,1u),anim,child,x,0xF8u);
        }
    }else if(stage==1u){
        ram8w(0xD014u,1u);battle_generic_pending_mask|=1u<<ei;
        for(unsigned t=0u;t<19u;t++){if(t==18u){ram8w(0x500Au,0xC2u);r46_zio_restore_plane(enemies,ei);}r46_zio_wait(enemies);r46_zio_present_objects(3u,r46_zio_frame_at(3u,t,1u),0u,-1,x,0xF8u);}
    }else if(stage==3u){
        ram8w(0xD014u,1u);battle_generic_pending_mask|=1u<<ei;
        for(unsigned counter=1u;counter<=88u;counter++){
            ram8w(0xD011u,(uint8_t)counter);
            if(counter==1u)ram8w(0x500Au,0xC6u);if(counter==40u)ram8w(0x500Au,0xCBu);
            if(counter==53u)r46_zio_restore_plane(enemies,ei);
            if(counter>=46u&&counter<=82u&&(counter-46u)%6u==0u)r46_zio_fade_line1(1u);
            r46_zio_wait(enemies);r46_zio_palette_sync();
            /* $22 runs before roots, so its first update is counter 83. */
            if(counter==82u){ram16w(0x2AA0u,0x22u);ram8w(0x2AA2u,0u);ram8w(0x2AA3u,0u);}
            int root=counter<49u?0:(counter<53u?1:-1);
            r46_zio_present_objects(4u,root,5u,r46_zio_frame_at(5u,counter-1u,1u),x,0xF8u);
        }
    }else{
        for(unsigned counter=1u;counter<=18u;counter++){
            if(counter==1u||counter==5u){
                for(unsigned j=1u;j<=13u;j++){uint16_t v=ram16r((uint16_t)(0xFB00u+j*2u)),q=ram16r((uint16_t)(0xFB80u+j*2u));for(unsigned shift=0u;shift<=8u;shift+=4u)if(((v>>shift)&15u)!=((q>>shift)&15u))v=(uint16_t)(v-(2u<<shift));ram16w((uint16_t)(0xFB00u+j*2u),v);}
            }
            r46_zio_wait(enemies);r46_zio_palette_sync();r46_zio_present_objects(6u,counter<5u?-1:(counter<9u?0:1),0u,-1,x,0xA0u);
        }
        ram8w(0x500Au,0xEAu);
        for(unsigned pulse=0u;pulse<7u;pulse++)for(unsigned counter=1u;counter<=6u;counter++){
            r46_zio_wait(enemies);
            if(counter==3u||counter==6u)r46_zio_present_objects(6u,counter==3u?2:3,0u,-1,x,0xA0u);
            /* Disable source display at counters 1/4; GBA forced blank is
             * white, so disable layers against a black backdrop instead. */
            BG_PALETTE[0]=0u;if(counter==1u||counter==4u)REG_DISPCNT=(uint16_t)(MODE4|OBJ_1D);
            if(counter==3u||counter==6u)REG_DISPCNT=(uint16_t)(MODE4|BG2_ENABLE|OBJ_1D|OBJ_ENABLE);
        }
        r46_zio_wait(enemies);BG_PALETTE[0]=0u;REG_DISPCNT=(uint16_t)(MODE4|OBJ_1D);
        ram16w(0xEF00u,8u);r46_zio_stop();battle_ui_discard_staged_anim();battle_damage_overlay_clear();battle_enemy_cue_cancel();battle_palette_hw_dirty=0u;ram8w(0x500Au,ram8r(0xECECu));
    }
    battle_source_ability_hide_all();ram16w(0xD000u,0u);ram8w(0xD014u,0u);return 1;
}
