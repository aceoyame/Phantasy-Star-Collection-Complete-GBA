#!/usr/bin/env python3
"""Actual-ROM dialogue seams, native scene state and source actor/controller probe.
Movement orchestration, screen/palette presentation and the spaceport effect
are stubs; the new scene code, actors, party removal and reload carrier copy
are compiled unchanged under ASan/UBSan.
"""
from pathlib import Path
import os,re,subprocess,json
from md_compression import kosinski_decompress
R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts'
rom=(R/'references/roms/ps4built.bin').read_bytes()
trees={a:kosinski_decompress(rom[a:]) for a in [0x1E32A0,0x1F9020,0x1E2680,0x1F7190]}
entries6=trees[0x1E32A0].split(b'\xff');entry36=trees[0x1F9020].split(b'\xff')[0]
assert entries6[0x2E].count(b'\xf7')==6 and entry36.count(b'\xf7')==5
assert rom[0x75F34:0x75F53]==bytes.fromhex('adaeafb0b1b2b3b4b5b6b7b8b9babbb9bcbbb9bdbbb9bebbb9bfbbb9c0bbff')
assert rom[0x75FA8:0x75FC7]==bytes.fromhex('b9c0bbb9bfbbb9bebbb9bdbbb9bcbbb9babbb6b7b8b3b4b5b0b1b2adaeafff')
src=(R/'gba/title_runtime_v06.c').read_text();alys=(R/'gba/r33_alys_cutscene.h').read_text()
def function(name,text=src):
 m=re.search(r'^static[^\n]*\b'+name+r'\([^;]*?\)\s*\{',text,re.M);assert m,name
 p=m.end();depth=1
 while depth:depth+=(text[p]=='{')-(text[p]=='}');p+=1
 return text[m.start():p]+'\n'
