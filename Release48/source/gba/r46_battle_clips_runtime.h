/* R46: playback of Genesis-recorded PSIV battle effect clips.
 *
 * tools/generate_r46_battle_clips.py records the original US ROM's own battle
 * effect objects (Battle_RunObjects / Battle_FillSpriteAttributes) one display
 * tick at a time, together with the CRAM lines they use and every Sound_Index
 * write.  This runtime replays exactly those sprite pieces: source mapping
 * offsets, source tiles, source palettes and source SFX ticks.  The only
 * lowering is the established 320x224 -> 240x160 battle projection (x*3/4,
 * y*5/7), done with GBA affine OBJ scaling, and the re-anchoring of tracks on
 * the live fighter that the source object was spawned on (party slot / enemy
 * slot), so a recorded victim or caster follows the real one. Anchors are
 * recorder inferences: CP05 supplies raw clips, but not source save states.
 * Only selected capacity-probed clips are enabled; DF3 art is ROM-corrected.
 */
#include "r46_battle_clips.h"
#define R46_OAM0 64u
#define R46_OAMN 48u
#define R46_AFF0 16u            /* affine groups 16..19 live in attr3 of OAM 64..79 */
#define R46_PALBANK0 8u         /* MD sprite lines 0..3 -> GBA OBJ banks 8..11 */
#define R46_TILE0 512u
#define R46_TILE_END 1024u
#define R46_CACHE 64u
#define R46_PA 341               /* 256*4/3 */
#define R46_PD 358               /* 256*7/5 */
/* Playback state: EWRAM/IWRAM BSS are fully budgeted and the 3 KiB System stack
 * cannot take it on top of deep battle call chains, so it lives in the MD RAM mirror
 * at Battle_Objects_Memory ($FFD000-$FFDFFF), which only the Genesis battle object
 * engine uses; the native battle never touches it. */
