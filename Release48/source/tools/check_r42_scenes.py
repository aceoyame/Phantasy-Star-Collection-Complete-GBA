#!/usr/bin/env python3
"""R42 original text and native scene/palette/ship progression under ASan/UBSan.
Production dialogue opcode runner, party/macro walking, source palette arithmetic
and R36 scene/space pass run. Input/glyph display, panels, map loading, generic
NPCs and hardware are fixtures; dialogue page waits cost zero fixture frames.
Independent source references compare every captured palette tick and text seam.
"""
from pathlib import Path
import re,json,subprocess,os,csv
from md_compression import kosinski_decompress,nemesis_decompress,enigma_decompress
R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts';src=(R/'gba/title_runtime_v06.c').read_text();rom=(R/'references/roms/ps4built.bin').read_bytes()
def function(name,text=src):
 m=re.search(r'^static[^\n]*\b'+name+r'\([^;]*?\)\s*\{',text,re.M);assert m,name
 p=m.end();d=1
 while d:d+=(text[p]=='{')-(text[p]=='}');p+=1
 return text[m.start():p]+'\n'
s=(O/'r41_scene_probe.c').read_text().split('int main(')[0]
s=s.replace(function('capture_tick',s),'static void capture_tick(void){capture42();}\n')
s=s.replace(function('field_followers_sync',s),'#define FIELD_MAX_FOLLOWERS 4u\nstatic uint16_t hw_follower_key[4];\n'+function('field_followers_sync'))
s='static void capture42(void);\n'+s
s=s.replace('tick++;ram16w(0xEF1C','tick++;if(tick>100000){fprintf(stderr,\"excess ticks\\n\");assert(0);}ram16w(0xEF1C')
s=s.replace(function('field_palette_increase_tone_once',s),function('field_palette_increase_tone_once'))
s=s.replace(function('field_dialogue_commit_palette',s),'static void field_dialogue_commit_palette(void){field_palette_mirror_live(64u);hw_load_palettes();}\n')
s=s.replace(function('field_dispatch_event_index',s),'static int field_dispatch_event_index(unsigned,unsigned*,unsigned*);\n')
s=s.replace(function('field_event_run_battle_request',s),'''static int field_event_run_battle_request(uint8_t index,unsigned*x,unsigned*y){(void)x;(void)y;battle_calls++;battle_id=index;assert(index==0x12&&flags[0xC5]&&(ram8r(0xEC4E)&8)&&ram8r(0xF40E)==255&&!ram8r(PS4_CHAR_BASE+10*128+0x16));ps4_return_to_title=battle_defeat;return 1;}\n''')
# Extend source-data fixtures only; actual art decoding is covered separately.
for address in [0x1FB610]:
 data=kosinski_decompress(rom[address:]);name=f'tree_{address:x}';pos=s.index('static unsigned kos_decode(')
 s=s[:pos]+f'static const uint8_t {name}[]={{'+','.join(map(str,data))+'};\n'+s[pos:]
 pos=s.index('default:assert(0);return 0;}',s.index('static unsigned kos_decode('));s=s[:pos]+f'case {address}u:assert(cap>=sizeof {name});memcpy(d,{name},sizeof {name});return sizeof {name};\n'+s[pos:]