s=(O/'r31_story_probe.c').read_text().split('int main(')[0]
s=s.replace('static unsigned tick,','static unsigned map_updates,edit_count,powerdown_palette_seen;static struct {unsigned x,y,plane,value,at;} edits[80];\nstatic unsigned tick,',1)
s=s.replace('static void hw_load_palettes(void){}','static void hw_load_palettes(void){if(cram[28]==0x24&&cram[29]==0x240)powerdown_palette_seen=1;}')
s=re.sub(r'static void field_dialogue_show\(.*?\n','',s)
s=re.sub(r'static void field_dialogue_resume_saved\(.*?\n','',s)
# Forward declarations are needed by the retained R31 helper header.
s=s.replace('#include "r31_story_events.h"','static int field_dialogue_show(unsigned,unsigned,unsigned);\nstatic int field_dialogue_resume_saved(unsigned,unsigned);\n#include "r31_story_events.h"')
s=s.replace('static void waitvb(void){tick++;}', 'static void waitvb(void){tick++;ram16w(0xEF1C,ram16r(0xEF1C)+1);}')
s+=r'''
#define PS4_CHAR_BASE 0xF500u
#define PS4_MACROS 0xF444u
#define FIELD_DIALOGUE_CAP 16384u
static uint8_t field_dialogue[FIELD_DIALOGUE_CAP],field_custom_resume_event=255,field_custom_resume_phase,field_event_reload_pending;
static unsigned field_dialogue_len,tree,reloads,panel_count,panel_id[32],panel_frames[32];
static struct {unsigned tree,offset,stop;} texts[32];static unsigned text_count;
static unsigned ps4_event_flag_test(unsigned id){return flags[id];}
static void field_palette_fade_in(void){}static void field_palette_fade_out(void){}
static void field_palette_all64_to_black(unsigned n){(void)n;}static void field_cutscene_panels_clear(void){}
static void hw_field_init(unsigned x,unsigned y){(void)x;(void)y;}
static void pause_panel_action(unsigned a,unsigned n){(void)a;(void)n;}
static void field_cutscene_panel_wait(unsigned p,unsigned n){assert(panel_count<32);panel_id[panel_count]=p;panel_frames[panel_count++]=n;tick+=n;}
static void field_palette_increase_tone_once(void){}
static void field_run_map_updates(void){map_updates++;}
static void field_event_redraw_explicit(unsigned x,unsigned y){r31_actor_draw(x,y);}
static void field_event_clear_source_object(unsigned n){r31_actor_clear(n);}
static void field_event_set_mapload_bit(unsigned n){ram8w(0xEC4E,ram8r(0xEC4E)|(1u<<n));}
static int ps4_load_field_map(uint16_t m){(void)m;memset(work_ram+0xC000,0,320);return 0;}
static void field_followers_sync(int n){(void)n;}
static void field_cutscene_reload(uint16_t m,uint16_t prev,uint16_t x,uint16_t y,uint16_t f,uint16_t a){
 ram16w(0xEC28,m);ram16w(0xEC2A,prev);ram16w(0xEC48,x);ram16w(0xEC4A,y);ram16w(0xEC44,f);ram16w(0xEC46,a);ram8w(0xEC4E,ram8r(0xEC4E)&~8u);field_event_reload_pending=1;reloads++;
}
static int field_event_place_appearing(unsigned n,unsigned*x,unsigned*y){(void)x;(void)y;assert(n==7);for(unsigned i=0;i<5;i++)assert(ram8r(0xC002+i*64)&2);ps4_event_flag_set(0x66);return 1;}
'''
# The original 3-byte FG elevator records must be written on the exact plane.
s=re.sub(r'static int field_layout_write_world\(.*?\n',r'''static int field_layout_write_world(unsigned x,unsigned y,unsigned plane,unsigned v){assert(edit_count<80);edits[edit_count].x=x;edits[edit_count].y=y;edits[edit_count].plane=plane;edits[edit_count].value=v;edits[edit_count++].at=map_updates;return 1;}
''',s)
for a,data in trees.items():s+=f'static const uint8_t tree_{a:x}[]={{'+','.join(map(str,data))+'};\n'
s+='static unsigned kos_decode(unsigned a,uint8_t*d,unsigned cap){tree=a;switch(a){\n'
for a,data in trees.items():s+=f'case {a}u:assert(cap>={len(data)});memcpy(d,tree_{a:x},{len(data)});return {len(data)};\n'
s+='default:assert(0);return 0;}}\n'
for n in ['field_dialogue_offset','field_dialogue_entry','field_dialogue_resolve']:s+=function(n)
s+=r'''
static int run_text(const uint8_t*p){
 const uint8_t*end=field_dialogue+field_dialogue_len;assert(p&&p<end&&text_count<32);
 texts[text_count].tree=tree;texts[text_count].offset=p-field_dialogue;field_dialogue_saved_cursor=0;
 while(p<end){unsigned op=*p++;if(op==0xFF||op==0xF7){field_dialogue_last_stop=op;field_dialogue_saved_cursor=p;texts[text_count++].stop=op;return 0;}
  unsigned n=0;if(op==0xF4||op==0xF9)n=1;else if(op==0xF5||op==0xFA||op==0xF6)n=2;
  else if(op==0xF2&&p<end){unsigned a=*p++;n=a==0?2:((a==3||a==4||a==11)?1:0);}assert((unsigned)(end-p)>=n);p+=n;
 }assert(0);return 0;
}
static int field_dialogue_show(unsigned id,unsigned x,unsigned y){(void)x;(void)y;dialogues++;return run_text(field_dialogue_resolve(id));}
static int field_dialogue_show_keep_panels(unsigned id,unsigned x,unsigned y){return field_dialogue_show(id,x,y);}
static int field_dialogue_resume_saved(unsigned x,unsigned y){(void)x;(void)y;resumes++;return run_text(field_dialogue_saved_cursor);}
static int field_dialogue_resume_saved_keep_panels(unsigned x,unsigned y){return field_dialogue_resume_saved(x,y);}
static void r33_map_wait(unsigned n,unsigned*x,unsigned*y){for(unsigned i=0;i<=n;i++){field_run_map_updates();field_event_redraw_explicit(*x,*y);waitvb();}}
'''
for n in ['ps4_macro_record_slot','ps4_macro_write','field_event_macro_remove_char','field_event_macro_add_attack','field_recover_party']:s+=function(n)
s+='#include "r48_party_departure.h"\n'
for n in ['r33_dialogue_tree','r33_party_position','r33_party_remove']:s+=function(n,alys)
s+='#include "r34_story_cutscenes.h"\n'
# Exact source segment offsets, compared with every text call through reloads.
for a,id,name in [(0x1E32A0,0x2E,'psyco_offsets'),(0x1F9020,0,'zio_offsets')]:
 data=trees[a];base=sum(len(x)+1 for x in data.split(b'\xff')[:id]);entry=data.split(b'\xff')[id]
 offsets=[base]+[base+i+1 for i,v in enumerate(entry) if v==0xF7]
 s+=f'static const unsigned {name}[]={{'+','.join(map(str,offsets))+'};\n'
