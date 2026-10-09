/* CP15: Zio3 alone owns this source scene; no new EWRAM/IWRAM allocation.
 * FB00/FB80 and 2A90 are the original CRAM shadows/palette object arena.
 * D000 is the original root object. Later Zio variants never enter this path. */
static int r46_zio_formation(const struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES]){
    for(unsigned i=0u;i<battle_enemy_count;i++)if(enemies[i].id==0x98u)return 1;return 0;
}
static int r46_zio_story_exited(const struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES]){return ram8r(0xEE98u)>=5u&&r46_zio_formation(enemies);}
static int r46_zio_scene_active(void){return ram8r(0xEE68u)&&ram8r(0xEE98u);}
static void r46_zio_reset(const struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES]){
    ram8w(0xEE98u,0u);ram8w(0xEE68u,(uint8_t)(r46_zio_formation(enemies)?0xFFu:0u));ram16w(0x2A90u,0u);ram16w(0x2AA0u,0u);ram16w(0xD000u,0u);
}
static void r46_zio_stop(void){ram8w(0xEE68u,0u);ram16w(0x2A90u,0u);ram16w(0x2AA0u,0u);ram16w(0xD000u,0u);}
__attribute__((noinline))
static void r46_zio_background_copy(const uint8_t* src,uint8_t* dst,unsigned n){
    if(r46_zio_scene_active())src=r46_zio_bank+R46_ZIO_BG_OFF+(unsigned)(dst-field_pause_fb);
    dma_copy32(src,dst,n);
}
static int r46_zio_body_full(void){return ram8r(0xEE98u)>=4u&&(ram16r(0xD000u)!=0x910u||ram8r(0xD011u)>=53u);}
static void r46_zio_palette_sync(void){
    for(unsigned j=0u;j<16u;j++){
        uint16_t a=md2gba(ram16r((uint16_t)(0xFB00u+j*2u))),b=md2gba(ram16r((uint16_t)(0xFB40u+j*2u))),c=md2gba(ram16r((uint16_t)(0xFB20u+j*2u)));
        field_pause_pal[240u+j]=a;field_pause_pal[192u+j]=b;field_pause_pal[208u+j]=c;
        BG_PALETTE[240u+j]=a;BG_PALETTE[192u+j]=b;BG_PALETTE[208u+j]=c;
    }
}
__attribute__((noinline))
static void r46_zio_palette_tick(void){
    if(!r46_zio_scene_active()||ram16r(0x2A90u)!=0x2Cu)return;
    unsigned f=ram8r(0x2A93u),t=ram8r(0x2A92u);
    if(t>=rom8(0x343EBu+f)){t=0u;f=(f+1u)%8u;}
    ram8w(0x2A92u,(uint8_t)(t+1u));ram8w(0x2A93u,(uint8_t)f);
    ram16w(0xFB50u,rom16(0x343F4u+f*2u));
    if(ram16r(0x2AA0u)==0x22u){unsigned frame=ram8r(0x2AA3u),timer=ram8r(0x2AA2u);if(timer>=3u){timer=0u;frame=(frame+1u)&3u;}ram8w(0x2AA2u,(uint8_t)(timer+1u));ram8w(0x2AA3u,(uint8_t)frame);ram16w(0xFB38u,rom16(0x33F96u+frame*2u));ram16w(0xFB3Au,rom16(0x33F9Eu+frame*2u));}
    r46_zio_palette_sync();
}
__attribute__((noinline))
static void r46_zio_mask_plane(void){
    if(!r46_zio_scene_active())return;
    int x=battle_enemy_source_center_x(ram8r(0xD01Fu));
    unsigned root=ram16r(0xD000u),mask=ram8r(0xD014u),full=(unsigned)r46_zio_body_full();
    /* EnemyInit_Zio/loc_342DC clear the upper nine source rows.  The private
     * background remains visible there. After Nightmare2, restored upper
     * cells use source line 1, while the lower seven rows use line 2. */
    int left=x-36,right=x+36;if(left<0)left=0;if(right>240)right=240;
    for(unsigned y=0u;y<45u;y++)for(int px=left;px<right;px++){
        unsigned at=y*240u+(unsigned)px;uint8_t c=field_pause_fb[at];
        if(c>=192u&&c<208u)field_pause_fb[at]=full?(uint8_t)(c+16u):r46_zio_bank[R46_ZIO_BG_OFF+at];
    }
    if(!mask)return;
    left=root==0x908u?x:x-36;right=root==0x908u?x+24:x+36;
    if(left<0)left=0;if(right>240)right=240;
    for(unsigned y=45u;y<85u;y++){
        unsigned at=y*240u+(unsigned)left;for(int px=left;px<right;px++)field_pause_fb[at+(unsigned)(px-left)]=r46_zio_bank[R46_ZIO_BG_OFF+at+(unsigned)(px-left)];
    }
}