typedef struct{uint16_t cache_block[R46_CACHE],cache_tile[R46_CACHE];uint16_t cache_n,tile_next,oam_used,pal_cursor;uint16_t pal_loaded[4];uint16_t cur[64],left[64];} R46State;
/* Event battle 0 keeps Igglanova/Xanafalgue OBJ art in tiles 512..767. */
static void r46_cache_reset(R46State* S){S->cache_n=0u;S->tile_next=(uint16_t)(battle_event_index==0u?768u:R46_TILE0);}
#define R46_MAXSPAN 640u
typedef struct{R46State st;uint16_t spal[256];uint8_t bgmap[192];uint8_t isbg[256];uint16_t nspan;uint8_t span[R46_MAXSPAN][3];} R46Scratch; /* spal: scene palette lines computed once per event */ /* span: row, x0, x1 of background pixels */
#define R46_SCR ((R46Scratch*)(void*)(work_ram+0xD000u))
_Static_assert(sizeof(R46Scratch)<=0x1000u,"Playback fits unused native MD object mirror");
static int r46_block_tile(R46State* S,unsigned bid){
    for(unsigned i=0u;i<S->cache_n;i++)if(S->cache_block[i]==bid)return S->cache_tile[i];
    const R46Block* b=&r46_blocks[bid];unsigned n=(unsigned)b->gw*b->gh;
    if(S->cache_n>=R46_CACHE||S->tile_next+n>R46_TILE_END)return -1;
    unsigned tile=S->tile_next;volatile uint32_t* dst=(volatile uint32_t*)(void*)((uintptr_t)OBJ_VRAM16+tile*32u);
    for(unsigned i=0u;i<n;i++,dst+=8){uint16_t ref=r46_block_tiles[b->first_tile+i];
        if(ref==0xFFFFu){for(unsigned k=0u;k<8u;k++)dst[k]=0u;}
        else{const uint32_t* s=(const uint32_t*)(const void*)(r46_tile_pool+(uint32_t)ref*32u);for(unsigned k=0u;k<8u;k++)dst[k]=s[k];}}
    S->tile_next=(uint16_t)(S->tile_next+n);S->cache_block[S->cache_n]=(uint16_t)bid;S->cache_tile[S->cache_n]=(uint16_t)tile;S->cache_n++;return (int)tile;
}
static void r46_affine_params(void){
    static const int16_t m[4][4]={{R46_PA,0,0,R46_PD},{-R46_PA,0,0,R46_PD},{R46_PA,0,0,-R46_PD},{-R46_PA,0,0,-R46_PD}};
    for(unsigned g=0u;g<4u;g++)for(unsigned k=0u;k<4u;k++)OAM16[((R46_AFF0+g)*4u+k)*4u+3u]=(uint16_t)m[g][k];
}
static const R46Clip* r46_clip_find(unsigned kind,unsigned a,unsigned b){
    unsigned lo=0u,hi=r46_clip_count;
    while(lo<hi){unsigned mid=(lo+hi)>>1;const R46Clip* c=&r46_clips[mid];unsigned k=((unsigned)c->kind<<16)|((unsigned)c->a<<8)|c->b,w=(kind<<16)|(a<<8)|b;
        if(k==w)return c;if(k<w)lo=mid+1u;else hi=mid;}
    return 0;
}
/* Live role context for one playback (MD sprite x coordinates). */
typedef struct{int party_md_x[5];uint8_t party_alive[5];int enemy_md_x[BATTLE_MAX_ENEMIES];uint8_t enemy_alive[BATTLE_MAX_ENEMIES];int caster_x,target_x;uint8_t caster_ok,target_ok;} R46Ctx;
static int r46_role_x(unsigned kind,unsigned idx,const R46Ctx* x){
    switch(kind){
    case 0u:return 0;
    case 1u:if(idx>=5u||!x->party_alive[idx])return 0x7FFF;return x->party_md_x[idx];
    case 2u:if(idx>=BATTLE_MAX_ENEMIES||!x->enemy_alive[idx])return 0x7FFF;return x->enemy_md_x[idx];
    case 3u:return x->caster_ok?x->caster_x:0x7FFF;
    case 4u:return x->target_ok?x->target_x:0x7FFF;
    }
    return 0x7FFF;
}
/* No libgcc division in this freestanding build: shift-subtract signed divide. */
static int r46_sdiv(int n,int d){
    int neg=(n<0)!=(d<0);uint32_t un=(uint32_t)(n<0?-n:n),ud=(uint32_t)(d<0?-d:d),q=0u,r=0u;
    for(int b=31;b>=0;b--){r=(r<<1)|((un>>b)&1u);if(r>=ud){r-=ud;q|=1u<<b;}}
    return neg?-(int)q:(int)q;
}
static int r46_track_x(const R46Track* tr,const R46Ent* e,const R46Ctx* x){
    int xa=r46_role_x(tr->a_kind,tr->a_idx,x);if(xa==0x7FFF)return 0x7FFF;
    if(tr->mode==1u){int xb=r46_role_x(tr->b_kind,tr->b_idx,x);if(xb==0x7FFF)return 0x7FFF;int den=(int)tr->bx-(int)tr->ax;if(!den)return xa+(e->x-tr->ax);
        return xa+r46_sdiv((int)(e->x-tr->ax)*(xb-xa),den);}
    return (tr->a_kind==0u)?e->x:xa+e->x;
}
static void r46_hide_from(R46State* S,unsigned n){for(unsigned i=n;i<S->oam_used;i++)OAM16[(R46_OAM0+i)*4u]=0x0200u;S->oam_used=(uint16_t)n;}
static void r46_pal_tick(R46State* S,const R46Clip* c,unsigned t){
    while(S->pal_cursor<c->ntickpal){const R46TickPal* e=&r46_tickpal[c->first_tickpal+S->pal_cursor];if(e->tick>t)break;
        if(e->pal!=S->pal_loaded[e->line]){dma_copy32(r46_pals[e->pal].c,(void*)(OBJ_PALETTE+(R46_PALBANK0+e->line)*16u),32u);S->pal_loaded[e->line]=e->pal;}S->pal_cursor++;}
}
static void r46_draw_tick(R46State* S,const R46Clip* c,unsigned t,const R46Ctx* x){
    uint16_t* ent_cursor=S->cur;uint16_t* ent_left=S->left;r46_pal_tick(S,c,t);
    unsigned oi=0u;
    /* Selected clips fit the protected 256-tile window. Reset before the tick,
     * never after an earlier OAM entry already references a loaded block. */
    r46_cache_reset(S);
    for(unsigned ti=0u;ti<c->ntracks&&ti<64u;ti++){
        const R46Track* tr=&r46_tracks[c->first_track+ti];if(t<tr->start||t>=tr->end)continue;
        /* advance this track's RLE cursor */
        while(ent_left[ti]==0u){ent_cursor[ti]++;ent_left[ti]=r46_ents[tr->first_ent+ent_cursor[ti]].run;}
        const R46Ent* e=&r46_ents[tr->first_ent+ent_cursor[ti]];ent_left[ti]--;
        if(e->frame==0xFFFFu)continue;int tx=r46_track_x(tr,e,x);if(tx==0x7FFF)continue;
        const R46Frame* f=&r46_frames[e->frame];
        for(unsigned pi=0u;pi<f->count&&oi<R46_OAMN;pi++){
            const R46Piece* p=&r46_pieces[f->first_piece+pi];const R46Block* b=&r46_blocks[p->block];
            int X=tx+p->dx,Y=e->y+p->dy;int cw=b->w*8,ch=b->h*8,W=b->gw*8,H=b->gh*8;
            int hf=p->flags&1u,vf=(p->flags>>1)&1u;
            int pcx28=(X-128)*21+cw*21/2,pcy28=(Y-128)*20+ch*10;
            int bcx28=pcx28-(hf?-1:1)*((cw-W)*21/2),bcy28=pcy28-(vf?-1:1)*((ch-H)*10);
            int fx=bcx28-W*14,fy=bcy28-H*14;int bx=fx>=0?fx/28:-((-fx+27)/28),by=fy>=0?fy/28:-((-fy+27)/28);
            if(bx>=240||by>=160||bx+W<=0||by+H<=0)continue;
            int tile=r46_block_tile(S,p->block);
            if(tile<0)continue; /* selected-data capacity probe proves unreachable */
            unsigned line=(p->flags>>2)&3u,shape=b->shape_size&3u,size=(b->shape_size>>2)&3u,aff=R46_AFF0+(unsigned)(hf|(vf<<1));
            volatile uint16_t* o=OAM16+(R46_OAM0+oi)*4u;
            o[0]=(uint16_t)((by&255)|0x0100u|(shape<<14));o[1]=(uint16_t)((bx&511)|(aff<<9)|(size<<14));o[2]=(uint16_t)((tile&0x3FF)|(1u<<10)|((R46_PALBANK0+line)<<12));
            oi++;if(oi>S->oam_used)S->oam_used=(uint16_t)oi;
        }
    }
    r46_hide_from(S,oi);
    r46_affine_params();
}
/* Scene CRAM events.  MD line 0 is the battle background (GBA LUT slots), lines 1/2
 * are the two enemy groups (GBA BG lines 12/13).  Tints are applied to the colours
 * the battle actually composed (field_pause_pal), so they carry over to any
 * background/enemy exactly like the source's additive palette objects. */