address=0x1DB2C8;tile=0x260;data=nemesis_decompress(rom[address:]);name=f'art_{address:x}';pos=s.index('static int load_nem_vram_auto(')
s=s[:pos]+f'static const uint8_t {name}[]={{'+','.join(map(str,data))+'};\n'+s[pos:]
pos=s.index('default:assert(0);}',s.index('static int load_nem_vram_auto('));s=s[:pos]+f'case {address}u:assert(tile=={tile});a={name};n=sizeof {name};break;\n'+s[pos:]
address=0x1DDE0A;data=enigma_decompress(rom[address:],0x260);name=f'map_{address:x}';pos=s.index('static unsigned enigma_decode(')
s=s[:pos]+f'static const uint16_t {name}[]={{'+','.join(map(str,data))+'};\n'+s[pos:]
pos=s.index('default:assert(0);}',s.index('static unsigned enigma_decode('));s=s[:pos]+f'case {address}u:a={name};n=sizeof {name}/2;break;\n'+s[pos:]
s+=function('field_palette_first32_black')+function('field_panel_source_palette')
s+=r'''
#define PS4_DIALOGUE_F7_SEGMENTS 1
static unsigned modes42[32],texts_at42[32];
enum{FIELD_DIALOGUE_NORMAL=0,FIELD_DIALOGUE_KEEP_PANELS=1,FIELD_DIALOGUE_SOURCE_PALETTE=2};
static int dialogue_has_panel_actions(const uint8_t*p,const uint8_t*e){(void)p;(void)e;return 0;}
static void dma_fill32(void*p,unsigned n,unsigned v){assert(!v);memset(p,0,n);}
static void panel_clear_scaled(void){}
static void dialogue_obj_begin(void){}static void dialogue_obj_page_wait(void){}static void dialogue_obj_hide(void){}
static void dialogue_obj_set_portrait(unsigned n){(void)n;}
static void dialogue_obj_source_glyph(unsigned x,unsigned y,unsigned q){(void)x;(void)y;(void)q;}
static unsigned dialogue_yes_no_select(void){assert(0);return 0;}
static const uint8_t* dialogue_skip_ff_entries(const uint8_t*p,const uint8_t*e,unsigned n){return field_dialogue_offset(p,e,n);}
static void field_dialogue_text_action(unsigned a,unsigned v){if(a==3||a==4){if(a==4||v)ram8w(0x500A,v);waitvb();}else if(a==11)ps4_event_flag_set(v);else if(a==5){}else{fprintf(stderr,"uncovered action %u\n",a);assert(0);}}
'''
f=function('field_dialogue_run_mode');f=f.replace('const uint8_t* end=field_dialogue+field_dialogue_len;','unsigned record=text_count;assert(record<32);texts[record].tree=tree;texts[record].offset=p-field_dialogue;modes42[record]=mode;texts_at42[record]=tick;\n    const uint8_t* end=field_dialogue+field_dialogue_len;',1)
f=f.replace('dialogue_obj_hide();','texts[record].stop=field_dialogue_last_stop;text_count++;dialogue_obj_hide();')
s+=f+'#include "r42_rykros_leroof.h"\n'
s+=r'''
static int field_dispatch_event_index(unsigned index,unsigned*x,unsigned*y){assert(index==0x801B);unresolved=index;return r42_cutscene_rykros(x,y);}
static FILE* trace42;static unsigned trace_kind42,trace_phase42;
static void capture42(void){if(!trace_kind42)return;fprintf(trace42,"%u,%u,%u,%u",trace_kind42,trace_phase42,tick,ram16r(0xEF1C));for(unsigned i=0;i<64;i++)fprintf(trace42,",%u",cram[i]);fputc(10,trace42);}
static void fresh42(void){fresh41();battle_calls=battle_id=battle_defeat=ps4_return_to_title=unresolved=0;memset(field_palette_saved,0,sizeof field_palette_saved);}
static void log_scene42(const char*label){fprintf(stderr,"finished %s ticks%u\n",label,tick);fprintf(trace42,"scene,%s,%u,%u\n",label,tick,map_updates);for(unsigned i=0;i<text_count;i++)fprintf(trace42,"text,%s,%u,%u,%u,%u\n",label,texts[i].tree,texts[i].offset,texts[i].stop,texts_at42[i]);for(unsigned i=0;i<sound_count;i++)fprintf(trace42,"cue,%s,%u,%u\n",label,sounds[i].cue,sounds[i].at);for(unsigned i=0;i<panel_count;i++)fprintf(trace42,"panel,%s,%u\n",label,panel_id[i]);}
int main(int argc,char**argv){assert(argc==3);FILE*f=fopen(argv[1],"rb");assert(f&&fread(rom,1,sizeof rom,f)==sizeof rom);fclose(f);trace42=fopen(argv[2],"w");assert(trace42);unsigned x=0,y=0;
 // Seth may occupy any follower slot; all original carrier/macro/status removal precedes real battle request.
 for(unsigned slot=1;slot<5;slot++){
  fresh42();unsigned ids[]={0,5,3,7},n=0;for(unsigned i=0;i<5;i++){unsigned id=i==slot?10:ids[n++];ram8w(0xF40A+i,id);r33_party_position(id,0x180+i*16,0x100,4);}
  followers();for(unsigned m=0;m<8;m++)for(unsigned i=0;i<5;i++)ps4_macro_write(m,ram8r(0xF40A+i),1,0);ram8w(PS4_CHAR_BASE+10*128+0x16,0x7F);r33_dialogue_tree(0x1FC920);
  assert(r42_cutscene_aero_prism(&x,&y));assert(battle_calls==1&&battle_id==0x12&&flags[0xC5]&&field_follower_count==3&&ram8r(0xF40E)==255&&!ps4_return_to_title);
  uint16_t px[]={0x190,0x190,0x1A0,0x1A0},py[]={0x100,0xF0,0x100,0xF0},face[]={8,0,12,0};
  for(unsigned i=0;i<4;i++){assert(ram8r(0xF40A+i)==ids[i]);assert(ram16r(0xC030+i*64)==px[i]&&ram16r(0xC034+i*64)==py[i]&&ram16r(0xC038+i*64)==px[i]&&ram16r(0xC03A+i*64)==py[i]&&ram16r(0xC006+i*64)==face[i]);}
  for(unsigned m=0;m<8;m++){for(unsigned i=0;i<4;i++)assert(ram8r(PS4_MACROS+m*20+i*4)==ids[i]&&ram8r(PS4_MACROS+m*20+i*4+1)==1);for(unsigned q=16;q<20;q++)assert(!ram8r(PS4_MACROS+m*20+q));}
  for(unsigned q=0;q<64;q++)assert(!ram8r(0xC100+q));if(slot==4)log_scene42("aero");
 }
 fresh42();r33_dialogue_tree(0x1FC920);ram8w(0xF40E,10);followers();battle_defeat=1;assert(r42_cutscene_aero_prism(&x,&y)&&ps4_return_to_title&&flags[0xC5]);
 fresh42();r33_dialogue_tree(0x1FAAC0);assert(r42_cutscene_rykros(&x,&y)&&flags[0xD0]&&!field_active_panel_count&&!field_cutscene_panels);log_scene42("rykros");
 fresh42();r33_dialogue_tree(0x1FB610);for(unsigned i=0;i<64;i++)cram[i]=(i%8)*0x222;r33_party_position(0,0x1F0,0x1E0,4);followers();ram8w(0xECF3,10);
 assert(r42_event_meeting_leroof(&x,&y)&&flags[0xD1]&&chaz.x==0x1F0&&chaz.y==0x1E0);for(unsigned i=0;i<32;i++)assert(cram[i]==rom16(0x18711A+i*2));log_scene42("meeting");
 fresh42();r33_dialogue_tree(0x1FB610);r33_party_position(0,0x1F0,0x1F0,4);followers();ram16w(0xEC28,0xF0);ram8w(0xF400,2);pad_script[0]=0x20;pad_count=1;
 assert(r42_cutscene_leroof_again(&x,&y)&&flags[0xD6]&&flags[0xD7]&&field_custom_resume_event==0xAD&&field_event_reload_pending);log_scene42("again");
 // First Rykros dispatch must return to the original unskipped300-frame space pass and real reload.
 for(unsigned seen=0;seen<2;seen++){
  fresh42();flags[rom8(0x63EF2)]=1;flags[0xD0]=seen;ram8w(0xF400,2);ram16w(0xEC28,0xEF);field_custom_resume_event=0xAD;field_custom_resume_phase=0;x=0;y=0;
  assert(r36_spaceship_resume(&x,&y)&&flags[0xD0]&&field_event_reload_pending&&field_custom_resume_event==0xAD&&field_custom_resume_phase==3&&ui_frames==301);assert(unresolved==(seen?0:0x801B));
  uint32_t p=0x64B34+12;assert(ram16r(0xEC28)==rom16(p)&&ram16r(0xEC48)==rom16(p+2)&&ram16r(0xEC4A)==rom16(p+4));log_scene42(seen?"ship_seen":"ship_first");
 }
 // Independent per-tick palette references, all source phase offsets and counter wrap.
 for(unsigned phase=0;phase<4;phase++)for(unsigned kind=1;kind<=7;kind++){
  fresh42();tick=0;ram16w(0xEF1C,0xFFFC+phase);for(unsigned i=0;i<64;i++){cram[i]=(i%8)*0x222;field_palette_saved[i]=cram[i];}ram8w(0xECF3,10);trace_kind42=kind;trace_phase42=phase;
  if(kind==1)r42_tone(30,0,0);if(kind==2){for(unsigned i=0;i<64;i++)cram[i]=0xEEE;r42_tone(30,1,0);}if(kind==3)r42_tone(40,0,1);
  if(kind==4)r42_rykros_reveal();if(kind==5)r42_leroof_reveal();if(kind==6){for(unsigned i=0;i<32;i++)cram[i]=0;r42_first32_from_saved(2);}if(kind==7)r42_leroof_wait(300);
  trace_kind42=0;
 }
 // Production opt-in palette routing: old KEEP mode leaves source CRAM alone.
 for(unsigned mode=1;mode<=3;mode+=2){fresh42();field_dialogue_len=6;uint8_t txt[]={0xF2,0,1,0x1B,0,0xFF};memcpy(field_dialogue,txt,6);memset(cram,0x55,sizeof cram);field_dialogue_run_mode(field_dialogue,0,0,mode);if(mode==1){for(unsigned i=0;i<64;i++)assert(cram[i]==0x5555);}else {unsigned changed=0;for(unsigned i=0;i<64;i++)changed+=cram[i]!=0x5555;assert(changed==16||changed==32);}}
 fclose(trace42);puts("PASS R42 production dialogue opcode runner; source scene seams/cues/flags; Seth slots/macros/status; real DF3 battle request and defeat; first/repeat Rykros ship continuation");}
'''
c=O/'r42_scene_probe.c';c.write_text(s);subprocess.run(['gcc','-std=c11','-O1','-g','-no-pie','-fsanitize=address,undefined','-I'+str(R/'gba'),str(c),'-o',str(O/'r42_scene_probe')],check=True)
trace=O/'R42_TRACE.csv';subprocess.run([str(O/'r42_scene_probe'),str(R/'references/roms/ps4built.bin'),str(trace)],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
rows=list(csv.reader(trace.open()));u16=lambda p:int.from_bytes(rom[p:p+2],'big')
# Instruction reference arithmetic from original source, independent of native helper extraction.
def ref(kind,phase):
 c=[(i%8)*0x222 for i in range(64)];saved=c[:];clock=(0xFFFC+phase)&65535;result=[];q=10
 def publish():
  nonlocal clock
  clock=(clock+1)&65535;result.append((clock,c[:]))
 def cycle():
  nonlocal q
  if clock&3:return
  if q>=11:q=0
  c[11]=u16(0x70672+q*2);c[12]=u16(0x70688+q*2);q+=1
 if kind in [1,2,3]:
  if kind==2:c=[0xEEE]*64
  for f in range(40 if kind==3 else 30):
   if not clock&3:
    for i in range(64):
     if kind==2:c[i]=sum((((c[i]>>shift)&14)-(2 if ((c[i]>>shift)&14)!=((saved[i]>>shift)&14) else 0))<<shift for shift in [0,4,8])
     else:c[i]=sum(min(14,((c[i]>>shift)&14)+2)<<shift for shift in [0,4,8])
   if kind==3:
    for i in range(64):
     if i<19 or i>=32:c[i]=saved[i]
   publish()
 if kind==4:
  for row,delay in enumerate(rom[0x782D2:0x782D8]):
   c[23:32]=[u16(0x782DA+row*18+i*2) for i in range(9)]
   for _ in range(delay+1):publish()
 if kind==5:
  n=0
  for remaining in range(70,-1,-1):
   c[2:11]=[0]*9;c[2:3+n]=[u16(0x70626+(8-n+i)*2) for i in range(n+1)]
   if not remaining&7:n+=1
   cycle();publish()
 if kind==6:
  c[:32]=[0]*32
  while True:
   changed=False
   for i in range(32):
    channels=[]
    for shift in [0,4,8,12]:
     a=(c[i]>>shift)&15;b=(saved[i]>>shift)&15
     if a!=b:a+=1;changed=True
     channels.append(a<<shift)
    c[i]=sum(channels)
   for _ in range(3):publish()
   if not changed:break
 if kind==7:
  for _ in range(300):cycle();publish()
 return result
count=0
for phase in range(4):
 for kind in range(1,8):
  actual=[(int(r[3]),list(map(int,r[4:]))) for r in rows if r[0]==str(kind) and r[1]==str(phase)];expected=ref(kind,phase)
  assert actual==expected,(kind,phase,next((i for i,(a,b) in enumerate(zip(actual,expected)) if a!=b),None));count+=len(actual)
# Parse the original source bytes independently to derive each F7 entry/continuation offset.
def segments(tree,entry):
 data=kosinski_decompress(rom[tree:]);p=0
 for _ in range(entry):p=data.index(255,p)+1
 out=[]
 while True:
  start=p
  while True:
   op=data[p];p+=1
   if op in [255,247]:out.append((tree,start,op));break
   if op in [244,249]:p+=1
   elif op in [245,250,246]:p+=2
   elif op==242:
    a=data[p];p+=1;p+=2 if a==0 else (1 if a in [3,4,11] else 0)
  if op==255:break
 return out
for label,tree,entry in [('aero',0x1FC920,1),('rykros',0x1FAAC0,0x16),('meeting',0x1FB610,0)]:
 expected=segments(tree,entry);actual=[tuple(map(int,r[2:5])) for r in rows if r[0]=='text' and r[1]==label];assert actual==expected,(label,actual,expected)
expected=segments(0x1FB610,11)+segments(0x1FB610,12);actual=[tuple(map(int,r[2:5])) for r in rows if r[0]=='text' and r[1]=='again'];assert actual==expected,(actual,expected)
# Source-authored cues/holds, including original dialogue F2 sound controls.
def cue_rows(label):return [(int(r[2]),int(r[3])) for r in rows if r[0]=='cue' and r[1]==label]
expect={'aero':[0xE6,0xF3,0xF8,0xFD,0xF3,0x9F,0xF3,0xEC,0xF3],'rykros':[0xF3,0xE6,0xF3,0xF3,0xF3],'meeting':[0xA1,0xF3,0xFB,0x9A],'again':[0xA1,0xF3,0xF3,0xF3,0xF8,0xF3,0xFD,0x9B]}
for label,cues in expect.items():assert [q for q,t in cue_rows(label)]==cues,label
c=cue_rows('aero');assert c[1][1]-c[0][1]==30 and c[3][1]-c[2][1]==31 and c[4][1]-c[3][1]==1 and c[6][1]-c[5][1]==100 and c[8][1]-c[7][1]==60
c=cue_rows('rykros');assert c[1][1]-c[0][1]==50 and c[2][1]-c[1][1]==30 and c[3][1]-c[2][1]==61 and c[4][1]-c[3][1]==46
c=cue_rows('meeting');assert c[1][1]-c[0][1]==416 and c[2][1]-c[1][1]==120
for label,expected in [('aero',list(range(0xA0,0xA6))),('rykros',[0x18B,0x18F,0x190]),('again',list(range(0x11B,0x128)))]:assert [int(r[2]) for r in rows if r[0]=='panel' and r[1]==label]==expected
(O/'R42_SOURCE_PROBE.json').write_text(json.dumps({'scope':__doc__,'source_scenes':[0x1A,0x1B,0x48,0x1C],'independent_palette_ticks':count,'palette_phase_offsets':[0,1,2,3],'seth_follower_slots':[1,2,3,4],'production_dialogue_opcode_runner':True,'first_and_repeat_rykros_complete_space_pass':True,'emulator_playthrough':False},indent=2)+'\n');print('PASS independent original dialogue offsets and',count,'palette ticks/64 words')
