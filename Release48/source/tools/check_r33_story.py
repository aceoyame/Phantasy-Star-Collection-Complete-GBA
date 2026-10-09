#!/usr/bin/env python3
"""Probe source clocks, chest-to-battle handoff and immutable OBJ bounds."""
from pathlib import Path
import os,subprocess
R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts'
# Reuse the already generated sanitized R31 actor test environment, without
# its main. This exercises the actual native headers with original ROM tables.
s=(O/'r31_story_probe.c').read_text().split('int main(')[0]
s=s.replace('static unsigned punch_cues;', 'static unsigned fragments_checked,fire_tick;static unsigned punch_cues;')
s=s.replace('if(a==0x500A){','if(a==0x500A){if(q==0xC2)fire_tick=tick;')
s=s.replace('ram16w(0xC034+i*64,y);field_event_sync_party_carrier', 'ram16w(0xC034+i*64,y);ram16w(0xC038+i*64,x);ram16w(0xC03A+i*64,y);field_event_sync_party_carrier')
s=s.replace('r31_actor_update();hw_update_objects(x,y);waitvb();',r'''
 assert(tick<10000);
 if(field_obj_count>2&&field_objs[2].type==0x1FC&&ram16r(0xC39C)==39){
  for(unsigned q=0;q<18;q++){uint16_t b=0xC380+q*64;unsigned a=0x6EBD2+q*16;
   assert(ram16r(b)==0x1FC&&ram16r(b+6)==q*4);
   assert(ram16r(b+0x30)==rom16(a)&&ram16r(b+0x34)==rom16(a+2));
   assert(ram32r(b+0x20)==(uint32_t)((int32_t)(int16_t)rom16(a+4)*256));
   assert(ram32r(b+0x24)==(uint32_t)((int32_t)(int16_t)rom16(a+6)*256));
  }fragments_checked++;
 }
 r31_actor_update();hw_update_objects(x,y);waitvb();''')
s+=r'''
static unsigned battles;
static void field_run_map_updates(void){}
static void field_event_redraw_explicit(unsigned x,unsigned y){hw_update_objects(x,y);}
static int field_event_run_battle_request(uint8_t b,unsigned*x,unsigned*y){
 (void)x;(void)y;assert(b==5&&flags[0x69]);assert(field_objs[0].facing==12&&(ram8r(0xC302)&4));battles++;return 1;
}
#include "r33_story_events.h"
int main(int argc,char**argv){assert(argc==2);FILE*f=fopen(argv[1],"rb");assert(f&&fread(rom,1,sizeof rom,f)==sizeof rom);fclose(f);
 unsigned x=0x148,y=0x128;
 reset(0);assert(!r33_event_psyco_wand_chest(&x,&y));assert(!tick&&!flags[0x69]&&!battles);
 reset(0);ram16w(0xC034,0x180);r31_actor_spawn(0,0x1F8,0x1D0,0x168,0,0x2E6);
 r31_actor_update();assert(ram8r(0xC302)&2);r31_actor_update();assert(!(ram8r(0xC302)&2));
 ram16w(0xC034,0x170);r31_actor_update();assert(ram8r(0xC302)&2);
 ram16w(0xC034,0x1D1);r31_actor_update();assert(ram8r(0xC302)&2);
 field_objs[0].facing=4;ram16w(0xC306,4);
 for(unsigned n=1;n<=11;n++){r31_actor_update();assert((n<11)==(field_objs[0].type!=0));if(n<11)assert(!!(ram8r(0xC302)&2)==!!((11-n)&2));}
 reset(0);r31_actor_spawn(2,0x1FC,0x1F1,0x160,0,0x2E6);ram32w(0xC3A0,0xC000);ram32w(0xC3A4,0xFFFF0000);
 for(unsigned n=1;n<39;n++){r31_actor_update();assert(ram32r(0xC3B0)==0x1F10000+n*0xC000);assert(ram32r(0xC3B4)==0x1600000-n*0x10000);assert(ram16r(0xC39C)==39-n);}
 r31_actor_update();assert(!field_objs[2].active&&!ram16r(0xC380));
 reset(0);fire_tick=0;r31_actor_spawn(0,0x20C,0x1D8,0x160,0,0x2E6);
 for(unsigned n=1;n<=244;n++){
  r31_actor_update();tick=n;
  if(n<129)assert(field_objs[0].facing==0);
  if(n==129)assert(field_objs[0].facing==4&&!(ram8r(0xC302)&6));
  if(n==157)assert(field_objs[0].facing==8&&fire_tick==156);
  if(n==193)assert(field_objs[0].facing==12&&!(ram8r(0xC302)&6));
  if(n<244&&field_objs[0].facing==12)assert(!(ram8r(0xC302)&4));
 }
 assert(field_objs[0].facing==12&&(ram8r(0xC302)&4));
 reset(0);ram16w(0xC034,0x180);chaz.y=0x180;ram8w(0xF40B,4);ram8w(0xF40C,5);ram8w(0xF40D,6);ram8w(0xF40E,3);r31_actor_spawn(0,0x1F8,0x1D0,0x168,0,0x2E6);
 assert(r33_event_psyco_wand_chest(&x,&y));assert(battles==1&&fragments_checked==1&&dialogues==2&&resumes==1);assert(max_tiles<=768);
 for(unsigned i=2;i<20;i++)assert(!field_objs[i].active);
 assert(ram16r(0xC138)==0x1E0&&ram16r(0xC13A)==0x170);assert(flags[0x69]);
 puts("PASS R33 barrier flicker/break, 18 source-stride splinters, 244-update guardian reveal, dialogue/battle handoff and OBJ bounds (movement/dialogue/battle/display stubbed)");
}
'''
c=O/'r33_story_probe.c';c.write_text(s)
subprocess.run(['gcc','-std=c11','-O1','-g','-fsanitize=address,undefined','-I'+str(R/'gba'),str(c),'-o',str(O/'r33_story_probe')],check=True)
subprocess.run([str(O/'r33_story_probe'),str(R/'references/roms/ps4built.bin')],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