static const uint8_t r46_cv[8]={0,4,9,13,18,22,27,31};
static unsigned r46_lvl(unsigned v){unsigned best=0u,bd=99u;for(unsigned i=0u;i<8u;i++){unsigned d=v>r46_cv[i]?v-r46_cv[i]:r46_cv[i]-v;if(d<bd){bd=d;best=i;}}return best;}
static uint16_t r46_tint(uint16_t c,int dr,int dg,int db){
    int r=(int)r46_lvl(c&31u)+dr,g=(int)r46_lvl((c>>5)&31u)+dg,b=(int)r46_lvl((c>>10)&31u)+db;
    if(r<0)r=0;if(r>7)r=7;if(g<0)g=0;if(g>7)g=7;if(b<0)b=0;if(b>7)b=7;
    return (uint16_t)(r46_cv[r]|(r46_cv[g]<<5)|(r46_cv[b]<<10));
}
static int r46_bg_slot(unsigned i){return i<80u||(i>=85u&&i<96u)||(i>=112u&&i<192u);}
static void r46_scene_line_to(const R46Scene* e,volatile uint16_t* pal,const uint8_t* bgmap);
static void r46_scene_line(const R46Scene* e,const uint8_t* bgmap){r46_scene_line_to(e,(volatile uint16_t*)BG_PALETTE,bgmap);}
/* kind 4 (boss openings): map each composed background slot to the MD line-0 entry of
 * the clip's steady-state palette with the same colour (0xFF = no entry). */
