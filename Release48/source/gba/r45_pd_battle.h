/* Profound Darkness changes form through EnemyAI_HalfHPOrLower and the
 * Nothing3/4 ($67/$68) enemy actions, NOT through Enemy_Dead. Source roots
 * $888/$89C load stats at their authored table callback. */
/* PD sprites occupy scanlines 0..124; the opaque party HUD begins at source
 * window row 22 -> GBA y=125. A complete scene is published immediately before
 * every PD controller, so its lower 35 scanlines can remain resident while the
 * authored controller updates only the moving scene band. This keeps every
 * source tick and only removes redundant HUD composition/VRAM transfer. */
static void r45_pd_present_controller_tick(struct BattleLiteActor party[5],unsigned pn,struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned target){
 R45PDVisual*s=r45_pd_visual();unsigned full_y1=battle_src_y(22u),party_y=(0x80u*5u)/7u;
 int fixed=s->resident_valid&&s->bg_id==s->bg_map&&s->resident_map==s->bg_map&&s->resident_party_mask==battle_party_visible_mask;
 int static_party=battle_party_pose_actor==255u&&!battle_party_anim_attack&&battle_party_x_override_actor==255u&&!battle_show_enemy_target&&!battle_damage_enemy_mask&&!battle_damage_party_mask;
 for(unsigned i=0;i<pn&&i<5u;i++)if(party[i].id>=11u)static_party=0;
 if(!static_party)fixed=0;
 for(unsigned y=party_y;y<full_y1;y++){unsigned sy=src_y[y];int16_t shift=sy<192u?s->bg[sy]:0;if(s->resident_bg[y-party_y]!=shift)fixed=0;}
 unsigned y1=fixed?party_y:full_y1;
 battle_compose_clip_set(0,0u,240,y1);
 battle_compose_igg(party,pn,enemies,target,0u);
 battle_compose_clip_clear();
 battle_present_band(0u,y1);
 /* The body ends above this band. Retain it only when every background
  * scroll row and the static party layer still match the published page. */
 for(unsigned y=party_y;y<full_y1;y++){unsigned sy=src_y[y];s->resident_bg[y-party_y]=sy<192u?s->bg[sy]:0;}
 s->resident_valid=(uint8_t)(s->bg_id==s->bg_map&&static_party);s->resident_map=s->bg_map;s->resident_party_mask=battle_party_visible_mask;
}
static void r45_pd_replace(struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned ei,unsigned id){
 battle_load_enemy(id,&enemies[ei]);
 /* The sprite callback precedes the stat callback; retain its live phase. */
 battle_enemy_anim_cache_bind(enemies,battle_enemy_count);battle_enemy_reaction_flags[ei]=0u;
 battle_enemy_death_visible_mask&=(uint8_t)~(1u<<ei);battle_enemy_death_pending_mask&=(uint8_t)~(1u<<ei);
 battle_generic_pending_mask=battle_generic_stage_ready=0u;
}
static void r45_pd_opening(struct BattleLiteActor party[5],unsigned pn,struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES]){
 if(!r45_pd_formation(enemies)||enemies[0].id!=0x85u)return;
 R45PDVisual*s=r45_pd_visual();r46_pd_cache_retire();s->controller=1u;s->resident_valid=0u;(void)r46_pd_body_map(s->body_id);
 /* EnemyInit's EE87 gives PD1 the first action. That action is $864, with
  * no attack roll, target selection, HP change or party turn consumed. */
 for(unsigned f=0;f<192;f++){r45_pd_palette_tick();battle_enemy_anim_advance_generic(enemies);r45_pd_body_snapshot(0x85u);r45_pd_open_tick(f);r45_pd_present_controller_tick(party,pn,enemies,0u);}
 s->bg_angle=191u;
 s->controller=0u;r46_pd_cache_retire();
}
/* Source controller stages: 16 palette ticks; 168/192 expansion ticks;
 * callback table with final zero sentinel tick; 168/192 contraction ticks;
 * 16 palette ticks. $8E8 is after $DA00 in PD1 and before it in PD2. */
