#!/usr/bin/env python3
"""One focused host probe: boss/UI layering and glyph bounds, plus ROM asset gates."""
from pathlib import Path
import re,subprocess,os,struct,json
R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts';s=(R/'gba/title_runtime_v06.c').read_text()
def function(name):
 m=re.search(r'^static[^\n]*\b'+name+r'\([^;]*?\)\s*\{',s,re.M);assert m,name
 p=m.end();d=1
 while d:d+=(s[p]=='{')-(s[p]=='}');p+=1
 return s[m.start():p]+'\n'
pre='''#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "precomp_assets.h"
#include "r29_dialogue_font.h"
#define BATTLE_MAX_ENEMIES 4
#define BATTLE_PARTY_W 36u
#define BATTLE_PARTY_H 34u
typedef uint16_t battle_u16_alias;
struct BattleLiteActor {unsigned id,hp;};
static uint8_t field_pause_fb[240*160],ps4_precomp_assets[40000];
static uint16_t display[240*160/2];
static uint16_t field_pause_pal[256],bg_palette[256],obj_palette[256];
static uint8_t battle_palette_hw_dirty;
#define BG_PALETTE bg_palette
#define OBJ_PALETTE obj_palette
#define MODE4_VRAM16 display
static struct BattleLiteActor actors[5],enemies[4],*battle_ui_party=actors,*battle_ui_enemies=enemies;
static unsigned battle_enemy_count=4,battle_ui_party_count=5,battle_ui_actor,battle_ui_target;
static uint8_t battle_enemy_placement[4],battle_show_enemy_target;
static uint8_t battle_physical_flash_mask;
static unsigned battle_event_index=1u;
static uint8_t battle_ui_overlay_active,battle_generic_stage_ready;
static unsigned battle_ui_overlay_x0,battle_ui_overlay_y0,battle_ui_overlay_x1,battle_ui_overlay_y1,battle_generic_pending_mask;
static int battle_generic_stage_x0,battle_generic_stage_x1;
static unsigned battle_generic_stage_y0,battle_generic_stage_y1;
static int battle_compose_clip_active,battle_compose_clip_x0,battle_compose_clip_x1;
static unsigned battle_compose_clip_y0,battle_compose_clip_y1;
static unsigned boss_x0,boss_x1,boss_y0,boss_y1,commits;
static void dma_copy32(const void*s,void*d,unsigned n){assert(n%4==0);memcpy(d,s,n);}
static unsigned battle_background_source_index(void){return 0;}
static int r45_pd_background_compose(const struct BattleLiteActor*e){(void)e;return 0;}
static int battle_enemy_native_rect(unsigned i,int*x0,unsigned*y0,int*x1,unsigned*y1){(void)i;*x0=boss_x0;*x1=boss_x1;*y0=boss_y0;*y1=boss_y1;return 1;}
static int battle_enemy_native_origin(unsigned i,unsigned id,int*x,int*y){(void)i;(void)id;*x=boss_x0;*y=boss_y0;return 1;}
static int battle_enemy_draw_visible(const struct BattleLiteActor*p,unsigned i){return p[i].hp!=0;}
static int battle_party_center_x(unsigned i){return 24+i*48;}
static unsigned battle_src_y(unsigned tile){return tile*8*5/7;}
static void battle_draw_party_frame(unsigned i,unsigned id){(void)i;(void)id;}
static void battle_draw_enemy(unsigned i,unsigned id,int x,int y,unsigned l){(void)i;(void)id;(void)x;(void)y;(void)l;
 for(unsigned yy=battle_compose_clip_y0;yy<battle_compose_clip_y1;yy++)for(int xx=battle_compose_clip_x0;xx<battle_compose_clip_x1;xx++)field_pause_fb[yy*240+xx]=0xc5;}
static void battle_draw_party_hud(const struct BattleLiteActor*p,unsigned n,unsigned a){(void)p;(void)n;(void)a;}
static void battle_draw_damage_overlays(void){}
static void battle_draw_enemy_target_overlay(const struct BattleLiteActor*p,unsigned t){(void)p;(void)t;}
static uint8_t glyph_pixels[256*64],choice_pixels[64*32];
static void dialogue_obj_pixel(unsigned x,unsigned y,uint8_t c){assert(x<256&&y<64);glyph_pixels[y*256+x]=c;}
static void dialogue_choice_pixel(unsigned x,unsigned y,uint8_t c){assert(x<64&&y<32);choice_pixels[y*64+x]=c;}
'''
# Source asset macro offsets are replaced only in the small background carrier.
pre=pre.replace('#include "precomp_assets.h"','#define PS4_PRECOMP_BATTLE_BG_PIXELS_OFF 0u\n#define PS4_PRECOMP_BATTLE_BG_FRAME_SIZE 38400u')
body=''.join(function(n) for n in ['battle_compose_clip_set','battle_compose_clip_clear','battle_ui_overlay_clear','battle_ui_discard_staged_anim','battle_ui_overlay_set','battle_ui_restore_overlay_from_vram_rect','battle_ui_stage_generic_anim','battle_ui_commit_generic_anim','dialogue_obj_source_glyph','dialogue_choice_source_glyph'])
main='''int main(void){
 for(unsigned i=0;i<4;i++){enemies[i].id=i;enemies[i].hp=100;}
 const unsigned rects[4][4]={{0,0,240,120},{80,0,160,110},{0,8,56,130},{180,16,240,100}};
 const unsigned windows[4][4]={{0,0,174,22},{18,85,60,125},{1,3,239,41},{20,23,41,32}};
 for(unsigned b=0;b<4;b++)for(unsigned w=0;w<4;w++){
  memset(field_pause_fb,0,sizeof(field_pause_fb));memset(display,0x37,sizeof(display));
  boss_x0=rects[b][0];boss_y0=rects[b][1];boss_x1=rects[b][2];boss_y1=rects[b][3];
  const unsigned *r=windows[w];battle_ui_overlay_set(r[0],r[1],r[2],r[3]);
  assert(battle_ui_stage_generic_anim(15));battle_ui_commit_generic_anim();
  uint8_t*v=(uint8_t*)display;
  for(unsigned y=r[1];y<r[3];y++)for(unsigned x=r[0];x<r[2];x++)assert(v[y*240+x]==0x37);
 }
 battle_generic_pending_mask=15;battle_generic_stage_ready=1;battle_ui_discard_staged_anim();
 assert(!battle_generic_pending_mask&&!battle_generic_stage_ready&&!battle_ui_overlay_active);
 /* All encoded characters stay in one 6x11 cell on both OBJ carriers. */
 for(unsigned c=0;c<128;c++){
  memset(glyph_pixels,0,sizeof(glyph_pixels));memset(choice_pixels,0,sizeof(choice_pixels));
  dialogue_obj_source_glyph(24,8,c);dialogue_choice_source_glyph(12,16,c);
  for(unsigned y=0;y<64;y++)for(unsigned x=0;x<256;x++)if(glyph_pixels[y*256+x])assert(x>=24&&x<30&&y>=8&&y<19);
  for(unsigned y=0;y<32;y++)for(unsigned x=0;x<64;x++)if(choice_pixels[y*64+x])assert(x>=12&&x<18&&y>=16&&y<27);
 }
 puts("PASS boss/UI protection: 16 clipped body/window overlaps, including odd edges");
 puts("PASS canonical frame retires stale animation stage; 128 dialogue/choice glyph bounds");return 0;}
'''
(O/'R29_PRESENTATION_PROBE.c').write_text(pre+body+main)
subprocess.run(['cc','-O1','-fsanitize=address,undefined','-I'+str(R/'gba'),str(O/'R29_PRESENTATION_PROBE.c'),'-o',str(O/'r29_presentation_probe')],check=True)
subprocess.run([str(O/'r29_presentation_probe')],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
# The source-projected interior must include the cursor and every label pixel.
y0=15*8*5//7;bottom=(15+6)*8*5//7
for row in range(3):assert y0+7+row*8+7<=bottom
assert 'battle_ui_box_source(3u,15u,6u,6u' in function('battle_choose_main_option')
# Every source enemy, including every boss formation, has bounded immutable art.
h=(R/'gba/precomp_assets.h').read_text();defs={n:int(v) for n,v in re.findall(r'#define (\w+) (\d+)u',h)};blob=(R/'gba/precomp_assets.bin').read_bytes();mo=defs['PS4_PRECOMP_BATTLE_ENEMY_NATIVE_META_OFF']
for i in range(153):
 m=struct.unpack_from('<8I',blob,mo+i*32)
 for off,size in [(m[0],m[1]),(m[2],m[3]),(m[4],m[5]*2)]:assert 0<=off<=len(blob) and off+size<=len(blob),(i,m)
 assert m[6] and m[7],i
rom=(R/'references/roms/ps4built.bin').read_bytes();ids=set()
import sys
sys.path.insert(0,str(R/'tools'))
from md_compression import kosinski_decompress
boss=kosinski_decompress(rom[0x284F7C:]);off=0
for i in range(27):
 assert off+7<len(boss)
 n=boss[off+4];assert 1<=n<=4,(i,n)
 for k in range(n):ids.add(boss[off+7+k*2])
 while boss[off]!=255:off+=1
 off+=1
assert ids
assert all(i<153 for i in ids)
# Compare files directly; do not hash the accepted sound implementation/banks.
baseline=R.parents[2]/'r28_work/R28/source'
if baseline.exists():
 sound=[p for p in (R/'gba').rglob('*') if p.is_file() and ('audio_p134r' in p.parts or p.name.startswith('ps4_audio') or p.suffix=='.pcm')]
 for p in sound:
  old=baseline/p.relative_to(R)
  assert old.exists() and p.read_bytes()==old.read_bytes(),p
 print('PASS accepted R28 audio files unchanged:',len(sound))
print('PASS RUN glyphs inside border; all 153 enemy art records bounded; source boss roster',len(ids))