static void r46_bgmap_build(const R46Scene* e,uint8_t* bgmap,int first){
    const uint16_t* ref=r46_pals[e->value].c;
    for(unsigned i=0u;i<192u;i++){if(first)bgmap[i]=0xFFu;if(!r46_bg_slot(i)||bgmap[i]!=0xFFu)continue;for(unsigned k=1u;k<16u;k++)if(ref[k]==field_pause_pal[i]){bgmap[i]=(uint8_t)k;break;}}
}
static void r46_scene_line_to(const R46Scene* e,volatile uint16_t* pal,const uint8_t* bgmap){
    if(e->line==0u){
        for(unsigned i=0u;i<192u;i++)if(r46_bg_slot(i)){uint16_t o=field_pause_pal[i];
            if(e->kind==3u){if(bgmap&&bgmap[i]!=0xFFu)pal[i]=r46_pals[e->value].c[bgmap[i]];else pal[i]=o;continue;}
            pal[i]=e->kind==1u?r46_tint(o,e->dr,e->dg,e->db):e->kind==2u?e->value:o;}
        return;
    }
    unsigned base=(e->line==1u?12u:13u)*16u;
    for(unsigned i=1u;i<16u;i++){uint16_t o=field_pause_pal[base+i];uint16_t v=o;
        if(e->kind==1u){if(e->mask&(1u<<i))v=r46_tint(o,e->dr,e->dg,e->db);}
        else if(e->kind==2u)v=e->value;else if(e->kind==3u)v=r46_pals[e->value].c[i];
        pal[base+i]=v;}
}
/* loc_252D8 / loc_2540E: per-channel white fade on MD lines 0-2, entries 0-13.
 * Out: every channel nibble +1 per step (clamped at $F).  In: start at $F and
 * step -1 toward the target.  The GBA shadow holds MD 3-bit levels (nibble =
 * 2*level), so step s maps exactly to level min(15,2L+s)>>1 / max(2L,15-s)>>1. */
static uint16_t r46_white_c(uint16_t c,unsigned s,int in){
    uint16_t o=0u;
    for(unsigned k=0u;k<3u;k++){unsigned L=r46_lvl((c>>(5u*k))&31u),n=2u*L;
        if(in)n=(15u>s&&15u-s>n)?15u-s:n;else n=(n+s>15u)?15u:n+s;
        o|=(uint16_t)(r46_cv[n>>1]<<(5u*k));}
    return o;
}
static void r46_white_apply(unsigned s,int in){
    volatile uint16_t* bg=(volatile uint16_t*)BG_PALETTE;volatile uint16_t* ob=(volatile uint16_t*)OBJ_PALETTE;
    for(unsigned i=0u;i<192u;i++)if(r46_bg_slot(i)){uint16_t v=r46_white_c(field_pause_pal[i],s,in);bg[i]=v;ob[i]=v;}
    for(unsigned l=12u;l<14u;l++)for(unsigned i=1u;i<14u;i++){unsigned j=l*16u+i;uint16_t v=r46_white_c(field_pause_pal[j],s,in);bg[j]=v;ob[j]=v;}
}
/* Plane-B override (Black Hole's arc, Zio3's black-out): the background pixels of the
 * composed battle frame (bg palette slots) are replaced by a projected source plane-B
 * image, shifted per row by the source H-scroll and by rows for V-scroll; enemies, party
 * and HUD pixels stay.  The image is RLE-decoded into the spare Mode 4 page 1.  While
 * the override is up, BG palette 0..63 holds the source CRAM and slot 79 the fill colour. */
#define R46_BG_FILL_SLOT 79u
static void r46_bg_decode(unsigned img){
    const R46BgImg* im=&r46_bgimg[img];const uint8_t* p=r46_bg_rle+im->off;const uint8_t* e=p+im->len;
    volatile uint16_t* d=MODE4_PAGE1;unsigned n=0u;uint16_t pend=0u;
    while(p<e&&n<240u*160u){unsigned c=*p++;
        if(c&0x80u){unsigned k=(c&0x7Fu)+3u,v=*p++;while(k--){if(n&1u)d[n>>1]=(uint16_t)(pend|(v<<8));else pend=(uint16_t)v;n++;}}
        else{unsigned k=c+1u;while(k--){unsigned v=*p++;if(n&1u)d[n>>1]=(uint16_t)(pend|(v<<8));else pend=(uint16_t)v;n++;}}}
    ps4_menu_base_cached=0u; /* page 1 no longer holds the menu base */
}
/* Background spans of the composed frame (pixels in bg palette slots), built once when
 * the override comes up.  0 = too fragmented: compose per pixel instead. */