__attribute__((noinline))
static int r45_pd_transform(struct BattleLiteActor party[5],unsigned pn,struct BattleLiteActor enemies[BATTLE_MAX_ENEMIES],unsigned ei,unsigned skill){
 if(ei>=battle_enemy_count)return 0;unsigned old=enemies[ei].id;
 if(!((old==0x85u&&skill==0x67u)||(old==0x86u&&skill==0x68u)))return 0;
 unsigned expand=old==0x85u?168u:192u,table=old==0x85u?0x2F70Au:0x2EB66u;
 unsigned callback=rom8(table+6u),last=16u+expand+callback+1u+expand,total=last+16u;
 R45PDVisual*s=r45_pd_visual();r46_pd_cache_retire();s->controller=1u;s->resident_valid=0u;s->body_id=(uint8_t)old;(void)r46_pd_body_map(old);
 for(unsigned i=0;i<192;i++)s->fg[i]=s->bg[i]=0;
 r45_pd_body_snapshot(old);r45_pd_source_init(old);
 for(unsigned i=0;i<4;i++)s->cycle_on[i*2u]=0u;
 unsigned stage2=16u+expand,stage3=stage2+callback+1u;
 /* Original roots $2F560/$2E9DA clear all four old enemy sprite objects
  * ($2F5AC/$2EA26). The sprite callback creates the new objects; their first
  * update is the following tick. Keeping old objects alive overwrites the
  * newly queued base at their old animation destinations. */
 unsigned animate_at=stage2+rom8(table+5u);
 for(unsigned f=0u;f<total;f++){
  r45_pd_palette_tick();
  if(f>=animate_at){struct BattleLiteActor animated[BATTLE_MAX_ENEMIES];for(unsigned i=0;i<battle_enemy_count;i++)animated[i]=enemies[i];animated[ei].id=s->anim_id;
   battle_enemy_anim_advance_generic(animated);r45_pd_body_animate(s->anim_id,s->source_bytes);}
  if(f<16u){if((f&7u)==7u)r45_pd_pal_range(16u,-2,1);}
  else if(f<stage2)r45_pd_expand(f-15u);
  else if(f<stage3){
   unsigned t=f-stage2+1u;r45_pd_expand(expand);
   if(t==1u)s->bg_map=(uint8_t)(old+1u-0x85u);
   if(t==rom8(table+1u))r45_pd_queue(1u,old+1u);
   if(t==rom8(table+2u))r45_pd_queue(2u,old+1u);
   if(t==rom8(table+3u))r45_pd_queue(4u,old+1u);
   if(t==rom8(table+4u))r45_pd_queue(3u,old+1u);
   if(t==rom8(table+5u)){s->anim_id=(uint8_t)(old+1u);for(unsigned op=0;op<BATTLE_ENEMY_ANIM_COMPONENTS;op++)battle_enemy_anim_frame[ei][op]=battle_enemy_anim_timer[ei][op]=0u;}
   if(t>rom8(table+4u)&&t<rom8(table+5u))r45_pd_queue_tick(1u);
  }else if(f<last){if(f==stage3)r45_pd_new_palette(old);r45_pd_contract((expand-2u-(f-stage3))&255u,expand);}
  if(f>=last&&((f-last)&7u)==7u)r45_pd_pal_range(16u,2,0);
  /* $D800 periodic child is visited before $DA00; $DC00 after it. */
  if(f<last&&((f-(old==0x85u?0u:1u))&3u)==0u)ram8w(0x500Au,0xDDu);
  if(f==last){ram8w(0x500Au,old==0x85u?0xEDu:0xA9u);if(old==0x86u)ram8w(0x500Bu,0xEDu);}
  if(f==16u+expand+callback-1u){
   r45_pd_replace(enemies,ei,old+1u);
   s->body_id=(uint8_t)(old+1u);
  }
  r45_pd_queue_tick(0u);
  s->bg_angle++;for(unsigned i=0;i<120;i++)s->bg[i]=(int16_t)(r45_pd_cos(s->bg_angle+i*4u)>>3);
  if(old==0x86u&&f==total-1u)for(unsigned i=9u;i<12u;i++){s->cycle_on[i]=1u;s->cycle_frame[i]=s->cycle_timer[i]=0u;}
  r45_pd_present_controller_tick(party,pn,enemies,ei);battle_obj_sync(enemies,0u);
 }
 s->controller=0u;r46_pd_cache_retire();
 return 1;
}
