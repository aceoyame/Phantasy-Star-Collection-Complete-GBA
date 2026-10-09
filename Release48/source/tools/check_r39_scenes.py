#!/usr/bin/env python3
"""R39 actual-source scene probe, with production party walking and palette math.
Map loading, dialogue display, general NPC wandering, tile animation and GBA
hardware are fixtures. Actual ROM text/actions, native scene headers, walking,
party carriers, destination selector and palette arithmetic execute under
ASan/UBSan. Independent references compare all 16 movement cases and all tested palette ticks.
Requires the R31/R34/R35/R36/R37/R38 probe chain; no playthrough certification.
"""
from pathlib import Path
import re,subprocess,json,os,csv
from md_compression import kosinski_decompress
R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts';src=(R/'gba/title_runtime_v06.c').read_text();rom=(R/'references/roms/ps4built.bin').read_bytes()
def function(name,text=src):
 m=re.search(r'^static[^\n]*\b'+name+r'\([^;]*?\)\s*\{',text,re.M);assert m,name
 p=m.end();d=1
 while d:d+=(text[p]=='{')-(text[p]=='}');p+=1
 return text[m.start():p]+'\n'
s=(O/'r38_scene_probe.c').read_text().split('int main(')[0]
s=s.replace('uint8_t facing,anim,anim_clock;} chaz;','uint8_t facing,anim,anim_clock,step_left;int8_t dx,dy;} chaz;')
s=s.replace('static struct {unsigned row_fg,col_fg,row_bg,col_bg;} field_map;','')
s=s.replace('static uint8_t lead_char_id;','static struct {unsigned row_fg,col_fg,row_bg,col_bg,collision_plane;} field_map;\nstatic uint8_t lead_char_id;')
for name in ['field_event_move_party_id','field_event_choreography_tick']:
 old=function(name,s);signature=old[:old.index('{')].rstrip()+';\n';s=s.replace(old,signature)
s=s.replace(function('field_event_source_speed',s),function('field_event_source_speed'))
s=s.replace('static void field_palette_all64_to_black(unsigned n){(void)n;}','static void field_palette_all64_to_black(unsigned);')
s+='static uint16_t field_palette_saved[64];\n'
for name in ['field_palette_save','field_palette_wait','field_palette_all64_to_black','field_palette_first32_from_black_to_current','choose_camera','field_event_axis_extent','field_event_wrapped_dir']:
 s+=function(name)
s+='static void field_event_redraw(unsigned*x,unsigned*y){field_event_redraw_explicit(*x,*y);}\nstatic void r30_tile_animation_update(void){}\n'
s+=function('field_event_choreography_tick')+function('field_event_move_party_id')
s+='static void presentation_hide_field_oam(void){for(unsigned i=0;i<112;i++)hw_hide_obj(i);}\n'
for address in [0x1EDCA0,0x1FC920]:
 data=kosinski_decompress(rom[address:]);name=f'tree_{address:x}'
 pos=s.index('static unsigned kos_decode(');s=s[:pos]+f'static const uint8_t {name}[]={{'+','.join(map(str,data))+'};\n'+s[pos:]
 pos=s.index('default:assert(0);return 0;}',s.index('static unsigned kos_decode('));s=s[:pos]+f'case {address}u:assert(cap>=sizeof {name});memcpy(d,{name},sizeof {name});return sizeof {name};\n'+s[pos:]