static int r46_bg_spans(const uint8_t* isbg){
    R46Scratch* R=R46_SCR;unsigned n=0u;
    for(unsigned y=0u;y<160u;y++){const uint8_t* fb=field_pause_fb+y*240u;unsigned x=0u;
        while(x<240u){while(x<240u&&!isbg[fb[x]])x++;if(x>=240u)break;unsigned x0=x;while(x<240u&&isbg[fb[x]])x++;
            if(n>=R46_MAXSPAN){R->nspan=0u;return 0;}R->span[n][0]=(uint8_t)y;R->span[n][1]=(uint8_t)x0;R->span[n][2]=(uint8_t)x;n++;}}
    R->nspan=(uint16_t)n;return 1;
}
static void r46_bg_compose_pixels(const R46BgEv* ev,const uint8_t* isbg){
    volatile uint16_t* out=MODE4_VRAM16;const volatile uint8_t* img=(const volatile uint8_t*)(const volatile void*)MODE4_PAGE1;
    const int8_t* sh=(ev->kind==1u&&ev->shift!=0xFFFFu)?r46_bg_shift+(uint32_t)ev->shift*160u:0;
    for(unsigned gy=0u;gy<160u;gy++){
        int sy=(int)gy+ev->vshift;if(sy<0)sy=0;if(sy>159)sy=159;
        int s=sh?sh[gy]:0;const uint8_t* fb=field_pause_fb+gy*240u;const volatile uint8_t* row=img+(unsigned)sy*240u;volatile uint16_t* o=out+gy*120u;
        for(unsigned gx=0u;gx<240u;gx+=2u){
            unsigned a=fb[gx],b=fb[gx+1u];
            if(isbg[a]){if(ev->kind==2u)a=R46_BG_FILL_SLOT;else{int x=(int)gx-s;if(x<0)x=0;if(x>239)x=239;a=row[x];}}
            if(isbg[b]){if(ev->kind==2u)b=R46_BG_FILL_SLOT;else{int x=(int)gx+1-s;if(x<0)x=0;if(x>239)x=239;b=row[x];}}
            o[gx>>1]=(uint16_t)(a|(b<<8));
        }
    }
}
/* Odd-aligned VRAM->VRAM row copy (source shifted by one byte): the only hot loop of
 * the override, kept in IWRAM (fits the 80 bytes left below the IRQ stack). */