s+=r'''
static void incoming(void){
 reset(0);text_count=edit_count=panel_count=map_updates=reloads=field_event_reload_pending=field_custom_resume_phase=0;field_custom_resume_event=255;
 uint8_t ids[5]={0,4,5,6,3};for(unsigned n=0;n<5;n++){ram8w(0xF40A+n,ids[n]);ps4_macro_write(0,ids[n],1,0);ram16w(0xC030+n*64,0x1E0+n*16);ram16w(0xC034+n*64,0x220+n*16);ram16w(0xC038+n*64,0x1E0+n*16);ram16w(0xC03A+n*64,0x220+n*16);ram8w(0xC00F+n*64,0xA0+n);}
 ram8w(0xECE0,1);ram8w(PS4_CHAR_BASE+4*128+0x4C,0x31);ram8w(PS4_CHAR_BASE+6*128+0x4C,0x32);
}
int main(int argc,char**argv){assert(argc==2);FILE*f=fopen(argv[1],"rb");assert(f&&fread(rom,1,sizeof rom,f)==sizeof rom);fclose(f);unsigned x=0x100,y=0x100;
 // Parameter bytes must not be interpreted as dialogue seams.
 incoming();r34_dialogue_segment(0x1E32A0,0x2E,7);assert(!field_dialogue_saved_cursor);
 // Literal source C actor, idle/16 pixel movement and original raw-art mappings.
 incoming();r31_actor_spawn(12,0xC,0x120,0xE0,8,0x480);unsigned t=tick;r34_object_move(12,0x100,0xE0,0,&x,&y);assert(tick-t==16&&field_objs[12].x==0x100&&field_objs[12].facing==12);
 for(unsigned n=0;n<40;n++){r31_actor_update_one(12);assert(field_objs[12].anim==0);}r31_actor_draw(x,y);assert(max_tiles<=768);
 incoming();r33_dialogue_tree(0x1F7190);assert(r34_cutscene_psyco_wand(&x,&y));assert(field_custom_resume_event==0xAA&&field_custom_resume_phase==0&&reloads==1&&!field_dialogue_saved_cursor);
 assert(ram16r(0xEC28)==0x3F&&ram16r(0xEC2A)==0x3E&&ram16r(0xEC48)==0x46&&ram16r(0xEC4A)==0x44&&ram16r(0xEC44)==12);
 field_obj_count=4;field_event_reload_pending=0;r33_dialogue_tree(0x1E2680);assert(r34_psyco_resume(&x,&y));assert(field_custom_resume_phase==1&&reloads==2&&!field_dialogue_saved_cursor&&!flags[0x63]&&!flags[0x67]);
 assert(ram16r(0xEC28)==0x39&&ram16r(0xEC48)==0x28&&ram16r(0xEC4A)==0x1C&&ram16r(0xEC46)==16);
 assert(text_count==6);for(unsigned n=0;n<5;n++)assert(texts[n+1].offset==psyco_offsets[n]&&texts[n+1].tree==0x1E32A0&&texts[n+1].stop==0xF7);
 unsigned expected_panels[]={0x56,0x57,0x5A,0x5B,0x5D,0x5E,0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x69,0x6A,0x6C};
 unsigned expected_frames[]={90,20,40,10,40,120,60,40,80,40,300,60,90,60,60,20,120};assert(panel_count==17);
 for(unsigned n=0;n<17;n++)assert(panel_id[n]==expected_panels[n]&&panel_frames[n]==expected_frames[n]);
 field_event_reload_pending=0;r33_dialogue_tree(0x1E2680);assert(r34_psyco_resume(&x,&y));assert(text_count==8);
 for(unsigned n=5;n<7;n++)assert(texts[n+1].offset==psyco_offsets[n]&&texts[n+1].tree==0x1E32A0);assert(texts[7].stop==0xFF);
 assert(flags[0x63]&&flags[0x67]&&!flags[0x68]&&field_custom_resume_event==255&&tree==0x1E2680);
 assert(chaz.x==0x110&&chaz.y==0xE0&&!field_objs[0].active&&!field_objs[12].active&&ram8r(0xECE0)==1&&!(ram8r(0xECFE)&1));
 assert(edit_count==2&&edits[0].plane==0&&edits[0].value==0x8F&&edits[1].plane==1&&edits[1].value==0x90);
 // Same-map bit-3 reload: all 320 source bytes survive a destructive map stub.
 incoming();r33_dialogue_tree(0x1F9020);ram16w(0xEC28,0xD4);assert(r34_cutscene_zio_defeated(&x,&y));assert(text_count==1&&texts[0].offset==zio_offsets[0]);
 assert(field_custom_resume_event==0xAB&&field_custom_resume_phase==0&&(ram8r(0xEC4E)&8)&&field_event_reload_pending);
 uint8_t carrier[320];memcpy(carrier,work_ram+0xC000,320);assert(!r34_load_preserving_party(0xD4));assert(!memcmp(carrier,work_ram+0xC000,320));r34_sync_preserved_party();
 assert(ram16r(0xC0F0)==0x1E8&&ram16r(0xC0F4)==0x1C0);unsigned olddest=ram16r(0xC0FA);t=tick;unsigned oldup=map_updates;
 r34_demi_drift(15,&x,&y);assert(tick-t==16&&map_updates-oldup==16&&ram16r(0xC0F4)==0x1B0&&ram16r(0xC0FA)==olddest&&!ram32r(0xC0E4));ram32w(0xC0F4,0x1C00000);
 field_event_reload_pending=0;r33_dialogue_tree(0x1F9020);assert(r34_zio_resume(&x,&y));assert(text_count==5&&field_custom_resume_phase==1&&field_event_reload_pending&&!field_dialogue_saved_cursor);
 for(unsigned n=1;n<5;n++)assert(texts[n].offset==zio_offsets[n]&&texts[n].tree==0x1F9020&&texts[n].stop==0xF7);
 assert(ram16r(0xC0F4)==0x160&&ram16r(0xC0FA)==0x1C0&&!(ram8r(0xEC4E)&8));assert(powerdown_palette_seen);
 assert(edit_count==60);for(unsigned n=0;n<60;n++){unsigned rev=n>=30,q=n%30;assert(edits[n].x==(14+q%3)*32&&edits[n].y==12*32&&edits[n].plane==0);assert(edits[n].value==rom8((rev?0x75FA8:0x75F34)+q));if(q>=3)assert(edits[n].at-edits[n-3].at==(rev?6:8));}
 assert(ram16r(0xEC28)==0&&ram16r(0xEC2A)==0xAC&&ram16r(0xEC48)==0x70&&ram16r(0xEC4A)==0xBC);
 for(unsigned ci=0;ci<11;ci++){ram16w(PS4_CHAR_BASE+ci*128+0x0E,0);ram16w(PS4_CHAR_BASE+ci*128+0x10,120+ci);ram8w(PS4_CHAR_BASE+ci*128+0x16,4);}
 ram16w(PS4_CHAR_BASE+6*128+0x0E,65);ram8w(PS4_CHAR_BASE+6*128+0x16,0);
 field_event_reload_pending=0;r33_dialogue_tree(0x1E2680);assert(r34_zio_resume(&x,&y));assert(ram16r(PS4_CHAR_BASE+0x0E)==120&&ram8r(PS4_CHAR_BASE+0x16)==0);assert(ram16r(PS4_CHAR_BASE+5*128+0x0E)==0&&ram16r(PS4_CHAR_BASE+3*128+0x0E)==0);
 assert(text_count==6&&texts[5].offset==zio_offsets[5]&&texts[5].tree==0x1F9020&&texts[5].stop==0xFF);
 uint8_t ids[3]={0,5,3};for(unsigned n=0;n<3;n++){assert(ram8r(0xF40A+n)==ids[n]);assert(ram8r(0xC00F+n*64)==(n==0?0xA0:(n==1?0xA2:0xA4)));assert(ram8r(PS4_MACROS+n*4)==ids[n]);}
 assert(ram8r(0xF40D)==255&&ram8r(0xF40E)==255&&!(ram8r(0xECFE)&4));assert(ram8r(PS4_CHAR_BASE+4*128+0x4C)==0x31&&ram8r(PS4_CHAR_BASE+6*128+0x4C)==0x32);
 assert(ram16r(0xEC48)==0x6C&&ram16r(0xEC4A)==0xB8&&field_custom_resume_phase==2&&!flags[0x68]&&!flags[0x61]);
 field_event_reload_pending=0;assert(r34_zio_resume(&x,&y));assert(flags[0x68]&&flags[0x66]&&flags[0x61]&&field_custom_resume_event==255);
 // Other departing characters: do not revive an already-wiped party, a
 // healthy survivor, absent/invalid IDs or an empty roster. Preserve TP.
 for(unsigned departing=0;departing<11;departing++){
  incoming();ram8w(0xF40A,departing);ram8w(0xF40B,departing==0?5:0);for(unsigned slot=2;slot<5;slot++)ram8w(0xF40A+slot,255);
  unsigned survivor=departing==0?5:0;ram16w(PS4_CHAR_BASE+departing*128+0x0E,42);ram8w(PS4_CHAR_BASE+departing*128+0x16,0);
  ram16w(PS4_CHAR_BASE+survivor*128+0x0E,0);ram16w(PS4_CHAR_BASE+survivor*128+0x10,321);ram16w(PS4_CHAR_BASE+survivor*128+0x12,17);ram8w(PS4_CHAR_BASE+survivor*128+0x16,4);
  r33_party_remove(departing);assert(ram8r(0xF40A)==survivor&&ram16r(PS4_CHAR_BASE+survivor*128+0x0E)==321&&ram8r(PS4_CHAR_BASE+survivor*128+0x16)==0&&ram16r(PS4_CHAR_BASE+survivor*128+0x12)==17);
 }
 incoming();ram16w(PS4_CHAR_BASE+6*128+0x0E,0);ram16w(PS4_CHAR_BASE+0x0E,0);r33_party_remove(6);assert(ram16r(PS4_CHAR_BASE+0x0E)==0);
 incoming();ram16w(PS4_CHAR_BASE+6*128+0x0E,10);ram8w(PS4_CHAR_BASE+6*128+0x16,0);ram16w(PS4_CHAR_BASE+0x0E,7);ram8w(PS4_CHAR_BASE+0x16,0);r33_party_remove(6);assert(ram16r(PS4_CHAR_BASE+0x0E)==7);
 incoming();memset(work_ram+0xF40A,255,5);r48_scripted_departure_guard(1);assert(ram8r(0xF40A)==255);
 puts("PASS R48 last-living Demi Zio continuation, 11 character departures, no extra healing/TP, empty roster; PASS R34 actual-ROM dialogue seams across five reload continuations, panel order/counts, source Hahn movement/OBJ bounds, 320-byte preserved reload, Demi fixed-point drift, original FG elevator tables and 8/6-update clocks, party/carrier/macro removal and source flags (orchestration/display/spaceport effect stubbed)");
}
'''
c=O/'r34_scene_probe.c';c.write_text(s)
subprocess.run(['gcc','-std=c11','-O1','-g','-fsanitize=address,undefined','-I'+str(R/'gba'),str(c),'-o',str(O/'r34_scene_probe')],check=True)
subprocess.run([str(O/'r34_scene_probe'),str(R/'references/roms/ps4built.bin')],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
(O/'R34_SOURCE_SEAMS.json').write_text(json.dumps({'psyco_wand_tree':hex(0x1E32A0),'entry':46,'f7_seams':6,'zio_tree':hex(0x1F9020),'zio_entry':0,'zio_f7_seams':5,'scope':'Original ROM text, tables and native state checks; no emulator/hardware playthrough.'},indent=2)+'\n')
