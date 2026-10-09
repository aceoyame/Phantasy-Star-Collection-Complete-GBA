#!/usr/bin/env python3
"""Independent ROM map/nibble oracle + production Zio controller sanitizer probe."""
from pathlib import Path
import json,re,struct,subprocess,os
from md_compression import nemesis_decompress,enigma_decompress
R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts'
rom=(R/'references/roms/ps4built.bin').read_bytes();bank=(R/'gba/r46_zio_story.bin').read_bytes();h=(R/'gba/r46_zio_story_data.h').read_text();e=json.loads((O/'R46_CP15_ZIO_ASSETS.json').read_text())
arrays=re.findall(r'=\{\n(.*?)\n\};',h,re.S)
parse=lambda a:[[int(x.strip().rstrip('u'),0) for x in row.split(',')] for row in re.findall(r'\{(.*?)\}',a)]
parts,anims=map(parse,arrays);pixels=0
# Independent inverse sprite sampler: no generator render/pack helper is used.
for ai,(first,n,fc,ho,table) in enumerate(anims):
 art=nemesis_decompress(rom[0x21EDD0:])
 if ai in (1,2,5,6):art=nemesis_decompress(rom[{1:0x21F3B8,2:0x21F3B8,5:0x21FDBA,6:0x21F820}[ai]:])
 frames=e['animations'][ai]['frames']
 assert list(bank[ho:ho+fc])==e['animations'][ai]['holds']
 for part in parts[first:first+n]:
  off,fb,geom,pal,ox,oy,pad=part;shape,size=geom&3,geom>>2
  w=((8,16,32,64),(16,32,32,64),(8,8,16,32))[shape][size]
  height=((8,16,32,64),(8,8,16,32),(16,32,32,64))[shape][size]
  for fi,address in enumerate(frames):
   a=int(address,16);num=rom[a]+1;rows=[]
   for i in range(num):
    b=rom[a+1+i*6:a+7+i*6];yy=int.from_bytes(b[:1],'big',signed=True);xx=int.from_bytes(b[4:5],'big',signed=True);tw=(b[1]>>2&3)+1;th=(b[1]&3)+1;word=int.from_bytes(b[2:4],'big')
    base=1 if ai==6 else 2
    rows.append((xx,yy,tw,th,word))
   for y in range(height):
    for x in range(w):
     xx,yy=ox+x,oy+y;expected=0
     for rx,ry,tw,th,word in rows:
      dx,dy=xx-rx,yy-ry
      if not(0<=dx<tw*8 and 0<=dy<th*8):continue
      if word&0x800:dx=tw*8-1-dx
      if word&0x1000:dy=th*8-1-dy
      ti=(word&2047)+(dx//8)*th+dy//8;at=ti*32+(dy%8)*4+(dx%8)//2
      assert at<len(art),(ai,hex(ti),len(art))
      ci=(art[at]>>(0 if dx%2 else 4))&15
      if ci:
       expected=ci if (base+(word>>13&3))&3==pal else 0
       break
     at=off+fi*fb+((y//8)*(w//8)+x//8)*32+(y%8)*4+(x%8)//2
     actual=bank[at]>>(4 if x%2 else 0)&15
     assert actual==expected,(ai,fi,part,x,y,actual,expected)
     pixels+=1
# Plane-B independent sample covers all 38,400 projected pixels.
bgoff=int(re.search(r'BG_OFF (\d+)',h)[1]);art=nemesis_decompress(rom[0x21D656:]);cells=enigma_decompress(rom[0x21E656:])
for y in range(160):
 for x in range(240):
  sx,sy=x*4//3,y*7//5;word=cells[sy//8*64+sx//8];px=sx%8;py=sy%8
  if word&0x800:px=7-px
  if word&0x1000:py=7-py
  ci=art[(word&2047)*32+py*4+px//2]>>(0 if px%2 else 4)&15
  assert bank[bgoff+y*240+x]==240+ci
pixels+=38400
probe=r'''
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "r46_zio_story_data.h"
#define BATTLE_MAX_ENEMIES 4u
#define MODE4 4u
#define BG2_ENABLE 0x400u
#define OBJ_1D 0x40u
#define OBJ_ENABLE 0x1000u
struct BattleLiteActor{uint16_t hp,max_hp,tp,max_tp;uint8_t id,status;};
struct BattleCommandLite{uint8_t type,id,target,inv_slot;};
static unsigned battle_enemy_count=1,battle_source_effect_active_oam,battle_generic_pending_mask,battle_palette_hw_dirty;
static uint8_t arena[65536],fb[38400];static uint8_t*field_pause_fb=fb;
static uint16_t field_pause_pal[256],bgpal[256],objpal[256],oam[512],display;
#define BG_PALETTE bgpal
#define OBJ_PALETTE objpal
#define OAM16 oam
#define REG_DISPCNT display
static uint8_t source_rom[0x300000];
static unsigned ticks,cues,lastcue,cuetick,presents,uploads,max_tile,iterations;
static unsigned cue_ids[16],cue_ticks[16];
static uint8_t ram8r(uint16_t p){return arena[p];}
static void ram8w(uint16_t p,uint8_t q){arena[p]=q;if(p==0x500a){assert(cues<16);cue_ids[cues]=q;cue_ticks[cues]=ticks;cues++;lastcue=q;cuetick=ticks;}}
static uint16_t ram16r(uint16_t p){return arena[p]*256u+arena[p+1];}
static void ram16w(uint16_t p,uint16_t q){arena[p]=q>>8;arena[p+1]=q;}
static uint8_t rom8(uint32_t p){assert(p<sizeof source_rom);return source_rom[p];}
static uint16_t rom16(uint32_t p){return rom8(p)*256u+rom8(p+1);}
static uint16_t md2gba(uint16_t c){static const unsigned t[8]={0,4,9,13,18,22,27,31};return t[c>>1&7]|t[c>>5&7]<<5|t[c>>9&7]<<10;}
static void dma_copy32(const void*s,void*d,unsigned n){memcpy(d,s,n);}
static int battle_enemy_source_center_x(unsigned i){assert(i<4);return 120;}
static int battle_source_y_to_gba(unsigned y){return ((int)y-128)*5/7;}
static void hw_hide_obj(unsigned i){assert(i>=64&&i<112);oam[i*4]=160;}
static void battle_source_obj_prebound4(unsigned i,int x,int y,int ox,int oy,unsigned geom,unsigned tile){assert(i>=64&&i<112);assert(tile>=512&&tile<1024);oam[i*4+2]=tile;}
static int battle_effect_upload(const uint8_t*s,unsigned n,unsigned tile){assert(s>=r46_zio_bank&&s+n<=r46_zio_bank+135728);assert(tile+n/32<=1024);if(tile+n/32>max_tile)max_tile=tile+n/32;uploads++;return 1;}
static void battle_action_idle_after_wait(struct BattleLiteActor*e){iterations++;}
static void battle_present_action(struct BattleLiteActor*p,unsigned n,struct BattleLiteActor*e,unsigned target,int enemy,unsigned actor,const struct BattleCommandLite*c,unsigned combo){presents++;}
static void battle_source_ability_hide_all(void){for(unsigned i=64;i<112;i++)hw_hide_obj(i);battle_source_effect_active_oam=0;}
static void battle_ui_discard_staged_anim(void){battle_generic_pending_mask=0;}
static void battle_damage_overlay_clear(void){}
static void battle_enemy_cue_cancel(void){}
static void ps4_audio_stop(void){}
static int ps4_audio_play(unsigned s){return 1;}
#include "r46_zio_scene.h"
static void waitvb(void){ticks++;r46_zio_palette_tick();}
#include "r46_zio_controller.h"
int main(int argc,char**argv){assert(argc==2);FILE*f=fopen(argv[1],"rb");assert(f&&fread(source_rom,1,sizeof source_rom,f)==sizeof source_rom);fclose(f);
 struct BattleLiteActor p[5]={{23,100,9,20,0,0},{0,100,5,20,1,0x44},{31,100,7,20,2,0},{90,100,3,20,3,0},{43,100,2,20,5,0}},e[4]={{32767,32767,0,0,0x98,0}};
 uint8_t before_p[sizeof p],before_e[sizeof e];memcpy(before_p,p,sizeof p);memcpy(before_e,e,sizeof e);
 r46_zio_reset(e);ram8w(0xecec,0xa9);unsigned durations[]={125,29,0,98,71};
 for(unsigned stage=0;stage<5;stage++){unsigned t=ticks,pr=presents;assert(r46_zio_story_action(p,5,e,0)==1);assert(ram8r(0xee98)==stage+1);assert(ticks-t==durations[stage]);assert(presents-pr==(stage!=2));assert(!memcmp(before_p,p,sizeof p)&&!memcmp(before_e,e,sizeof e));assert(r46_zio_story_exited(e)==(stage==4));if(stage==0)assert(ram16r(0xfb00)==0&&ram16r(0xfb06)==0xeca);}
 assert(ram16r(0x4144)==2);assert(ram16r(0xef00)==8);assert(!ram8r(0xee68)&&!ram16r(0x2a90)&&!ram16r(0x2aa0));assert(display==(MODE4|OBJ_1D));assert(!battle_generic_pending_mask&&!battle_palette_hw_dirty);assert(iterations==ticks);assert(ticks==323);
 assert(cues==7);unsigned ids[]={0xc4,0xcd,0xc2,0xc6,0xcb,0xea,0xa9};unsigned times[]={28,62,153,164,203,280,323};for(unsigned i=0;i<cues;i++){assert(cue_ids[i]==ids[i]);assert(cue_ticks[i]==times[i]);}
 // Fallback to slot zero is intentional, even if incapacitated. Later Zio
 // IDs and generic ability $70 must never enter this story controller.
 for(unsigned id=0x8b;id<=0x8c;id++){e[0].id=id;r46_zio_reset(e);unsigned t=ticks;assert(!r46_zio_story_action(p,5,e,0));assert(ticks==t&&!ram8r(0xee98)&&!r46_zio_story_exited(e));}
 p[1].id=6;e[0].id=0x98;r46_zio_reset(e);ram8w(0xee98,4);assert(r46_zio_story_action(p,5,e,0));assert(ram16r(0x4144)==0);
 // Dedicated source background must restore clipped rows from the same bank.
 r46_zio_reset(e);ram8w(0xee98,1);memset(fb,192+7,sizeof fb);r46_zio_mask_plane();assert(fb[12*240+120]==r46_zio_bank[R46_ZIO_BG_OFF+12*240+120]);assert(fb[60*240+100]==199);ram16w(0xd000,0x908);ram8w(0xd014,1);r46_zio_mask_plane();assert(fb[60*240+120]==r46_zio_bank[R46_ZIO_BG_OFF+60*240+120]);assert(fb[60*240+100]==199);
 printf("PASS production Zio stages: 323 authored waits; exact cues; unchanged HP/TP/status; Alys/dead-Alys/fallback targeting; later variants isolated; max OBJ tile %u\n",max_tile);
}
'''
(O/'r46_zio_story_probe.c').write_text(probe)
(O/'r46_zio_probe_bank.s').write_text('.section .rodata\n.global r46_zio_bank\nr46_zio_bank:\n.incbin "'+str(R/'gba/r46_zio_story.bin')+'"\n.section .note.GNU-stack,"",@progbits\n')
subprocess.run([os.environ.get('CC','cc'),'-g','-O1','-fsanitize=address,undefined','-I'+str(R/'gba'),str(O/'r46_zio_story_probe.c'),str(O/'r46_zio_probe_bank.s'),'-o',str(O/'r46_zio_story_probe')],check=True)
subprocess.run([str(O/'r46_zio_story_probe'),str(R/'references/roms/ps4built.bin')],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
(O/'R46_CP15_ZIO_ORACLE.json').write_text(json.dumps({'independent_sprite_and_background_pixels':pixels,'authored_controller_waits':323,'stage_waits':[125,29,0,98,71],'unchanged_actor_state':True,'Alys_target_even_when_incapacitated':True,'no_Alys_fallback_slot':0,'later_Zio_variants_isolated':True,'ASan_UBSan':'PASS','original_VDP_capture_or_hardware_timing_verified':False},indent=2)+'\n')
print('PASS independent source map/nibble oracle:',pixels,'pixels')
