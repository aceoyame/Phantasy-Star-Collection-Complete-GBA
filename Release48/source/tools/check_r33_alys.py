#!/usr/bin/env python3
"""Source dialogue seams and Alys scene reload/party state probe."""
from pathlib import Path
import os,re,subprocess
from md_compression import kosinski_decompress
R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts'
rom=(R/'references/roms/ps4built.bin').read_bytes()
entries=kosinski_decompress(rom[0x1E32A0:]).split(b'\xff')
assert entries[0x2C].count(b'\xf7')==3 and entries[0x2D].count(b'\xf7')==10
src=(R/'gba/title_runtime_v06.c').read_text()
def function(name):
 m=re.search(r'^static[^\n]*\b'+name+r'\([^;]*?\)\s*\{',src,re.M);assert m,name
 p=m.end();depth=1
 while depth:depth+=(src[p]=='{')-(src[p]=='}');p+=1
 return src[m.start():p]+'\n'
s=(O/'r31_story_probe.c').read_text().split('int main(')[0]
s=s.replace('static unsigned punch_cues;', 'static unsigned remaining_segments;static unsigned punch_cues;')
s=s.replace('ram16w(0xC034+i*64,y);field_event_sync_party_carrier', 'ram16w(0xC034+i*64,y);ram16w(0xC038+i*64,x);ram16w(0xC03A+i*64,y);field_event_sync_party_carrier')
s=re.sub(r'static void field_dialogue_show\(.*?\n',r'''static void field_dialogue_show(unsigned id,unsigned x,unsigned y){(void)x;(void)y;dialogues++;remaining_segments=id==0x43?1:(id==0x2C?3:(id==0x2D?10:0));field_dialogue_saved_cursor=remaining_segments?work_ram:0;field_dialogue_last_stop=remaining_segments?0xF7:0xFF;}
''',s)
s=re.sub(r'static void field_dialogue_resume_saved\(.*?\n',r'''static void field_dialogue_resume_saved(unsigned x,unsigned y){(void)x;(void)y;assert(remaining_segments);remaining_segments--;resumes++;field_dialogue_saved_cursor=remaining_segments?work_ram:0;field_dialogue_last_stop=remaining_segments?0xF7:0xFF;}
''',s)
s+=r'''
#define PS4_CHAR_BASE 0xF500u
#define PS4_MACROS 0xF444u
#define FIELD_DIALOGUE_CAP 16384u
static uint8_t field_dialogue[FIELD_DIALOGUE_CAP],field_custom_resume_event=255,field_custom_resume_phase,field_event_reload_pending;
static unsigned field_dialogue_len,reloads,tree;
static unsigned kos_decode(unsigned addr,uint8_t*dst,unsigned cap){(void)dst;(void)cap;tree=addr;return 100;}
static unsigned ps4_event_flag_test(unsigned id){return flags[id];}
static void field_palette_fade_in(void){}static void field_palette_fade_out(void){}
static void field_palette_all64_to_black(unsigned d){(void)d;}static void field_palette_copy_saved_to_live(void){}
static void field_cutscene_panels_clear(void){}
static void field_cutscene_panel_wait(unsigned p,unsigned n){assert(p==0x4A&&n==70);tick+=n;}
static void r33_map_wait(unsigned n,unsigned*x,unsigned*y){(void)x;(void)y;tick+=n+1;}
static int field_dialogue_resume_saved_keep_panels(unsigned x,unsigned y){field_dialogue_resume_saved(x,y);return 1;}
static void field_cutscene_reload(uint16_t m,uint16_t prev,uint16_t x,uint16_t y,uint16_t f,uint16_t a){
 ram16w(0xEC28,m);ram16w(0xEC2A,prev);ram16w(0xEC48,x);ram16w(0xEC4A,y);ram16w(0xEC44,f);ram16w(0xEC46,a);ram8w(0xEC4E,ram8r(0xEC4E)&~8u);field_event_reload_pending=1;reloads++;
}
'''
for name in ['ps4_macro_record_slot','ps4_macro_write','field_event_macro_remove_char','field_event_macro_add_attack']:s+=function(name)
s+=r'''
#include "r33_alys_cutscene.h"
static void incoming(unsigned saya){
 reset(0);flags[0x12]=saya;field_custom_resume_event=255;field_custom_resume_phase=field_event_reload_pending=reloads=tree=remaining_segments=0;
 uint8_t ids[5]={0,1,2,4,5};for(unsigned n=0;n<5;n++){ram8w(0xF40A+n,ids[n]);ram8w(0xC00F+n*64,0xA0+n);ps4_macro_write(0,ids[n],1,0);}
 ram8w(PS4_CHAR_BASE+128+0x16,3);ram8w(PS4_CHAR_BASE+256+0x16,3);
 ram8w(PS4_CHAR_BASE+128+0x4C,0x21);ram8w(PS4_CHAR_BASE+256+0x4C,0x22);ram8w(0xEC4E,8);
}
static void inn_loaded(void){
 field_event_reload_pending=0;field_obj_count=4;
 for(unsigned n=0;n<4;n++){field_objs[n].active=1;field_objs[n].type=n==2?0x298:(n==3?0x74:0x38);field_objs[n].facing=0;field_objs[n].x=0x1E0;field_objs[n].y=0x1D0;field_objs[n].gba_tile=488+n*8;field_objs[n].render_w=n==3?0:16;field_objs[n].render_h=n==3?0:32;}
 r31_actor_load();
}
int main(int argc,char**argv){assert(argc==2);FILE*f=fopen(argv[1],"rb");assert(f&&fread(rom,1,sizeof rom,f)==sizeof rom);fclose(f);unsigned x=0x148,y=0x128;
 reset(0);r31_actor_spawn(7,0x1A0,0x268,0x318,0,0x580);
 for(unsigned n=1;n<=35;n++){r31_actor_update();assert((n<35)==(field_objs[7].active!=0));}
 reset(0);ram8w(0xECE0,1);r31_actor_spawn(0,0x8194,0x260,0x310,0,0x2B8);ram16w(0xC338,0x260);ram16w(0xC33A,0x2B0);
 for(unsigned n=1;n<=48;n++){r31_actor_update();assert(field_objs[0].x==0x260&&field_objs[0].y==0x310-n*2&&field_objs[0].facing==4);}
 for(unsigned n=0;n<20;n++){r31_actor_update();assert(field_objs[0].anim==0&&field_objs[0].y==0x2B0);}
 ram8w(0xECFE,2);ram16w(0xC338,0x270);ram16w(0xC33A,0x2C0);r31_actor_update();assert(field_objs[0].x==0x260&&field_objs[0].y==0x2B2);
 for(unsigned visited=0;visited<2;visited++){
  incoming(visited);assert(r33_cutscene_alys_wounded(&x,&y));assert(!flags[0x47]&&!(ram8r(0xEC4E)&8));assert(reloads==1&&field_custom_resume_event==0xA9);
  if(!visited){
   assert(field_custom_resume_phase==0&&ram16r(0xEC28)==0x39&&ram16r(0xEC2A)==0&&ram16r(0xEC48)==0x4C&&ram16r(0xEC4A)==0x6C&&ram16r(0xEC44)==4&&ram16r(0xEC46)==4);
   field_event_reload_pending=0;assert(r33_cutscene_alys_resume(&x,&y));assert(flags[0x12]&&resumes==4&&reloads==2&&!flags[0x47]);assert(!field_objs[7].active);
  }else assert(resumes==1);
  assert(field_custom_resume_phase==1&&field_event_reload_pending&&ram16r(0xEC28)==0x3F&&ram16r(0xEC2A)==0x3E&&ram16r(0xEC48)==0x46&&ram16r(0xEC4A)==0x44&&ram16r(0xEC44)==8&&ram16r(0xEC46)==0);
  uint8_t ids[4]={0,4,5,6};for(unsigned n=0;n<4;n++){assert(ram8r(0xF40A+n)==ids[n]);assert(ram8r(PS4_MACROS+n*4)==ids[n]&&ram8r(PS4_MACROS+n*4+1)==1);}assert(ram8r(0xF40E)==255&&!ram8r(PS4_MACROS+17));
  assert(ram8r(0xC00F)==0xA0&&ram8r(0xC04F)==0xA3&&ram8r(0xC08F)==0xA4);assert(!ram8r(PS4_CHAR_BASE+128+0x16)&&!ram8r(PS4_CHAR_BASE+256+0x16));assert(ram8r(PS4_CHAR_BASE+128+0x4C)==0x21&&ram8r(PS4_CHAR_BASE+256+0x4C)==0x22);
  inn_loaded();unsigned before=resumes;assert(r33_cutscene_alys_resume(&x,&y));assert(resumes==before+10&&dialogues==(visited?2:3)&&!remaining_segments);
  assert(flags[0x47]&&field_custom_resume_event==255&&!field_custom_resume_phase&&tree==0x1E2680&&!(ram8r(0xECFE)&1));assert(chaz.x==0x210&&chaz.y==0x220);assert(max_tiles<=768);
 }
 incoming(1);lead_char_id=1;ram8w(0xF40A,1);ram8w(0xF40B,0);r33_alys_queue_inn();assert(lead_char_id==0&&ram8r(0xF40A)==0&&ram8r(0xF40B)==4&&ram8r(0xF40C)==5&&ram8r(0xF40D)==6);
 puts("PASS R33 original Tree6 F7 seams, optional Saya branch, both reloads, party/64-byte carrier and macro compaction, preserved equipment, Demi join, dialogue restoration and actor bounds (movement/dialogue/display stubbed)");
}
'''
c=O/'r33_alys_probe.c';c.write_text(s)
subprocess.run(['gcc','-std=c11','-O1','-g','-fsanitize=address,undefined','-I'+str(R/'gba'),str(c),'-o',str(O/'r33_alys_probe')],check=True)
subprocess.run([str(O/'r33_alys_probe'),str(R/'references/roms/ps4built.bin')],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