__attribute__((section(".iwram.text"),noinline,aligned(4)))
static void r46_comb_odd(volatile uint16_t* d,const volatile uint16_t* q,unsigned n){
    uint32_t h0=*q++;
    do{uint32_t h1=*q++;*d++=(uint16_t)((h0>>8)|(h1<<8));h0=h1;}while(--n);
}
static void r46_bg_compose(const R46BgEv* ev,const uint8_t* isbg,const R46BgEv* prev){
    R46Scratch* R=R46_SCR;if(!R->nspan){r46_bg_compose_pixels(ev,isbg);return;}
    /* same image and vertical offset as the frame on screen: only rows whose shift changed */
    const int8_t* psh=0;int partial=prev&&prev->kind==1u&&ev->kind==1u&&prev->img==ev->img&&prev->vshift==ev->vshift;
    if(partial)psh=prev->shift!=0xFFFFu?r46_bg_shift+(uint32_t)prev->shift*160u:0;
    volatile uint16_t* out=MODE4_VRAM16;const volatile uint16_t* p1=MODE4_PAGE1;const volatile uint8_t* p1b=(const volatile uint8_t*)(const volatile void*)MODE4_PAGE1;
    const int8_t* sh=(ev->kind==1u&&ev->shift!=0xFFFFu)?r46_bg_shift+(uint32_t)ev->shift*160u:0;
    const unsigned fill=ev->kind==2u,F=R46_BG_FILL_SLOT;
    for(unsigned k=0u;k<R->nspan;k++){
        unsigned y=R->span[k][0],x=R->span[k][1],x1=R->span[k][2];volatile uint16_t* o=out+y*120u;
        int s=sh?sh[y]:0;if(partial&&s==(psh?psh[y]:0))continue;
        int sy=(int)y+ev->vshift;if(sy<0)sy=0;if(sy>159)sy=159;unsigned rb=(unsigned)sy*240u;
        #define R46PIX(xx) (fill?F:(unsigned)p1b[rb+(unsigned)(((int)(xx)-s)<0?0:(((int)(xx)-s)>239?239:((int)(xx)-s)))])
        if(x&1u){o[x>>1]=(uint16_t)((o[x>>1]&0x00FFu)|(R46PIX(x)<<8));x++;}
        unsigned xe=x1&~1u;
        if(xe>x){
            if(fill){uint16_t v=(uint16_t)(F|(F<<8));volatile uint16_t* d=o+(x>>1);for(unsigned i=0u;i<((xe-x)>>1);i++)d[i]=v;}
            else{
                /* clamped edge pairs (source outside the row) per pixel, interior by copy */
                unsigned xa=x,xb=xe;
                while(xa<xb&&(int)xa-s<0){o[xa>>1]=(uint16_t)(R46PIX(xa)|(R46PIX(xa+1u)<<8));xa+=2u;}
                while(xb>xa&&(int)xb-s>240){xb-=2u;o[xb>>1]=(uint16_t)(R46PIX(xb)|(R46PIX(xb+1u)<<8));}
                if(xb>xa){int sx=(int)xa-s;unsigned n2=(xb-xa)>>1;volatile uint16_t* d=o+(xa>>1);
                    if(!(sx&1))dma_copy16((const void*)(p1+((rb+(unsigned)sx)>>1)),(void*)d,n2*2u);
                    else r46_comb_odd(d,p1+((rb+(unsigned)sx-1u)>>1),n2);}
            }
            x=xe;
        }
        if(x<x1){o[x>>1]=(uint16_t)((o[x>>1]&0xFF00u)|R46PIX(x));}
        #undef R46PIX
    }
}
static int r46_clip_play_ex(const R46Clip* c,const R46Ctx* x,struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],int commit_scene);
static int r46_clip_play(const R46Clip* c,const R46Ctx* x,struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES]){return r46_clip_play_ex(c,x,enemies,0);}
/* commit_scene: the clip's final scene palette state is the battle's new palette
 * (boss opening objects such as DarkForce2's $83C reveal the enemy palette for good). */
