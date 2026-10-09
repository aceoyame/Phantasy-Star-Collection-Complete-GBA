#!/usr/bin/env python3
"""ASan/UBSan production weapon controller probe with explicit VDP fixtures."""
from pathlib import Path
import re,subprocess,json,os
R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts';s=(R/'gba/title_runtime_v06.c').read_text()
def fn(name):
 m=re.search(r'^static [^\n]*\b'+name+r'\([^;\n]*\)\{',s,re.M);assert m,name
 start=m.start();end=m.end();depth=1
 while depth:
  if s[end]=='{':depth+=1
  elif s[end]=='}':depth-=1
  end+=1
 return s[start:end]+'\n'
funcs=['battle_r46_weapon_pixels','battle_r46_weapon_duration','battle_r46_weapon_frame_at','battle_r46_body_palette','battle_r46_weapon_palette','battle_r46_weapon_draw','battle_r46_melee_source_attack','battle_r46_hit_exempt','battle_r46_gun_reaction_tick','battle_r46_gun_source_attack']
code=''.join(fn(n) for n in funcs).replace('static void battle_r46_weapon_draw(','static void battle_r46_weapon_draw_impl(')
pre=r'''
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "r46_weapon_effects.h"
#include "source_enemy_effects.h"
#include "source_combo_effects.h"
#define BATTLE_MAX_ENEMIES 4
#define BATTLE_SOURCE_ABILITY_OBJ0 64u
#define BATTLE_SOURCE_ABILITY_OBJN 48u
#define BATTLE_SOURCE_ABILITY_PALBANK 15u
#define PS4_PRECOMP_PARTY_WREN_POSE_LOAD_GUN_INDEX 3u
#define PS4_PRECOMP_PARTY_DEMI_POSE_LOAD_GUN_INDEX 3u
#define PS4_PRECOMP_PARTY_WREN_POSE_FIRE_GUN_INDEX 4u
#define PS4_PRECOMP_PARTY_DEMI_POSE_FIRE_GUN_INDEX 4u
struct BattleLiteActor {uint8_t id;uint16_t hp;};
struct BattleCommandLite {uint8_t type,id,target,inv_slot;};
struct R46WeaponTrack {uint16_t anim,start;int16_t x,y;};
extern const uint8_t source_rom[];
static uint8_t arena[65536],battle_enemy_placement[4]={16,23,30,37};
static unsigned battle_enemy_count=4,battle_event_index=1,battle_source_effect_active_oam;
static uint8_t battle_party_visible_mask=31,battle_party_pose_actor=255,battle_party_pose_index=255,battle_party_x_override_actor=255;
static uint8_t battle_slasher_hit_mask;
static uint8_t battle_physical_flash_mask,battle_palette_hw_dirty;
static unsigned battle_generic_pending_mask;
static uint16_t field_pause_pal[256];
static int16_t battle_party_x_override_value=123;
static uint16_t palette[256],oam[512];
#define OAM16 oam
#define OBJ_PALETTE palette
static unsigned ticks,cues,draws,max_oam,max_bytes,uploaded_bytes,last_oi,cases;
static unsigned gun_active,hit_flags,impact_mask,gun_item,launch_count,impact_count,foi_count,gun_origin,foi_ticks[4],zero_count,zero_ticks[3];
static uint8_t ram8r(uint16_t p){return arena[p];}
static void ram8w(uint16_t p,uint8_t v){assert(p==0x500a);cues++;if(gun_active&&v==0xb6)launch_count++;if(gun_active&&v==0xbe){assert(foi_count<4);foi_ticks[foi_count++]=ticks-gun_origin;}if(gun_active&&!v){assert(zero_count<3);zero_ticks[zero_count++]=ticks-gun_origin;}}
static uint8_t rom8(uint32_t p){assert(p<0x300000);return source_rom[p];}
static uint16_t rom16(uint32_t p){return (uint16_t)(rom8(p)*256u+rom8(p+1));}
static uint16_t battle_char_record(unsigned id){return (uint16_t)(0x1000u+id*256u);}
static unsigned battle_item_type(uint8_t item){return item?rom8(0x2a8e28+(item-1)*22u+10):0;}
static uint16_t md2gba(uint16_t c){static const unsigned t[8]={0,4,9,13,18,22,27,31};return t[c>>1&7]|t[c>>5&7]<<5|t[c>>9&7]<<10;}
static int battle_source_x_to_gba(unsigned x){return 120+((int)x-0x120)*3/4;}
static int battle_source_y_to_gba(unsigned y){return ((int)y-0x80)*5/7;}
static void dma_copy32(const void*s,void*d,unsigned n){assert(n==32);memcpy(d,s,n);}
static void hw_hide_obj(unsigned i){assert(i>=64&&i<112);}
static void waitvb(void){ticks++;if(battle_palette_hw_dirty)memcpy(palette,field_pause_pal,sizeof palette);battle_palette_hw_dirty=0;}
static void battle_obj_sync(const struct BattleLiteActor*e,unsigned ch){(void)e;(void)ch;}
static void battle_action_hold(struct BattleLiteActor*e,unsigned n){(void)e;ticks+=n;}
static void battle_action_idle_after_wait(struct BattleLiteActor*e){(void)e;}
static void battle_present_action(const struct BattleLiteActor*p,unsigned n,struct BattleLiteActor*e,unsigned target,unsigned ae,unsigned actor,const struct BattleCommandLite*c,unsigned combo){(void)p;(void)n;(void)e;(void)target;(void)ae;(void)actor;(void)c;(void)combo;ticks++;}
static void battle_source_ability_hide_all(void){battle_source_effect_active_oam=0;}
static int battle_effect_upload(const uint8_t*src,unsigned n,unsigned tile){assert(src);assert(n&&!(n&31));assert(tile>=(battle_event_index==0?768:512)&&tile+n/32<=1024);uploaded_bytes+=n;return 1;}
static void battle_source_obj_prebound4(unsigned oi,int x,int y,int ox,int oy,unsigned geom,unsigned tile){OAM16[oi*4+2]=(uint16_t)(tile|(1u<<10)|(15u<<12));(void)ox;(void)oy;assert(oi>=64&&oi<112&&geom<16&&(geom&3)<3&&tile<1024);assert(x>-96&&x<336&&y>=0&&y<224);last_oi=oi+1;}
static void battle_r46_weapon_draw(const struct R46WeaponTrack*,unsigned,unsigned);
'''
wrapper=r'''
static void battle_r46_weapon_draw(const struct R46WeaponTrack*t,unsigned count,unsigned elapsed){
 if(gun_active&&elapsed==1){
  gun_origin=ticks-1;unsigned all=battle_item_type(gun_item)==4,wid=rom8(0x883a+gun_item),shots=all?4:1;
  unsigned launches=shots*(wid==0x1c?2:1),ordinal=0;assert(count>=launches);impact_count=count-launches;
  for(unsigned i=0;i<battle_enemy_count;i++)if(impact_mask&(1u<<i)){
   if(!(hit_flags&15))continue;
   unsigned k=launches+ordinal*(r46_gun_map[gun_item][2]<R46_WEAPON_ANIM_COUNT?2:1);
   assert(t[k].anim==r46_gun_map[gun_item][1]&&t[k].start==13+ordinal*8);
   if(r46_gun_map[gun_item][2]<R46_WEAPON_ANIM_COUNT)assert(t[k+1].anim==r46_gun_map[gun_item][2]&&t[k+1].start==21+ordinal*8);
   ordinal++;
  }
  assert(count==launches+ordinal*(r46_gun_map[gun_item][2]<R46_WEAPON_ANIM_COUNT?2:1));
 }
 unsigned expect=0;assert(count<=12);
 for(unsigned i=0;i<count;i++)if(elapsed>=t[i].start){
  assert(t[i].anim<R46_WEAPON_ANIM_COUNT);const Ps4SourceAbilityAnim*a=&r46_weapon_anims[t[i].anim];unsigned age=elapsed-t[i].start,frame=0;
  for(;frame<a->frame_count;frame++){unsigned hold=r46_weapon_bank[a->holds_off+frame];if(age<hold)break;age-=hold;}
  if(frame<a->frame_count)expect+=a->part_count;
 }
 assert(expect<=48);uploaded_bytes=0;last_oi=64;battle_r46_weapon_draw_impl(t,count,elapsed);assert(last_oi-64==expect);unsigned oi=64;
 for(unsigned i=0;i<count;i++)if(elapsed>=t[i].start){const Ps4SourceAbilityAnim*a=&r46_weapon_anims[t[i].anim];int f=battle_r46_weapon_frame_at(t[i].anim,elapsed-t[i].start);if(f>=0)for(unsigned j=0;j<a->part_count;j++){unsigned want=r46_weapon_parts[a->first_part+j].pad&1?14:15;assert((OAM16[oi++*4+2]>>12)==want);}}
 if(expect>max_oam)max_oam=expect;if(uploaded_bytes>max_bytes)max_bytes=uploaded_bytes;draws++;
}
static void one(unsigned id,unsigned rh,unsigned lh,unsigned gun,unsigned event,unsigned mask,unsigned flags){
 struct BattleLiteActor p[5]={{0,600},{0,600},{0,600},{0,600},{0,600}},e[4]={{0,500},{0,500},{0,500},{0,500}};
 p[2].id=id;struct BattleCommandLite c={1,0,0,255};memset(arena,0,sizeof arena);arena[battle_char_record(id)+0x4c]=rh;arena[battle_char_record(id)+0x4d]=lh;
 battle_event_index=event;battle_enemy_count=event?4:3;for(unsigned i=0;i<4;i++)e[i].hp=(mask>>i)&1?500:0;
 battle_slasher_hit_mask=(uint8_t)flags;gun_active=gun;hit_flags=flags;gun_item=rh;launch_count=impact_count=foi_count=zero_count=0;
 uint16_t pal_before[256];for(unsigned j=0;j<256;j++)field_pause_pal[j]=j*17;memcpy(pal_before,field_pause_pal,sizeof pal_before);
 impact_mask=mask&((1u<<battle_enemy_count)-1u)&(battle_item_type(rh)==4?15:1);
 if(!e[0].hp)return;uint8_t before[65536];memcpy(before,arena,sizeof arena);struct BattleLiteActor ps[5],es[4];memcpy(ps,p,sizeof p);memcpy(es,e,sizeof e);
 unsigned oldv=battle_party_visible_mask,oldpa=battle_party_pose_actor,oldp=battle_party_pose_index,oldx=battle_party_x_override_actor;int oldxx=battle_party_x_override_value;
 unsigned result=gun?battle_r46_gun_source_attack(p,5,e,0,2,&c):battle_r46_melee_source_attack(p,5,e,0,2,&c);assert(result==1);
 assert(!memcmp(ps,p,sizeof p)&&!memcmp(es,e,sizeof e)&&!memcmp(before,arena,sizeof arena));assert(battle_party_visible_mask==oldv&&battle_party_pose_actor==oldpa&&battle_party_pose_index==oldp&&battle_party_x_override_actor==oldx&&battle_party_x_override_value==oldxx);assert(!battle_source_effect_active_oam);cases++;
 if(gun){
  assert(launch_count==(battle_item_type(rh)==4?4:1));unsigned ordinal=0,cue=0;
  for(unsigned i=0;i<battle_enemy_count;i++)if(impact_mask&(1u<<i)){if(flags&(1u<<i)){assert(cue<foi_count&&foi_ticks[cue++]==13+ordinal*8+(r46_gun_map[rh][2]<R46_WEAPON_ANIM_COUNT?8:0));}ordinal++;}
  assert(cue==foi_count);if(!(flags&15))assert(!impact_count&&!foi_count);
  assert(zero_count==((flags&0xf0)?3:0));for(unsigned j=0;j<zero_count;j++)assert(zero_ticks[j]==foi_ticks[0]+4+j*4);
  assert(!battle_physical_flash_mask&&!memcmp(pal_before,field_pause_pal,sizeof pal_before));
 }
 gun_active=0;
}
int main(void){
 for(unsigned item=0;item<144;item++){
  battle_r46_weapon_palette(item);battle_r46_body_palette();unsigned sel=rom8(0x88f4+item);
  for(unsigned i=0;i<16;i++){
   unsigned body=i?rom16(0x76a4+(i-1)*2):0;assert(palette[224+i]==md2gba(body));
   unsigned expected=rom16(0x296300+i*2);
   if(sel&128)expected=body;
   else{if(i==9)expected=0xeee;if(i==10)expected=0;if(i==14)expected=0x600;if(i==15)expected=0x62e;if(sel&&i>=10&&i<=13)expected=rom16(0x8984+(sel-1)*8+(i-10)*2);}
   assert(palette[240+i]==md2gba(expected));
  }
 }
 unsigned ids[7]={0,2,3,4,5,8,10};
 for(unsigned i=0;i<7;i++){unsigned id=ids[i],two=id==0?8:(id==3||id==8?16:(id==4?21:0)),one=id==5?32:1;
  for(unsigned ev=0;ev<2;ev++){
   if(two)one(id,two,0,0,ev,15,15);
   if(id==3||id==8){one(id,0,16,0,ev,15,15);continue;}
   one(id,one,0,0,ev,15,15);one(id,0,one,0,ev,15,15);one(id,one,id==5?36:19,0,ev,15,15);
  }
 }
 for(unsigned item=1;item<144;item++)if(rom8(0x883a+item)>=0x16&&rom8(0x883a+item)<=0x1c)
  for(unsigned id=6;id<=7;id++)for(unsigned ev=0;ev<2;ev++)for(unsigned mask=1;mask<16;mask+=2)
   for(unsigned flags=0;flags<16;flags++)if(!(flags&~(mask&((1u<<(ev?4:3))-1u)&(battle_item_type(item)==4?15:1)))){
    one(id,item,0,1,ev,mask,flags);if(battle_item_type(item)==3&&flags)one(id,item,0,1,ev,mask,flags|16);
   }
 printf("PASS production weapon controllers: %u cases, %u draw ticks, max %u OAM / %u uploaded bytes; all resources/HP and presentation state restored\n",cases,draws,max_oam,max_bytes);
}
'''
# Avoid a C identifier colliding with the fixture function name.
wrapper=wrapper.replace('unsigned id=ids[i],two=', 'unsigned id=ids[i],two=').replace(',one=id==5?32:1;',',hand=id==5?32:1;').replace('one(id,one,','one(id,hand,').replace('one(id,0,one,','one(id,0,hand,')
f=O/'r46_weapon_runtime_probe.c';f.write_text(pre+code+wrapper)
asm=O/'r46_weapon_runtime_banks.s';lines=['.section .rodata']
for name,path in [('source_rom',R/'references/roms/ps4built.bin')]+[(f'ps4_source_{n}_bank',R/'gba'/f'source_{n}_effects.bin') for n in ['ability','enemy','combo']]+[('r46_weapon_bank',R/'gba/r46_weapon_effects.bin')]:lines+=['.balign 4',f'.global {name}',name+':',f'.incbin "{path}"']
lines+=['.section .note.GNU-stack,"",@progbits'];asm.write_text('\n'.join(lines)+'\n')
exe=O/'r46_weapon_runtime_probe';cmd=['gcc','-std=c11','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie','-I'+str(R/'gba'),str(f),str(asm),str(R/'gba/r46_weapon_effects.c'),'-o',str(exe)]
p=subprocess.run(cmd,capture_output=True,text=True);assert p.returncode==0,p.stderr
p=subprocess.run([str(exe)],capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS="detect_leaks=0"));(O/'R46_WEAPON_RUNTIME_CHECK.log').write_text(p.stdout+p.stderr);assert p.returncode==0,p.stdout+p.stderr
assert 'battle_r46_melee_source_attack(party,pn,enemies,enemy_target,actor,c)' in s and 'battle_r46_gun_source_attack(party,pn,enemies,enemy_target,actor,c)' in s
report={'result':'PASS','output':p.stdout.strip(),'scope':'Production weapon controllers/drawing under ASan/UBSan. All-miss suppression, mixed-hit art/cue distinction, occupied-target/plasma impact and Foi delays, launch counts and restored resources/state checked. Original asset pixels checked separately. DMA/VDP/actor composition/idle/input/audio output and physical outcome flags are fixtures. Gun reactions/critical palettes have separate source-counter/native probes. This probe checks state restoration and zero-index commands; exact parent-stage timing and full parity remain open.'};(O/'R46_WEAPON_RUNTIME_EVIDENCE.json').write_text(json.dumps(report,indent=2)+'\n');print(p.stdout,end='')