s+='#include "r39_lutz_air_seth.h"\n'
# Production map-start alignment; the actual loader itself is a fixture.
s+='#define FIELD_MAX_FOLLOWERS 4u\nstatic uint16_t hw_follower_key[4];\n'+function('field_followers_sync_aligned')
s+=r'''
static unsigned capture;static FILE* movement_trace;static FILE* palette_trace;
static void capture_tick(void){
 if(capture==1){fprintf(movement_trace,"%u",tick);for(unsigned i=0;i<5;i++){unsigned b=0xC000+i*64;fprintf(movement_trace,",%u,%u,%u,%u,%u",ram16r(b+0x30),ram16r(b+0x34),ram16r(b+0x38),ram16r(b+0x3A),ram16r(b+6));}fputc('\n',movement_trace);}
 if(capture==2){fprintf(palette_trace,"%u",tick);for(unsigned i=0;i<64;i++)fprintf(palette_trace,",%u",cram[i]);fputc('\n',palette_trace);}
}
static void sanctuary(unsigned map,unsigned sx,unsigned sy,unsigned face,unsigned align){
 assert(!ps4_load_field_map(map));ram16w(0xEC28,map);field_event_reload_pending=0;
 chaz.x=sx*8;chaz.y=sy*8;chaz.facing=face/4;lead_char_id=ram8r(0xF40A);
 field_followers_sync_aligned(align);r33_party_position(lead_char_id,chaz.x,chaz.y,face);
 for(unsigned i=0;i<field_follower_count;i++){struct PartyFollowerLite*p=&field_followers[i];r33_party_position(p->id,p->x,p->y,face);}
 r33_dialogue_tree(0x1EDCA0);for(unsigned i=0;i<64;i++)cram[i]=(uint16_t)((i%8)*0x222);
}
int main(int argc,char**argv){
 assert(argc==4);FILE*f=fopen(argv[1],"rb");assert(f&&fread(rom,1,sizeof rom,f)==sizeof rom);fclose(f);
 movement_trace=fopen(argv[2],"w");palette_trace=fopen(argv[3],"w");assert(movement_trace&&palette_trace);
 unsigned x=0,y=0;
 // Source all64 nibble fade and BG32-only restore, including final empty hold.
 story_fresh();for(unsigned i=0;i<64;i++)cram[i]=(uint16_t)((i%8)*0x222);
 capture=2;field_palette_all64_to_black(2);for(unsigned i=0;i<64;i++)assert(!cram[i]);
 for(unsigned i=0;i<64;i++)cram[i]=(uint16_t)((i%8)*0x222);
 field_palette_first32_from_black_to_current(2);capture=0;
 assert(tick==87);for(unsigned i=0;i<64;i++)assert(cram[i]==(i%8)*0x222);
 // Exact Lutz real-map requests and actual Tree20 F7/action stream.
 story_fresh();ram8w(0xF40E,9);sanctuary(0x16E,0x3C,0x3C,0,0);unsigned before=map_updates;
 assert(r39_cutscene_lutz(&x,&y));assert(map_updates-before==46); // 30 map waits plus 16 walking frames
 assert(field_custom_resume_event==0xB0&&!field_custom_resume_phase&&field_event_reload_pending&&!flags[0x97]);
 assert(ram16r(0xEC28)==0x16F&&ram16r(0xEC2A)==0x16E&&ram16r(0xEC48)==0x3C&&ram16r(0xEC4A)==0x4C&&ram16r(0xEC44)==12&&!ram16r(0xEC46));
 assert(text_count==1&&texts[0].tree==0x1EDCA0&&texts[0].stop==255);
 sanctuary(0x16F,0x3C,0x4C,12,0);before=map_updates;
 assert(r39_story_resume(&x,&y));assert(flags[0x97]&&field_custom_resume_event==0xB0&&field_custom_resume_phase==1&&field_event_reload_pending);
 assert(map_updates-before==226); // 96 walking ticks plus 130 main holds
 assert(ram16r(0xEC48)==0x3C&&ram16r(0xEC4A)==0x48&&!ram16r(0xEC44)&&ram16r(0xEC46)==8&&!(ram8r(0xECFE)&1));
 assert(text_count==3&&texts[1].stop==0xF7&&texts[2].stop==255&&texts[1].tree==0x1EDCA0&&texts[2].tree==0x1EDCA0&&texts[2].offset>texts[1].offset);
 assert(ram16r(0xC130)==0x1F0&&ram16r(0xC134)==0x1F0);
 sanctuary(0x16F,0x3C,0x48,0,8);assert(r39_story_resume(&x,&y));assert(field_custom_resume_event==255);
 for(unsigned i=0;i<5;i++)assert(ram16r(0xC030+i*64)==0x1E0&&ram16r(0xC034+i*64)==0x240-i*16);
 // Original Air Castle panel/text then actual selector confirm and cancel.
 for(unsigned cancel=0;cancel<2;cancel++){
  story_fresh();ram16w(0xEC28,0xD4);ram8w(0xF400,1);flags[0x98]=1;r33_dialogue_tree(0x1E99F0);pad_count=1;pad_script[0]=cancel?0x10:0x20;
  assert(r39_cutscene_air_castle(&x,&y));assert(flags[0x99]&&field_custom_resume_event==0xAD&&field_event_reload_pending);
  assert(text_count==1&&texts[0].tree==0x1E99F0&&texts[0].stop==255&&panel_id[0]==0x18A);
  assert(ram8r(0xED42)==0xDC);assert(field_custom_resume_phase==(cancel?5:0));
  if(cancel)assert(ram16r(0xEC28)==0xD4);else assert(ram16r(0xEC28)==rom16(0x64B02+8+2));
 }
 // Seth occupies slot5 and all used macro slots, with flag AFTER reload.
 story_fresh();assert(r39_cutscene_seth(&x,&y));assert(ram8r(0xF40E)==10&&!flags[0xC1]&&field_custom_resume_event==0xB0&&field_custom_resume_phase==2);
 assert(ram16r(0xEC28)==0&&ram16r(0xEC2A)==0xFFFF&&ram16r(0xEC48)==0xEE&&ram16r(0xEC4A)==0x136&&!ram16r(0xEC44)&&!ram16r(0xEC46));
 assert(panel_count==3&&panel_id[0]==0x118&&panel_id[1]==0x18D&&panel_id[2]==0x119&&text_count==1&&texts[0].tree==0x1FC920);
 assert(ram8r(PS4_MACROS+16)==10&&ram8r(PS4_MACROS+17)==1);assert(!ps4_load_field_map(0));field_event_reload_pending=0;assert(r39_story_resume(&x,&y)&&flags[0xC1]&&field_custom_resume_event==255);
 // Unchanged production walking: all speeds, axis priority, wrap ties, independent party.
 for(unsigned mode=0;mode<4;mode++)for(unsigned controls=0;controls<4;controls++){
  story_fresh();ram8w(0xF40E,9);ram8w(0xECE0,mode);ram8w(0xECFE,controls);field_map.row_fg=field_map.col_fg=7;
  for(unsigned i=0;i<5;i++)r33_party_position(ram8r(0xF40A+i),0xF0,0xF0,0);followers();
  fprintf(movement_trace,"case,%u,%u\n",mode,controls);capture=1;field_event_move_party_id(0,0x10,0x10,&x,&y);capture=0;
  assert(chaz.x==0x10&&chaz.y==0x10);
 }
 fclose(movement_trace);fclose(palette_trace);puts("PASS R39 source dialogue/map/panel/flag ordering, real selector confirm/cancel, production walking/palette loops under ASan/UBSan (map/UI/hardware fixtures)");
}
'''
# Capture the unchanged native loops at their fixture VBlank boundary.
s=s.replace('static void waitvb(void){','static void capture_tick(void);\nstatic void waitvb(void){').replace('tick++;ram16w(0xEF1C,ram16r(0xEF1C)+1);','tick++;ram16w(0xEF1C,ram16r(0xEF1C)+1);capture_tick();')
c=O/'r39_scene_probe.c';c.write_text(s)
subprocess.run(['gcc','-std=c11','-O1','-g','-fsanitize=address,undefined','-I'+str(R/'gba'),str(c),'-o',str(O/'r39_scene_probe')],check=True)
movement=O/'R39_MOVEMENT_TRACE.csv';palette=O/'R39_PALETTE_TRACE.csv'
subprocess.run([str(O/'r39_scene_probe'),str(R/'references/roms/ps4built.bin'),str(movement),str(palette)],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
# Independent source auto-input: choose axes, 16-pixel steps, propagate old
# coordinates to the next character at step start, signed wrapped route.
expected=[];actual=[];positions=None;steps=None;mode=controls=0
for row in csv.reader(movement.open()):
 if row[0]=='case':
  mode,controls=map(int,row[1:]);positions=[[240,240,240,240,0] for i in range(5)];steps=[[0,0,0] for i in range(5)];positions[0][2:4]=[16,16];continue
 for i,p in enumerate(positions):
  if not steps[i][0] and p[:2]!=p[2:4]:
   directions=[]
   for cur,dst in zip(p[:2],p[2:4]):
    delta=dst-cur;directions.append(0 if not delta else (1 if delta>0 else -1)*(-1 if 256-abs(delta)<=abs(delta) else 1))
   axis=1 if controls&2 and directions[1] else (0 if directions[0] else 1);d=directions[axis];steps[i]=[16,0 if axis else d,d if axis else 0];p[4]=([8,12] if axis==0 else [0,4])[d<0]
   if not controls&1 and i<4:positions[i+1][2:4]=p[:2]
  if steps[i][0]:
   n=min(1<<mode,steps[i][0]);p[0]=(p[0]+steps[i][1]*n)%256;p[1]=(p[1]+steps[i][2]*n)%256;steps[i][0]-=n
 expected.append(sum(positions,[]));actual.append(list(map(int,row[1:])))
assert actual==expected,next(((i,a,b) for i,(a,b) in enumerate(zip(actual,expected)) if a!=b),None)
colors=[(i%8)*0x222 for i in range(64)];frames=[]
live=colors[:]
while any(live):
 live=[sum(max(0,((c>>(4*n))&15)-1)<<(4*n) for n in range(4)) for c in live];frames.extend([live[:]]*3)
live=[0]*32+colors[32:]
while True:
 changed=False;out=[]
 for c,t in zip(live,colors):
  q=0
  for n in range(4):
   a,b=(c>>(4*n))&15,(t>>(4*n))&15
   if a<b:a+=1;changed=True
   q|=a<<(4*n)
  out.append(q)
 live=out;frames.extend([live[:]]*3)
 if not changed:break
observed=[list(map(int,r[1:])) for r in csv.reader(palette.open())];assert frames==observed
report={'scope':__doc__,'source_scenes':[0x14,0x15,0x19],'dialogue_trees':{'lutz':'1EDCA0','air_castle':'1E99F0','seth':'1FC920'},'movement_reference_cases':16,'movement_reference_frames':len(actual),'palette_reference_vblanks':len(frames),'source_palette_delays_exact':True,'seth_flag_deferred_until_reload':True,'air_castle_confirm_and_cancel':True,'emulator_playthrough':False}
(O/'R39_SOURCE_PROBE.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS independent walking/palette references:',len(actual),'movement frames and',len(frames),'palette VBlanks')
