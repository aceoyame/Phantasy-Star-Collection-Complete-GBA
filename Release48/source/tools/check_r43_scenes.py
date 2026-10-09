#!/usr/bin/env python3
"""R43 scene/text/state and scripted NPC/recovery probes under ASan/UBSan.
Production dialogue runner, party walking, R43 helpers, character stat update,
RecoverStats and ROM pose fallback execute. Input, panel publication, map load,
ordinary NPCs and hardware are fixtures. Dialogue input waits are zero fixture
frames. Independent original text, recovery byte masks, item records and wrapped
16-pixel movement references validate outputs; this is no emulator playthrough.
"""
from pathlib import Path
import re,json,subprocess,os,csv
from md_compression import kosinski_decompress
R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts';src=(R/'gba/title_runtime_v06.c').read_text();rom=(R/'references/roms/ps4built.bin').read_bytes()
def function(name,text=src):
 m=re.search(r'^static[^\n]*\b'+name+r'\([^;]*?\)\s*\{',text,re.M);assert m,name
 p=m.end();d=1
 while d:d+=(text[p]=='{')-(text[p]=='}');p+=1
 return text[m.start():p]+'\n'
s=(O/'r42_scene_probe.c').read_text().split('int main(')[0]
space=(R/'gba/r36_spaceship.h').read_text().replace('static void r36_space_pass(void)', 'static void r36_space_pass_native(void)')
s=s.replace('#include \"r36_spaceship.h\"', 'static void r36_space_pass(void);\n'+space+'\nstatic void r36_space_pass(void){for(unsigned i=0xC000;i<0xC800;i++)assert(!ram8r(i));r36_space_pass_native();}\n')
s=s.replace('panel_frames[panel_count++]=0;', 'panel_frames[panel_count++]=tick;')
header42=(R/'gba/r42_rykros_leroof.h').read_text().replace('static void r42_tone(', 'static void r42_tone_native(')
s=s.replace('#include "r42_rykros_leroof.h"', 'static void r42_tone(unsigned,unsigned,unsigned);\n'+header42)
s=s.replace(function('field_update_npcs',s),'static int r43_npc_update(unsigned);\nstatic void field_update_npcs(unsigned x,unsigned y){(void)x;(void)y;for(unsigned i=0;i<field_obj_count;i++)if(field_objs[i].active)r43_npc_update(i);}\n')
s=s.replace(function('field_recover_party',s),function('field_recover_party'))
s+=function('item_s8')+function('item_u8')+function('update_one_char_derived')
s+='static const uint8_t*ps4_psc_party_pose(unsigned a,unsigned f,unsigned n){(void)a;(void)f;(void)n;return 0;}\n'+function('nibswap')+function('hw_copy_frame_rom')
s+=r'''
static unsigned tone43_index;
static void r42_tone(unsigned frames,unsigned restore,unsigned selective){
 unsigned id=++tone43_index;fprintf(trace42,"tone-start,%u,%u,%u,%u,%u,%u",id,tick,ram16r(0xEF1C),frames,restore,selective);for(unsigned i=0;i<64;i++)fprintf(trace42,",%u",cram[i]);for(unsigned i=0;i<64;i++)fprintf(trace42,",%u",field_palette_saved[i]);fputc(10,trace42);
 trace_kind42=100+id;trace_phase42=0;r42_tone_native(frames,restore,selective);trace_kind42=0;
}
static void log43(const char*label){log_scene42(label);for(unsigned i=0;i<panel_count;i++)fprintf(trace42,"paneltime,%s,%u,%u\n",label,panel_id[i],panel_frames[i]);}
'''
s+='#include "r43_script_npcs.h"\n#include "r43_elsydeon_reunion.h"\n'
s+=r'''
static const char* output43;
static void dump43(const char*label){char path[512];snprintf(path,sizeof path,"%s/R43_%s_ram.bin",output43,label);FILE*f=fopen(path,"wb");assert(f&&fwrite(work_ram,1,sizeof work_ram,f)==sizeof work_ram);fclose(f);}
static void seed43(void){for(unsigned i=0xF500;i<0xFAE1;i++)work_ram[i]=(uint8_t)(i*37+11);for(unsigned id=0;id<11;id++)for(unsigned q=0x4C;q<0x50;q++)ram8w(0xF500+id*128+q,0);}
static void npc43(unsigned i,unsigned type,unsigned x,unsigned y){struct FieldObjLite*o=&field_objs[i];o->active=1;o->type=type;o->x=x;o->y=y;o->facing=0;o->anim=0;ram16w(0xC300+i*64,type);ram16w(0xC330+i*64,x);ram16w(0xC334+i*64,y);}
static void check_map43(unsigned map,unsigned x,unsigned y,unsigned face){assert(field_event_reload_pending&&ram16r(0xEC28)==map&&ram16r(0xEC2A)==65535&&ram16r(0xEC48)==x&&ram16r(0xEC4A)==y&&ram16r(0xEC44)==face&&!ram16r(0xEC46)&&!(ram8r(0xEC4E)&8));}
int main(int argc,char**argv){assert(argc==4);FILE*f=fopen(argv[1],"rb");assert(f&&fread(rom,1,sizeof rom,f)==sizeof rom);fclose(f);trace42=fopen(argv[2],"w");assert(trace42);output43=argv[3];unsigned x=0,y=0;
 // Full RAM outputs independently check exact writes, including sentinel/empty party.
 for(unsigned count=0;count<=5;count++){fresh42();seed43();for(unsigned i=0;i<5;i++)ram8w(0xF40A+i,i<count?(i*2):255);char label[64];snprintf(label,sizeof label,"recovery%u_before",count);dump43(label);field_recover_party();snprintf(label,sizeof label,"recovery%u_after",count);dump43(label);}
 const unsigned last[]={1,4,6,8,9};
 for(unsigned c=0;c<5;c++){
  fresh42();seed43();ram8w(0xF40E,last[c]);for(unsigned i=0;i<5;i++)r33_party_position(ram8r(0xF40A+i),0x1E0,0x210-i*16,0);followers();r33_dialogue_tree(0x1EDCA0);dump43("cave_before");
  assert(r43_cutscene_before_cave(&x,&y)&&flags[0xD8]);check_map43(0x16F,0x3C,0x32,4);assert(ram8r(0xF40A)==0);for(unsigned i=1;i<5;i++)assert(ram8r(0xF40A+i)==255);assert(ram8r(0xED54)==0&&ram8r(0xED55)==5&&ram8r(0xED56)==3&&ram8r(0xED57)==7&&ram8r(0xED58)==last[c]);dump43("cave_after");if(c==4)log43("cave");
 }
 fresh42();seed43();ram8w(0xF40A,0);for(unsigned i=1;i<5;i++)ram8w(0xF40A+i,255);unsigned saved[]={0,5,3,7,9};for(unsigned i=0;i<5;i++)ram8w(0xED54+i,saved[i]);r33_party_position(0,0x1E0,0x190,0);followers();r33_dialogue_tree(0x1EDCA0);dump43("elsydeon_before");
 assert(r43_cutscene_elsydeon(&x,&y)&&flags[0xD9]&&ram8r(0xF54C)==0x77&&!ram8r(0xF54D));check_map43(0x16F,0x3C,0x32,0);for(unsigned i=0;i<5;i++)assert(ram8r(0xF40A+i)==saved[i]);assert(!field_active_panel_count);dump43("elsydeon_after");log43("elsydeon");
 fresh42();seed43();r33_party_position(0,0x1E0,0x190,0);followers();r33_dialogue_tree(0x1EDCA0);field_obj_count=2;npc43(0,0x70,0x1F0,0x1A0);npc43(1,0x264,0x1E0,0x1A0);dump43("reunion_before");
 assert(r43_cutscene_reunion(&x,&y)&&field_custom_resume_event==0xB2&&field_custom_resume_phase==1&&ui_frames==301);check_map43(0xBF,0x3C,0x24,0);log43("reunion_departure");
 reload40(0xBF,0x3C,0x24,0,0);r33_dialogue_tree(0x1FAAC0);field_obj_count=5;unsigned types[]={0x2C8,0x2CC,0x2D0,0x2D4,0x2D8},nx[]={0x1F0,0x1F0,0x1E0,0x1B0,0x230},ny[]={0x130,0x120,0x1D0,0x1B0,0x1C0};for(unsigned i=0;i<5;i++)npc43(i,types[i],nx[i],ny[i]);
 assert(r43_story_resume(&x,&y)&&flags[0xDA]&&field_custom_resume_phase==2&&ram8r(0xECEC)==0x89);check_map43(0xBF,0x3C,0x36,0);for(unsigned i=0;i<5;i++){unsigned tx[]={0x1F0,0x1F0,0x1E0,0x1D0,0x1F0},ty[]={0x190,0x180,0x1C0,0x1B0,0x1B0};assert(field_objs[i].x==tx[i]&&field_objs[i].y==ty[i]);}log43("reunion_arrival");
 reload40(0xBF,0x3C,0x36,0,0);assert(r43_story_resume(&x,&y)&&field_custom_resume_event==255);dump43("reunion_after");assert(!r43_story_resume(&x,&y));
 // Seven character-NPC types, four source speeds, both axis orders, and wrapped routes.
 unsigned alltypes[]={0x70,0x264,0x2C8,0x2CC,0x2D0,0x2D4,0x2D8};
 for(unsigned t=0;t<7;t++)for(unsigned speed=0;speed<4;speed++)for(unsigned axis=0;axis<2;axis++)for(unsigned route=0;route<4;route++){
  fresh42();field_obj_count=1;field_map.row_fg=field_map.col_fg=15;unsigned sx=route==1?496:16,sy=route==2?496:32,tx=route==0?64:(route==1?16:16),ty=route==0?80:(route==2?16:32);npc43(0,alltypes[t],sx,sy);r43_npc_init(0);ram16w(0xC338,tx);ram16w(0xC33A,ty);ram8w(0xECE0,speed);ram8w(0xECFE,axis?2:0);
  for(unsigned step=0;step<100;step++){assert(r43_npc_update(0));fprintf(trace42,"npc,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",t,speed,axis,route,step,field_objs[0].x,field_objs[0].y,ram16r(0xC328),ram16r(0xC32A),ram16r(0xC306));if(field_objs[0].x==tx&&field_objs[0].y==ty&&!ram16r(0xC328)&&!ram16r(0xC32A)){assert(r43_npc_update(0)&&!ram32r(0xC320)&&!ram32r(0xC324));break;}assert(step<99);}
 }
 // Production ROM pose fallback; independent byte comparison follows in Python.
 char path[512];snprintf(path,sizeof path,"%s/R43_npc_poses.bin",output43);f=fopen(path,"wb");assert(f);fresh42();field_obj_count=1;
 unsigned arttypes[]={0x70,0x264,0x2C8,0x2CC,0x2D0,0x2D4,0x2D8,0x68,0x6C};
 for(unsigned t=0;t<9;t++)for(unsigned face=0;face<4;face++)for(unsigned anim=0;anim<4;anim++){npc43(0,arttypes[t],16,32);field_objs[0].gba_tile=480;field_objs[0].facing=face*4;field_objs[0].anim=anim;assert(r43_npc_art(0));assert(fwrite(OBJ_VRAM16+480*16,1,256,f)==256);}field_objs[0].type=0x123;assert(!r43_npc_art(0));fclose(f);
 fclose(trace42);puts("PASS R43 original cave/El sydeon/reunion state, real reload seams, RecoverStats, scripted NPC trajectories and original ROM pose fallback under ASan/UBSan");}
'''
c=O/'r43_scene_probe.c';c.write_text(s);subprocess.run(['gcc','-std=c11','-O1','-g','-no-pie','-fsanitize=address,undefined','-I'+str(R/'gba'),str(c),'-o',str(O/'r43_scene_probe')],check=True)
trace=O/'R43_TRACE.csv';subprocess.run([str(O/'r43_scene_probe'),str(R/'references/roms/ps4built.bin'),str(trace),str(O)],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
rows=list(csv.reader(trace.open()));u16=lambda p:int.from_bytes(rom[p:p+2],'big')
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
for label,tree,entries in [('cave',0x1EDCA0,[0x30]),('elsydeon',0x1EDCA0,[0x31,0x32]),('reunion_departure',0x1EDCA0,[0]),('reunion_arrival',0x1FAAC0,[1])]:
 actual=[tuple(map(int,r[2:5])) for r in rows if r[0]=='text' and r[1]==label];expected=sum([segments(tree,e) for e in entries],[])
 if label=='reunion_arrival':expected=segments(0x1EDCA0,0)+expected # log spans both real reload phases
 assert actual==expected,(label,actual,expected)
# Original panel order includes F2-created art, with explicit source holds.
expected_panels={'cave':[0x18E,0x128], 'elsydeon':[0x129,0x12A,0x12B,0x12C,0x12D,0x12E,0x12F,0x130,0x131,0x132,0x18C,0x133,0x134,0x135,0x136,0x137,0x138,0x139,0x13A,0x13B,0x13C], 'reunion_departure':[0x13D], 'reunion_arrival':[0x13D,0x13F,0x13E,0xA7]}
for label,expected in expected_panels.items():assert [int(r[2]) for r in rows if r[0]=='panel' and r[1]==label]==expected,label
cues={'cave':[0xE2,0xF3], 'elsydeon':[0xF3,0xDC,0xA1,0xF3,0xF3,0xA9], 'reunion_departure':[0xF3,0x9B], 'reunion_arrival':[0xF3,0x9B,0x89,0xF3]}
for label,expected in cues.items():assert [int(r[2]) for r in rows if r[0]=='cue' and r[1]==label]==expected,label
p={int(r[2]):int(r[3]) for r in rows if r[0]=='paneltime' and r[1]=='elsydeon'}
for a,b,delay in [(0x129,0x12A,160),(0x12A,0x12B,120),(0x12B,0x12C,30),(0x12C,0x12D,160),(0x12D,0x12E,160),(0x12E,0x12F,50),(0x12F,0x130,341),(0x130,0x131,50),(0x131,0x132,50),(0x132,0x18C,341),(0x18C,0x133,160),(0x133,0x134,160),(0x134,0x135,160),(0x135,0x136,200),(0x136,0x137,160),(0x138,0x139,160),(0x139,0x13A,120),(0x13A,0x13B,160)]:assert p[b]-p[a]==delay,(hex(a),hex(b),p[b]-p[a],delay)
# Every production palette tick uses the original component arithmetic and source clock.
palette_ticks=0
starts=[r for r in rows if r[0]=='tone-start'];assert len(starts)==7
for r in starts:
 id,tick,clock,frames,restore,selective=map(int,r[1:7]);c=list(map(int,r[7:71]));saved=list(map(int,r[71:135]));expected=[];assert frames==40 and not selective
 if id in [3,4]:assert c==[0xEEE]*64
 if id==2:assert c[48:]==[0xEEE]*16
 if id==6:assert c[:32]==[0xEEE]*32
 for _ in range(frames):
  if not clock&3:
   if restore:c=[sum((((v>>shift)&14)-(2 if ((v>>shift)&14)!=((target>>shift)&14) else 0))<<shift for shift in [0,4,8]) for v,target in zip(c,saved)]
   else:c=[sum(min(14,((v>>shift)&14)+2)<<shift for shift in [0,4,8]) for v in c]
  clock=(clock+1)&65535;expected.append((clock,c[:]))
 actual=[(int(q[3]),list(map(int,q[4:]))) for q in rows if q[0]==str(100+id)];assert actual==expected,id;palette_ticks+=len(actual)
# Exact RecoverStats byte mask, full 64 KiB RAM comparisons for zero through five occupied slots.
def recovered(a,ids):
 b=bytearray(a)
 for id in ids:
  d=0xF500+id*128;b[d+14:d+16]=a[d+16:d+18];b[d+18:d+20]=a[d+20:d+22];b[d+22]=0
  for q in range(8):b[d+0x6A+q*2]=a[d+0x6B+q*2]
 for v in range(3):
  for q in range(8):d=0xFA86+v*32+q*2;b[d]=a[d+1]
 return b
for count in range(6):
 before=(O/f'R43_recovery{count}_before_ram.bin').read_bytes();after=(O/f'R43_recovery{count}_after_ram.bin').read_bytes();assert after==recovered(before,[i*2 for i in range(count)]),count
before=(O/'R43_cave_before_ram.bin').read_bytes();after=(O/'R43_cave_after_ram.bin').read_bytes();assert after[0xF500:0xFAE1]==recovered(before,[0])[0xF500:0xFAE1]
# Equipment effects from the original 22-byte item records, independent Python arithmetic.
def equip(a,id,items):
 b=bytearray(a);d=0xF500+id*128;b[d+0x4C:d+0x50]=bytes(items)
 records=[rom[0x2A8E28+(i-1)*22:0x2A8E28+i*22] if i else bytes(22) for i in items]
 signed=lambda q:q-256 if q>=128 else q
 for src_off,dst_off,item_off in [(0x18,0x19,11),(0x1B,0x1C,12),(0x1E,0x1F,13),(0x21,0x22,14)]:b[d+dst_off]=(a[d+src_off]+sum(signed(r[item_off]) for r in records))&255
 for base,dst,mod in [(0x18,0x24,15),(0x1E,0x28,16),(0x1B,0x2C,17)]:
  value=a[d+base]+sum(signed(r[11 if base==0x18 else (13 if base==0x1E else 12)])+signed(r[mod]) for r in records);b[d+dst:d+dst+2]=(value&65535).to_bytes(2,'big')
 b[d+0x50:d+0x52]=b'\0\0';els=[0]*14
 for slot,r in enumerate(records):
  if not items[slot]:continue
  element=r[18]
  if slot<2 and r[10]!=5:b[d+0x50+slot]=element
  elif element and element<=14:els[element-1]=1
 for i in range(14):b[d+0x30+i*2]=els[i] or a[d+0x31+i*2]
 return b
for label,configs in [('elsydeon',[(0,[0x77,0,0,0])]),('reunion',[(1,[0x71,0x88,0xA0,0x7C]),(4,[0x78,0,0x68,0x67]),(6,[0x79,0,0x70,0x6F]),(8,[0x62,0,0x6A,0x59]),(9,[0x5D,0x65,0x69,0x59])])]:
 before=(O/f'R43_{label}_before_ram.bin').read_bytes();after=(O/f'R43_{label}_after_ram.bin').read_bytes();expected=before
 for id,items in configs:expected=equip(expected,id,items)
 assert after[0xF500:0xFA80]==expected[0xF500:0xFA80],label
# Independent GetAutoInput/FieldObj_Move/UpdateStepDuration/UpdatePosition reference.
count=0
for t in range(7):
 for speed in range(4):
  for axis in range(2):
   for route in range(4):
    x=496 if route==1 else 16;y=496 if route==2 else 32;tx=64 if route==0 else 16;ty=80 if route==0 else (16 if route==2 else 32);left=0;dx=dy=0;face=0;expected=[]
    def direction(a,b):
     delta=b-a
     if not delta:return 0
     return (-1 if delta>0 else 1) if 512-abs(delta)<=abs(delta) else (1 if delta>0 else -1)
    for step in range(100):
     if not left:
      xd,yd=direction(x,tx),direction(y,ty);dx=dy=0
      if axis and yd:dy=yd
      elif xd:dx=xd
      elif yd:dy=yd
      if dx or dy:left=16;face=8 if dx>0 else (12 if dx<0 else (0 if dy>0 else 4))
     if left:n=min(1<<speed,left);left-=n;x=(x+dx*n)%512;y=(y+dy*n)%512
     expected.append((step,x,y,left*256 if dx else 0,left*256 if dy else 0,face))
     if x==tx and y==ty and not left:break
    actual=[tuple(map(int,r[5:])) for r in rows if r[0]=='npc' and tuple(map(int,r[1:5]))==(t,speed,axis,route)];assert actual==expected,(t,speed,axis,route,actual,expected);count+=len(actual)
# 144 character poses, including existing Hahn/Alys types, original tile layout/nibble conversion byte-exact.
expected=bytearray()
for id in [3,5,1,4,6,8,9,1,2]:
 for face in range(4):
  for anim in range(4):
   frame=(0x30 if face==1 else (0x18 if face in [2,3] else 0))+[0,8,0,16][anim]
   for row in range(4):
    for col in range(2):
     tile=rom[0x290000+id*0x900+(frame+col*4+row)*32:0x290000+id*0x900+(frame+col*4+row+1)*32];expected.extend(((q&15)<<4)|(q>>4) for q in tile)
assert (O/'R43_npc_poses.bin').read_bytes()==expected
(O/'R43_SOURCE_PROBE.json').write_text(json.dumps({'scope':__doc__,'original_text_seams_match':True,'recovery_full_ram_cases':6,'cave_companion_cases':5,'original_item_record_derived_stats':True,'original_panels_cues_holds_match':True,'palette_ticks_compared':palette_ticks,'npc_trajectory_cases':224,'npc_frames_compared':count,'npc_pose_bytes_compared':len(expected),'real_reunion_reload_phases':[1,2],'full_space_pass_frames':300,'emulator_playthrough':False},indent=2)+'\n')
print('PASS independent original text/party/recovery/equipment references;',count,'NPC frames;',len(expected),'original NPC pose bytes')