static int r46_clip_play_ex(const R46Clip* c,const R46Ctx* x,struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],int commit_scene){
    if(!c||!c->ticks)return 0;
    unsigned sc=0u;int scene_touched=0;const R46Scene* scene_cur[3]={0,0,0};uint8_t* bgmap=R46_SCR->bgmap;int have_map=0;
    R46State* S=&R46_SCR->st;unsigned nt=c->ntracks>64u?64u:c->ntracks;
    for(unsigned i=0u;i<nt;i++){S->cur[i]=0u;S->left[i]=r46_ents[r46_tracks[c->first_track+i].first_ent].run;}
    r46_cache_reset(S);S->oam_used=0u;S->pal_cursor=0u;for(unsigned l=0u;l<4u;l++)S->pal_loaded[l]=0xFFFFu;
    unsigned si=0u,bi=0u,bpi=0u;int bg_on=0;unsigned bg_img=0xFFFFu;const R46BgEv* bg_prev=0;const R46BgPal* bg_pal_cur=0;
    if(c->nscene)dma_copy16(field_pause_pal,R46_SCR->spal,512u);
    if(c->nbg){uint8_t* lut=R46_SCR->isbg;for(unsigned i=0u;i<256u;i++)lut[i]=(uint8_t)(i<192u&&r46_bg_slot(i));}
    for(unsigned t=0u;t<c->ticks;t++){
        const R46BgEv* bev=0;while(bi<c->nbg&&r46_bgev[c->first_bg+bi].tick<=t){bev=&r46_bgev[c->first_bg+bi];bi++;}
        if(bev&&bev->kind==1u&&bev->img!=bg_img){r46_bg_decode(bev->img);bg_img=bev->img;}
        unsigned scene_dirty=battle_generic_pending_mask;const R46BgEv* active_bg=bg_prev;
        waitvb();
        if(scene_dirty&&bg_on){(void)r46_bg_spans(R46_SCR->isbg);bg_prev=0;}
        while(bpi<c->nbgpal&&r46_bgpal[c->first_bgpal+bpi].tick<=t){bg_pal_cur=&r46_bgpal[c->first_bgpal+bpi];bpi++;}
        if(!bev&&bg_on)bev=active_bg;
        if(bev){
            if(bev->kind==0u){if(bg_on){dma_copy32(field_pause_fb,(void*)MODE4_VRAM16,240u*160u);dma_copy32(field_pause_pal,(void*)BG_PALETTE,128u);bg_on=0;}}
            else{if(bev->kind==2u)BG_PALETTE[R46_BG_FILL_SLOT]=bev->color;if(!bg_on){(void)r46_bg_spans(R46_SCR->isbg);bg_prev=0;}r46_bg_compose(bev,R46_SCR->isbg,bg_prev);bg_prev=bev;bg_on=1;}
        }
        if(bg_on&&bg_pal_cur)for(unsigned l=0u;l<4u;l++)dma_copy32(r46_pals[bg_pal_cur->pal[l]].c,(void*)(BG_PALETTE+l*16u),32u);
        while(si<c->nsfx&&r46_sfx[c->first_sfx+si].tick<=t){ram8w(0x500Au,r46_sfx[c->first_sfx+si].sfx);si++;}
        r46_draw_tick(S,c,t,x);
        battle_action_idle_after_wait(enemies); /* retain one normal update per source tick */
        while(sc<c->nscene&&r46_scene[c->first_scene+sc].tick<=t){const R46Scene* e=&r46_scene[c->first_scene+sc];if(e->kind==4u){r46_bgmap_build(e,bgmap,!have_map);have_map=1;}
            else if(e->line<3u){scene_cur[e->line]=e;r46_scene_line_to(e,(volatile uint16_t*)R46_SCR->spal,have_map?bgmap:0);}sc++;scene_touched=1;}
        /* re-publish every tick (the enemy-animation commit may have restored the base palette) */
        if(scene_cur[0]&&!bg_on)dma_copy16(R46_SCR->spal,(void*)BG_PALETTE,192u*2u);
        for(unsigned l=1u;l<3u;l++)if(scene_cur[l])dma_copy16(R46_SCR->spal+(l==1u?12u:13u)*16u+1u,(void*)(BG_PALETTE+(l==1u?12u:13u)*16u+1u),15u*2u);
    }
    r46_hide_from(S,0u);
    if(bg_on){dma_copy32(field_pause_fb,(void*)MODE4_VRAM16,240u*160u);scene_touched=1;}
    if(commit_scene)for(unsigned l=0u;l<3u;l++)if(scene_cur[l])r46_scene_line_to(scene_cur[l],(volatile uint16_t*)field_pause_pal,have_map?bgmap:0);
    if(scene_touched)dma_copy32(field_pause_pal,(void*)BG_PALETTE,512u);
    battle_palette_hw_dirty=1u; /* restore OBJ banks borrowed for source sprite lines */
    return 1;
}
static void r46_ctx_base(R46Ctx* x,const struct BattleLiteActor party[5],unsigned pn,const struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES]){
    static const uint16_t source_x[5]={0x120u,0x0F0u,0x150u,0x0C0u,0x180u};
    for(unsigned i=0u;i<5u;i++){x->party_md_x[i]=source_x[i];x->party_alive[i]=(uint8_t)(i<pn&&party&&party[i].hp);}
    for(unsigned i=0u;i<BATTLE_MAX_ENEMIES;i++){x->enemy_md_x[i]=0x80+(int)((battle_enemy_placement[i]&0x7Fu)<<3);x->enemy_alive[i]=(uint8_t)(i<battle_enemy_count&&enemies&&(enemies[i].hp||(battle_enemy_death_visible_mask&(1u<<i))));}
    x->caster_ok=0u;x->target_ok=0u;x->caster_x=x->target_x=0;
}
/* Enemy action: caster = enemy slot ei, target = victim party slot (if any). */
static int r46_play_enemy_clip(const struct BattleLiteActor party[5],unsigned pn,struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned ei,uint8_t ability,unsigned victim){
    if(ei>=battle_enemy_count)return 0;const R46Clip* c=r46_clip_find(R46_CLIP_ENEMY,enemies[ei].id,ability);if(!c)return 0;
    R46Ctx x;r46_ctx_base(&x,party,pn,enemies);
    x.caster_x=x.enemy_md_x[ei];x.caster_ok=1u;
    if(victim<5u&&x.party_alive[victim]){x.target_x=x.party_md_x[victim];x.target_ok=1u;}
    return r46_clip_play(c,&x,enemies);
}
/* $FFEE87 opening action (boss reveal objects): caster = enemy ei, no target. */
static int r46_play_enemy_intro(const struct BattleLiteActor party[5],unsigned pn,struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned ei){
    if(ei>=battle_enemy_count)return 0;const R46Clip* c=r46_clip_find(R46_CLIP_ENEMY_INTRO,enemies[ei].id,0u);if(!c)return 0;
    R46Ctx x;r46_ctx_base(&x,party,pn,enemies);x.caster_x=x.enemy_md_x[ei];x.caster_ok=1u;
    return r46_clip_play_ex(c,&x,enemies,1);
}
/* Combo: caster = first participant (who[0]); target = current enemy target. */
static int r46_play_combo_clip(const struct BattleLiteActor party[5],unsigned pn,struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned combo,unsigned current_target,const unsigned who[4],unsigned participants){
    const R46Clip* c=r46_clip_find(R46_CLIP_COMBO,combo,0u);if(!c)return 0;
    /* Source Holocaust/Black Hole bypass plane rewrites against PD. Keep
     * existing PD-owned presentation until that branch is captured. */
    for(unsigned i=0u;i<battle_enemy_count;i++)if(enemies[i].id>=0x85u&&enemies[i].id<=0x87u)return 0;
    R46Ctx x;r46_ctx_base(&x,party,pn,enemies);
    if(participants&&who[0]<pn){x.caster_x=x.party_md_x[who[0]];x.caster_ok=1u;}
    unsigned et=current_target;if(et>=battle_enemy_count||!enemies[et].hp){for(unsigned i=0u;i<battle_enemy_count;i++)if(enemies[i].hp){et=i;break;}}
    if(et<battle_enemy_count&&x.enemy_alive[et]){x.target_x=x.enemy_md_x[et];x.target_ok=1u;}
    return r46_clip_play(c,&x,enemies);
}
/* Selected actor-free vehicle and offensive item captures. Presentation only:
 * the native caller consumes resources/applies gameplay once after returning.
 * Friendly items and incomplete actor-bearing normal attacks keep native paths. */
