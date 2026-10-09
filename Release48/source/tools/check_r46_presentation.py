#!/usr/bin/env python3
"""ASan/UBSan probes of production drawing, DF3 controller and debug controls.

Hardware/VDP, sound output, actor composition and button input are fixtures.
"""
from pathlib import Path
import json, os, re, subprocess

R=Path(__file__).resolve().parents[1]; O=R.parent/'build_artifacts'
prod=(R/'gba/title_runtime_v06.c').read_text()
def fn(name):
    m=re.search(r'^static[^\n]*\b'+name+r'\([^;]*?\)\s*\{',prod,re.M);assert m,name
    p=m.end();d=1
    while d:d+=(prod[p]=='{')-(prod[p]=='}');p+=1
    return prod[m.start():p]+'\n'
def run(name,code,extra=()):
    c=O/(name+'.c');c.write_text(code);exe=O/name
    subprocess.run(['gcc','-std=c99','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie','-I'+str(R/'gba'),str(c),*map(str,extra),'-o',str(exe)],check=True)
    p=subprocess.run([str(exe),str(R/'references/roms/ps4built.bin'),str(O)],check=True,capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
    print(p.stdout,end='');return p.stdout
pre=r'''
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "r28_readable_font.h"
#include "r29_dialogue_font.h"
#include "battle_damage_window_data.h"
#include "r46_debug_data.h"
#include "r28_vehicle_assets.h"
#define PS4_WINDOW_PAL_BASE 64u
#define PS4_PRECOMP_BATTLE_FONT6X6_COUNT 87u
#define PS4_PRECOMP_BATTLE_FONT6X6_MASK_OFF 0u
#define PS4_ITEM_NAMES 0x2AAFB4u
#define PS4_TECH_NAMES 0x2AB622u
#define PS4_SKILL_NAMES 0x2AB702u
#define PS4_COMBO_NAMES 0x28554Cu
#define PS4_COMBO_COUNT 14u
#define BATTLE_MAX_ENEMIES 4
#define BATTLE_SOURCE_ABILITY_PALBANK 15u
#define BATTLE_SOURCE_ABILITY_OBJ0 64u
#define BATTLE_SOURCE_ABILITY_OBJN 48u
#define BATTLE_ATTACK_OBJ_TILE 768u
struct BattleLiteActor {unsigned id,hp;};
struct BattleCommandLite {uint8_t type,id,target,inv_slot;};
static uint8_t field_pause_fb[38400],title_fb[38400],rom[0x300000],ps4_precomp_assets[87*36];
static uint16_t oam[512];
#define OAM16 oam
static uint8_t battle_damage_columns;
static unsigned rect_right,glyph_right=240,glyph_bottom=160,events[16],event_count,event_pos,presents;
static uint8_t flag_bits[3][256];
static unsigned mutations,changed_type,changed_id,changed_value;
static unsigned rom8(uint32_t p){assert(p<sizeof rom);return rom[p];}
static void hw_hide_obj(unsigned i){assert(i<128);OAM16[i*4]=0x200;}
static void battle_draw_ram_name6(unsigned x,unsigned y,unsigned id,uint8_t c){(void)id;(void)c;assert(x+48<=rect_right&&y+7<=160);}
static void battle_ui_box_source(unsigned wx,unsigned wy,unsigned ww,unsigned wh,unsigned*x,unsigned*y,unsigned*r,unsigned*b){*x=wx*6;*y=wy*40/7;*r=(wx+ww+1)*6;*b=(wy+wh+1)*40/7;rect_right=*r;glyph_right=*r-6;}
static void battle_ui_overlay_set(unsigned x,unsigned y,unsigned r,unsigned b){assert(!x&&!y&&r==rect_right&&b<=23);}
static void ps4_ui_clear(void){memset(field_pause_fb,0,sizeof field_pause_fb);glyph_right=240;glyph_bottom=160;}
static void ps4_ui_box(unsigned x,unsigned y,unsigned r,unsigned b){assert(x<r&&r<=240&&y<b&&b<=160);}
static void ps4_ui_wait_buttons_up(void){}
static unsigned ps4_ui_wait_edge(uint8_t*p){(void)p;assert(event_pos<event_count);return events[event_pos++];}
static void ps4_ui_present(void){presents++;}
static void pause_draw_cursor(unsigned x,unsigned y,unsigned c){(void)c;assert(x<240&&y+8<=160);}
static unsigned ps4_flag_test_type(unsigned t,unsigned i){assert(t<3&&i<256);return flag_bits[t][i];}
static void ps4_flag_set_type(unsigned t,unsigned i){flag_bits[t][i]=1;mutations++;changed_type=t;changed_id=i;changed_value=1;}
static void ps4_flag_clear_type(unsigned t,unsigned i){flag_bits[t][i]=0;mutations++;changed_type=t;changed_id=i;changed_value=0;}
'''
names=['ps4_window_ascii_code','ps4_window_font_index','battle_draw_window_glyph6','battle_draw_text6_limit',
       'battle_draw_rom_text6_limit','battle_draw_rom_name6_limit','battle_draw_uint6',
       'battle_enemy_name_off','battle_enemy_skill_name_off','battle_src_y',
       'battle_draw_damage_glyph','battle_draw_damage_window','ps4_readable_dialogue_glyph',
       'title_intro_text_ptr','title_intro_draw_line','game_start_draw_scaled_source_glyph',
       'battle_obj_geometry','battle_effect_affine','battle_obj_any','battle_source_obj_prebound4',
       'battle_source_y_to_gba','battle_draw_action_banner','ps4_debug_select_encounter','ps4_debug_flag_help','ps4_debug_edit_flag']
body=''.join(fn(n) for n in names)
main=r'''
static void queue(unsigned e){event_count=1;event_pos=0;events[0]=e;}
static void glyph_expected(uint8_t*expected,unsigned x,unsigned y,unsigned ch,unsigned color){
 for(unsigned dy=0;dy<11&&y+dy<160;dy++)for(unsigned dx=0;dx<6&&x+dx<240;dx++)
  if(r29_dialogue_font[ch&127][dy]&(128>>dx))expected[(y+dy)*240+x+dx]=color;
}
int main(int argc,char**argv){
 assert(argc==3);FILE*f=fopen(argv[1],"rb");assert(f&&fread(rom,1,sizeof rom,f)==sizeof rom);fclose(f);
 memset(ps4_precomp_assets,1,sizeof ps4_precomp_assets);
 unsigned damage_cases=0;
 for(unsigned cols=1;cols<=5;cols+=2)for(unsigned heal=0;heal<2;heal++)for(unsigned value=0;value<=1000;value++){
  static const int centers[3]={0,120,239};
  for(unsigned c=0;c<3;c++){
   memset(field_pause_fb,0,sizeof field_pause_fb);battle_damage_columns=cols;
   battle_draw_damage_window(centers[c],heal?18:5,value,heal);
   int left=(((centers[c]*4)/3-24)&~7);if(left<0)left=0;if(left>280)left=280;
   unsigned x0=left*3/4,y0=battle_src_y(heal?18:5),v=value>999?999:value;
   unsigned digit_base=heal?13:3;
   unsigned glyphs[5]={0,v<100?2:digit_base+v/100,v<10?2:digit_base+(v/10)%10,digit_base+v%10,1};
   unsigned start=(5-cols)/2,end=5-start;
   for(unsigned y=0;y<160;y++)for(unsigned x=0;x<240;x++){
    unsigned want=0;
    if(x>=x0&&x<x0+30&&y>=y0&&y<y0+12){
     unsigned col=(x-x0)/6,dx=(x-x0)%6,dy=y-y0,g=glyphs[col];
     if(col>=start&&col<end){unsigned num=g>=3,ci=battle_damage_glyphs[(num?2:g)*72+dy*6+dx];if(ci)want=64+ci;
      if(num&&dx<5&&dy>=3&&dy<10&&(r28_font5x7[27+g-digit_base][dx]&(1<<(dy-3))))want=64+(heal?11:15);
     }
    }
    assert(field_pause_fb[y*240+x]==want);
   }
   damage_cases++;
  }
 }
 /* Compare actual font paths pixel-for-pixel, including clipped screen edges. */
 static uint8_t expected[38400];
 for(unsigned ch=0;ch<256;ch++)for(unsigned edge=0;edge<3;edge++){
  unsigned x=edge==0?30:(edge==1?237:240),y=edge==0?20:(edge==1?157:160);
  memset(expected,0,sizeof expected);memset(field_pause_fb,0,sizeof field_pause_fb);
  glyph_expected(expected,x,y,ch,83);ps4_readable_dialogue_glyph(field_pause_fb,x,y,ch,83);assert(!memcmp(expected,field_pause_fb,sizeof expected));
 }
 for(unsigned ch=0;ch<256;ch++){
  memset(expected,0,sizeof expected);memset(field_pause_fb,0,sizeof field_pause_fb);
  glyph_expected(expected,30,62,ch,240);game_start_draw_scaled_source_glyph(40,88,ch,240);assert(!memcmp(expected,field_pause_fb,sizeof expected));
 }
 for(unsigned id=0;id<8;id++)for(unsigned line=0;line<2;line++){
  memset(expected,0,sizeof expected);memset(title_fb,0,sizeof title_fb);unsigned x=54,y=line?85:62,c=line?47:31,p=title_intro_text_ptr(id);
  while(rom[p]!=255&&x<232){unsigned ch=rom[p++];if(ch>=240)continue;glyph_expected(expected,x,y,ch,c);x+=6;}
  title_intro_draw_line(id,line);assert(!memcmp(expected,title_fb,sizeof expected));
 }
 /* Every raw effect geometry uses separate inverse matrices for flips. */
 static const unsigned dims[12][2]={{8,8},{16,16},{32,32},{64,64},{16,8},{32,8},{32,16},{64,32},{8,16},{8,32},{16,32},{32,64}};
 for(unsigned g=0;g<12;g++)for(unsigned flip=0;flip<2;flip++){
  memset(oam,0xa5,sizeof oam);unsigned w=dims[g][0],h=dims[g][1],shape=g/4,size=g%4;
  battle_obj_any(16,120,80,w,h,768,flip,1,1);unsigned matrix=flip?30:31;
  assert(oam[64]==((80-h/2)|0x2100|(shape<<14)));assert(oam[65]==((120-w/2)|(matrix<<9)|(size<<14)));
  assert(oam[matrix*16+3]==(uint16_t)(flip?-341:341)&&oam[matrix*16+7]==0&&oam[matrix*16+11]==0&&oam[matrix*16+15]==358);
  for(unsigned i=0;i<512;i++)if(i<64||i>67)if(i!=matrix*16+3&&i!=matrix*16+7&&i!=matrix*16+11&&i!=matrix*16+15)assert(oam[i]==0xa5a5);
  for(int ox=-96;ox<=96;ox+=16)for(int oy=-96;oy<=96;oy+=16){
   battle_source_obj_prebound4(64,120,80,ox,oy,shape|(size<<2),768);
   int x=120+(ox+(int)w/2)*3/4-(int)w/2,y=80+(oy+(int)h/2)*5/7-(int)h/2;
   assert(oam[256]==((y&255)|0x100|(shape<<14))&&oam[257]==((x&511)|(31<<9)|(size<<14)));
  }
 }
 for(unsigned id=0;id<153;id++){queue(0x20);assert(ps4_debug_select_encounter("ENEMIES",0,id)==(int)id);}
 for(unsigned id=0;id<sizeof r46_debug_bosses/sizeof r46_debug_bosses[0];id++){queue(0x20);assert(ps4_debug_select_encounter("BOSSES",1,id)==(int)id);}
 queue(0x80);assert(ps4_debug_select_encounter("ENEMIES",0,0)==-1);
 events[0]=1;events[1]=0x20;event_count=2;event_pos=0;assert(ps4_debug_select_encounter("ENEMIES",0,0)==152);
 for(unsigned t=0;t<3;t++)for(unsigned id=0;id<256;id++){
  unsigned pages=ps4_debug_flag_help(r46_debug_flag_help[t][id],0);assert(pages>=1);
  for(unsigned page=0;page<pages;page++)ps4_debug_flag_help(r46_debug_flag_help[t][id],page);
  for(unsigned state=0;state<2;state++){
   flag_bits[t][id]=state;unsigned before=mutations;queue(0x80);assert(!ps4_debug_edit_flag(t,id)&&mutations==before&&flag_bits[t][id]==state);
   queue(0x20);assert(ps4_debug_edit_flag(t,id)&&mutations==before+1&&changed_type==t&&changed_id==id&&changed_value==!state);
  }
 }
 struct BattleLiteActor party[5]={{0,100}},enemy[4]={{0x84,100}};struct BattleCommandLite cmd={0};
 for(unsigned type=1;type<=5;type++){cmd.type=type;unsigned n=type==2?38:(type==3?54:(type==4?160:1));
  for(unsigned id=1;id<=n;id++){cmd.id=id;memset(field_pause_fb,0,sizeof field_pause_fb);battle_draw_action_banner(party,enemy,0,0,&cmd,0);
   for(unsigned y=0;y<160;y++)for(unsigned x=rect_right-6;x<240;x++)assert(!field_pause_fb[y*240+x]);
  }
 }
 for(unsigned id=0;id<153;id++){enemy[0].id=id;cmd.id=0;memset(field_pause_fb,0,sizeof field_pause_fb);battle_draw_action_banner(party,enemy,1,0,&cmd,0);
  for(unsigned y=0;y<160;y++)for(unsigned x=rect_right-6;x<240;x++)assert(!field_pause_fb[y*240+x]);}
 for(unsigned id=1;id<=14;id++){memset(field_pause_fb,0,sizeof field_pause_fb);battle_draw_action_banner(party,enemy,0,0,&cmd,id);
  for(unsigned y=0;y<160;y++)for(unsigned x=rect_right-6;x<240;x++)assert(!field_pause_fb[y*240+x]);}
 for(unsigned v=11;v<=13;v++)for(unsigned id=0;id<9;id++){
  party[0].id=v;cmd.type=id?6:1;cmd.id=id;memset(field_pause_fb,0,sizeof field_pause_fb);battle_draw_action_banner(party,enemy,0,0,&cmd,0);
  unsigned n=(unsigned)strlen(r28_vehicle_names[v-11]);for(unsigned y=0;y<160;y++)for(unsigned x=rect_right-6;x<240;x++)assert(!field_pause_fb[y*240+x]);
  unsigned gap=6+n*6;for(unsigned y=0;y<16;y++)for(unsigned x=gap;x<gap+6;x++)assert(!field_pause_fb[y*240+x]);
 }
 printf("PASS production presentation: %u damage windows, 1024 glyph/edge cases, 16 intro lines, 24 raw effect geometries, 4056 part positions\n",damage_cases);
 puts("PASS all 153 enemy / 56 boss selections; all 768 flag descriptions; 1536 explicit edits and cancelled edits; source action-name bounds");
}
'''
log=run('r46_presentation_probe',pre+body+main)

# Use the production DF3 controller and generic frame-hold player, with the
# rendering/hardware callbacks recording their effects after each source tick.
bank=(R/'gba/source_ability_effects.bin').read_bytes()
text=(R/'gba/source_ability_effects.c').read_text()
m=re.search(r'ps4_source_ability_anims\[\]=\{(.*?)\n\};',text,re.S)
holds={}
for line in m[1].splitlines():
    v=[int(q.removesuffix('u'),0) for q in re.findall(r'0x[0-9A-Fa-f]+u?|\d+u?',line)]
    if not v:continue
    for i in range(v[2]):holds[v[3]+i]=bank[v[3]+i]
pre2=r'''
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "source_ability_effects.h"
#include "source_enemy_effects.h"
#define BATTLE_MAX_ENEMIES 4
#define BATTLE_SOURCE_ABILITY_PALBANK 15u
#define BATTLE_SOURCE_ABILITY_OBJ0 64u
#define BATTLE_SOURCE_ABILITY_OBJN 48u
#define BATTLE_ATTACK_OBJ_TILE 768u
struct BattleLiteActor {unsigned id,hp;};
static uint16_t obj_palette[256];
#define OBJ_PALETTE obj_palette
static unsigned battle_enemy_count=1,battle_event_index=255,ticks,idles,kind,frame,child_calls,targets;
static unsigned battle_source_effect_active_oam,obj_kind[128],obj_frame[128],tile_frame[1024];
static unsigned kinds[100],frames[100],colors[100],cues[4],cue_ticks[4],ncues;
static const uint8_t cvt[8]={0,4,9,13,18,22,27,31};
static void waitvb(void){assert(ticks<99);ticks++;}
static void battle_action_idle_after_wait(struct BattleLiteActor*e){(void)e;assert(idles+1==ticks);idles++;kind=0;unsigned children=0,charges=0;
 for(unsigned i=64;i<112;i++){kind|=obj_kind[i];if(obj_kind[i]==2){children++;frame=obj_frame[i];}if(obj_kind[i]==1)charges++;}
 assert(!children||children==targets);assert(charges<=1);
 if((kind&2)&&(!(kinds[ticks-1]&2)||frames[ticks-1]!=frame))child_calls++;
 kinds[ticks]=kind;frames[ticks]=frame;colors[ticks]=OBJ_PALETTE[253];}
static void ram8w(unsigned a,unsigned v){assert(a==0x500a&&ncues<4);cues[ncues]=v;cue_ticks[ncues++]=ticks;}
static int battle_enemy_anchor_x(unsigned i){assert(!i);return 120;}
static int battle_party_center_x(unsigned i){return 24+i*48;}
static void battle_source_enemy_palette(const Ps4SourceEnemyDesc*d){memcpy(obj_palette+240,ps4_source_enemy_palettes[d->palette_id],32);}
static void hw_hide_obj(unsigned i){assert(i>=64&&i<112);obj_kind[i]=0;}
static void battle_source_ability_hide_all(void){for(unsigned i=0;i<battle_source_effect_active_oam;i++)hw_hide_obj(64+i);battle_source_effect_active_oam=kind=0;}
static int battle_effect_upload(const uint8_t*src,unsigned bytes,unsigned tile){assert(bytes%32==0&&tile>=768&&tile+bytes/32<=1024);
 uintptr_t offset=(uintptr_t)src-(uintptr_t)ps4_source_ability_bank;tile_frame[tile]=255;
 if(offset<1112346){const Ps4SourceAbilityAnim*a=0;for(unsigned i=0;i<132;i++)if(ps4_source_ability_anims[i].source_table==0x25BE8E){a=&ps4_source_ability_anims[i];break;}assert(a&&a->part_count==1);
  const Ps4SourceAbilityPart*p=&ps4_source_ability_parts[a->first_part];assert(offset>=p->off&&offset<p->off+p->frame_bytes*6&&bytes==p->frame_bytes);tile_frame[tile]=(offset-p->off)/p->frame_bytes;
 }return 1;
}
static void battle_source_obj_prebound4(unsigned oi,int x,int y,int ox,int oy,unsigned geom,unsigned tile){(void)ox;(void)oy;(void)geom;assert(oi>=64&&oi<112);
 if(y==102){assert(x>=24&&x<=216&&((x-24)%48)==0&&tile_frame[tile]<6);obj_kind[oi]=2;obj_frame[oi]=tile_frame[tile];}
 else{assert(x==120&&y==62&&tile_frame[tile]==255);obj_kind[oi]=1;}
}
static void battle_source_enemy_frame_position(const Ps4SourceEnemyAnim*a,unsigned f,const int*x,const int*y,unsigned n,unsigned upload){(void)a;(void)upload;assert(f==0&&n==1&&x[0]==120&&y[0]==62);kind=1;}
static void battle_skill_source_anim_start_audio(unsigned skill,unsigned table){(void)skill;(void)table;assert(0);}
static void battle_skill_source_audio_tick(unsigned skill,unsigned elapsed){(void)skill;(void)elapsed;assert(0);}
static void battle_source_ability_frame(const Ps4SourceAbilityAnim*a,unsigned f,const int*x,const int*y,unsigned n){assert(a->source_table==0x25BE8E&&f<6&&n==targets);for(unsigned i=0;i<n;i++)assert(x[i]>=24&&x[i]<=216&&y[i]==102);child_calls++;kind=2;frame=f;}
static int battle_enemy_source_action(struct BattleLiteActor*e,unsigned i,unsigned a){(void)e;(void)i;(void)a;return 77;}
static int battle_source_enemy_rimit_action(const struct BattleLiteActor*p,unsigned n,struct BattleLiteActor*e,unsigned v,unsigned own){(void)p;(void)n;(void)e;(void)v;(void)own;return 77;}
static int battle_source_enemy_earthquake_action(struct BattleLiteActor*e,unsigned own){(void)e;(void)own;return 77;}
static int battle_source_igglanova_fission_action(struct BattleLiteActor*e,unsigned i,const Ps4SourceEnemyDesc*d){(void)e;(void)i;(void)d;return 77;}
static int battle_source_enemy_generated_action(const struct BattleLiteActor*p,unsigned n,struct BattleLiteActor*e,unsigned i,unsigned v,const Ps4SourceEnemyDesc*d,unsigned own){(void)p;(void)n;(void)e;(void)i;(void)v;(void)d;(void)own;return 77;}
'''
pre2+='const uint8_t ps4_source_ability_bank['+str(len(bank))+']={'+','.join(f'[{i}]={v}' for i,v in sorted(holds.items()))+'};\n'
pre2+='const uint8_t ps4_source_enemy_bank['+str((R/'gba/source_enemy_effects.bin').stat().st_size)+']={0};\n'
pre2+=r'''
/* Corrosion remains the native explicit controller. The four recorded paths
 * are tested independently by check_r46_recorded_runtime.py. */
static int r46_play_enemy_clip(const struct BattleLiteActor*p,unsigned pn,struct BattleLiteActor*e,unsigned ei,uint8_t ability,unsigned victim){
 (void)p;(void)pn;(void)e;(void)ei;(void)victim;assert(ability==0x4d);return 0;
}
'''
body2=''.join(fn(n) for n in ['md2gba','battle_source_y_to_gba','battle_source_ability_find_anim','battle_source_df3_frame','battle_source_df3_corrosion','battle_enemy_source_action_full'])
main2=r'''
int main(void){
 for(unsigned mask=0;mask<32;mask++){
  struct BattleLiteActor party[5],enemies[4]={{0x84,500}};targets=0;
  for(unsigned i=0;i<5;i++){party[i].id=i;party[i].hp=(mask>>i)&1;targets+=party[i].hp;}
  ticks=idles=kind=frame=child_calls=ncues=0;memset(kinds,0,sizeof kinds);memset(frames,0,sizeof frames);
  assert(battle_enemy_source_action_full(party,5,enemies,0,0x4d,0,0)==1);
  assert(enemies[0].hp==500);for(unsigned i=0;i<5;i++)assert(party[i].hp==((mask>>i)&1));
  assert(ticks==(targets?69:51)&&ticks==idles&&!kind&&child_calls==(targets?6:0));
  assert(ncues==2&&cues[0]==0xcd&&cue_ticks[0]==0&&cues[1]==0xc2&&cue_ticks[1]==35);
  for(unsigned t=1;t<=35;t++){assert(kinds[t]==(((t-1)&3)<2?1:0));assert(colors[t]==md2gba(t<12?0x468:(t<24?0x246:0x24)));}
  if(targets){static const unsigned starts[6]={36,38,41,44,48,51};
   for(unsigned f=0;f<6;f++)for(unsigned t=starts[f];t<(f<5?starts[f+1]:54);t++)assert(kinds[t]==(2|(((t-1)&3)<2?1:0))&&frames[t]==f);
  }
  unsigned start=targets?54:36;
  for(unsigned t=0;t<16;t++){assert(kinds[start+t]==(t<15&&((start+t-1)&3)<2?1:0));assert(colors[start+t]==md2gba(t<7?0x246:0x468));}
 }
 puts("PASS production DF3 Corrosion: all 32 living-target masks, 35 charge + 18 child + 16 fade ticks, concurrent parent/children and continuous blink phase, 6 source holds, palette/audio cues, unchanged HP");
}
'''
log+=run('r46_corrosion_probe',pre2+body2+main2,[R/'gba/source_ability_effects.c',R/'gba/source_enemy_effects.c'])
(O/'R46_PRESENTATION_CHECK.log').write_text(log)
(O/'R46_PRESENTATION_EVIDENCE.json').write_text(json.dumps({'result':'PASS','damage_windows':18018,'font_edge_cases':1024,'intro_lines':16,'raw_effect_geometry_cases':24,'part_positions':4056,'enemy_selections':153,'boss_selections':56,'flag_descriptions':768,'flag_edits':1536,'cancelled_edits':1536,'corrosion_target_masks':32,'scope':'Extracted production C, ASan/UBSan. Native hardware/VDP, sound output, actor composition and input are fixtures. Original art pixels have an independent separate comparison. Real-device visuals and full-game completion remain unverified.'},indent=2)+'\n')