static int r46_play_command_clip(const struct BattleLiteActor party[5],unsigned pn,struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned enemy_target,unsigned actor,const struct BattleCommandLite* cmd){
    if(!cmd||actor>=pn)return 0;const R46Clip* c=0;
    if(cmd->type==4u)c=r46_clip_find(R46_CLIP_ITEM,cmd->id,0u);
    else if(cmd->type==6u&&party[actor].id>=11u&&party[actor].id<14u)c=r46_clip_find(R46_CLIP_VEHICLE,party[actor].id-10u,cmd->id);
    else if(cmd->type==1u&&party[actor].id>=11u&&party[actor].id<14u)c=r46_clip_find(R46_CLIP_VEHICLE,party[actor].id-10u,0u);
    if(!c)return 0;R46Ctx x;r46_ctx_base(&x,party,pn,enemies);
    unsigned et=enemy_target;if(et>=battle_enemy_count||!enemies[et].hp){for(unsigned i=0u;i<battle_enemy_count;i++)if(enemies[i].hp){et=i;break;}}
    x.caster_x=x.party_md_x[actor];x.caster_ok=1u;
    if(et<battle_enemy_count&&x.enemy_alive[et]){x.target_x=x.enemy_md_x[et];x.target_ok=1u;}
    uint8_t pa=battle_party_pose_actor,pi=battle_party_pose_index;
    if(party[actor].id<11u){int pose=battle_party_cast_pose_index(party[actor].id);if(pose>=0){battle_party_pose_actor=(uint8_t)actor;battle_party_pose_index=(uint8_t)pose;}}
    battle_present_action(party,pn,enemies,et<battle_enemy_count?et:0u,0,actor,cmd,0u);
    int ok=r46_clip_play(c,&x,enemies);
    battle_party_pose_actor=pa;battle_party_pose_index=pi;
    battle_present_action(party,pn,enemies,et<battle_enemy_count?et:0u,0,actor,cmd,0u);
    return ok;
}
static int r46_play_vehicle_attack(const struct BattleLiteActor party[5],unsigned pn,struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned enemy_target,unsigned actor,const struct BattleCommandLite* cmd){
    if(!cmd||cmd->type!=1u||actor>=pn||party[actor].id<11u||party[actor].id>=14u)return 0;
    return r46_play_command_clip(party,pn,enemies,enemy_target,actor,cmd);
}
