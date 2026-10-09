#!/usr/bin/env python3
"""Production R44 events/RunText/record movement/actor dispatch under ASan/UBSan.
Original-ROM text, camera tables and animation durations plus an independent
movement/inventory reference are checked. Input, ordinary NPCs, map loading,
glyph/panel publication and hardware are fixtures. No emulator playthrough.
"""
from pathlib import Path
import re,csv,json,subprocess,os
from md_compression import kosinski_decompress
R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts';rom=(R/'references/roms/ps4built.bin').read_bytes()
def fn(name,s):
 m=re.search(r'^static[^\n]*\b'+name+r'\([^;]*?\)\s*\{',s,re.M);assert m,name
 p=m.end();d=1
 while d:d+=(s[p]=='{')-(s[p]=='}');p+=1
 return s[m.start():p]
s=(O/'r43_scene_probe.c').read_text().split('int main(')[0]
s=re.sub(r'^const uint8_t r31_field_pose_data\[\]=\{[^\n]+','const uint8_t r31_field_pose_data[]={'+','.join(map(str,(R/'gba/r31_field_poses.bin').read_bytes()))+'};',s,flags=re.M)
s=s.replace('#include "r31_field_actors.h"','#define PS4_R44_SOURCE_EVENTS 1\nstatic int r44_monsen_actor(const struct FieldObjLite*);\nstatic void r44_event_actor_update(unsigned);\n#include "r31_field_actors.h"')
for a in [0x1E7D50,0x1F0620]:
 d=kosinski_decompress(rom[a:]);n=f'tree_{a:x}';s=s.replace('static unsigned kos_decode(',f'static const uint8_t {n}[]={{'+','.join(map(str,d))+'};\nstatic unsigned kos_decode(');s=s.replace('switch(a){',f'switch(a){{case {a}u:assert(cap>=sizeof {n});memcpy(d,{n},sizeof {n});return sizeof {n};',1)
s=s.replace(fn('field_update_npcs',s),'static void field_update_npcs(unsigned x,unsigned y){(void)x;(void)y;r31_actor_update();for(unsigned i=0;i<field_obj_count;i++)if(field_objs[i].active)r43_npc_update(i);}')
s=s.replace('enum{FIELD_DIALOGUE_NORMAL=0,FIELD_DIALOGUE_KEEP_PANELS=1,FIELD_DIALOGUE_SOURCE_PALETTE=2};','')
s=s.replace('static int run_text(','''enum{FIELD_DIALOGUE_NORMAL=0,FIELD_DIALOGUE_KEEP_PANELS=1,FIELD_DIALOGUE_SOURCE_PALETTE=2};
static unsigned dialogue_portrait_id,dialogue_choice_visible;
static int field_dialogue_run_mode(const uint8_t*,unsigned,unsigned,unsigned);
static int field_dialogue_run_resolved(const uint8_t*p,unsigned x,unsigned y){return field_dialogue_run_mode(p,x,y,FIELD_DIALOGUE_NORMAL);}
static int run_text(''')
prod=(R/'gba/title_runtime_v06.c').read_text()
for n in ['field_dialogue_show','field_dialogue_show_keep_panels','field_dialogue_resume_saved','field_dialogue_resume_saved_keep_panels']:s=s.replace(fn(n,s),fn(n,prod))
s=s.replace(fn('capture42',s),r'''
static const char*case44;static unsigned trace44;
static void capture42(void){if(!trace44)return;
 fprintf(trace42,"frame,%s,%u,%u,%u,%u,%u,%u",case44,tick,map_updates,text_count,ram8r(0xECFE),ram16r(0xECD0),ram8r(0xECF2));
 for(unsigned i=0;i<4;i++)fprintf(trace42,",%u",ram32r(0xEF90+i*4));
 for(unsigned i=0;i<5;i++){uint16_t b=0xC000+i*64;fprintf(trace42,",%u,%u,%u,%u,%u,%u,%u,%u",ram8r(0xF40A+i),ram32r(b+0x30),ram32r(b+0x34),ram16r(b+0x38),ram16r(b+0x3A),ram16r(b+6),ram16r(b+0x28),ram16r(b+0x2A));}
 for(unsigned i=8;i<12;i++)fprintf(trace42,",%u,%u,%u,%u",field_objs[i].active,field_objs[i].x,field_objs[i].y,field_objs[i].anim);
 fprintf(trace42,",%u,%u,%u,%u",field_objs[6].active,ram8r(0xC490),ram8r(0xC491),ram16r(0xC49C));for(unsigned i=0;i<5;i++)fprintf(trace42,",%u",ram8r(0xC002+i*64));fputc(10,trace42);
}
''')
s+='#include "r44_source_motion.h"\n#include "r44_event_actors.h"\n#include "r44_monsen_dying_boy.h"\n'
s+=r'''
static void seed44(const unsigned*ids,unsigned count,unsigned map,unsigned px,unsigned py){
 fresh42();memset(work_ram+0xC000,0,0x800);memset(field_objs,0,sizeof field_objs);memset(field_followers,0,sizeof field_followers);field_obj_count=8;field_follower_count=0;
 for(unsigned i=0;i<5;i++)ram8w(0xF40A+i,i<count?ids[i]:255);lead_char_id=ids[0];
 for(unsigned i=0;i<count;i++){r33_party_position(ids[i],px,py-i*16,0);ram16w(0xC000+i*64,(ids[i]+1)*4|0x8000);}
 chaz.x=px;chaz.y=py;chaz.facing=0;followers();ram16w(0xEC28,map);field_map.row_fg=field_map.col_fg=field_map.row_bg=field_map.col_bg=31;ram8w(0xECE0,1);ram8w(0xECFE,0x80);
 npc43(map==0x73?6:2,0x3C,px-16,py-16);r33_dialogue_tree(map==0x73?0x1E7D50:0x1F0620);ram32w(0xEF94,(px-152)<<16);ram32w(0xEF90,(py-88)<<16);ram32w(0xEF9C,(px-152)<<16);ram32w(0xEF98,(py-88)<<16);
}
int main(int argc,char**argv){assert(argc==3);FILE*f=fopen(argv[1],"rb");assert(f&&fread(rom,1,sizeof rom,f)==sizeof rom);fclose(f);trace42=fopen(argv[2],"w");assert(trace42);unsigned x,y;char label[80];
 for(unsigned mode=0;mode<2;mode++)for(unsigned edge=0;edge<3;edge++){
  unsigned ids[]={0};seed44(ids,1,0x73,0x2F0,0x240);field_objs[6].active=0;
  ram32w(0xEF94,(edge==0?0:edge==1?700:350)*65536u+0x1234);ram32w(0xEF9C,ram32r(0xEF94));ram32w(0xEF90,(edge==0?0:edge==1?798:400)*65536u+0x4321);ram32w(0xEF98,ram32r(0xEF90));snprintf(label,sizeof label,"quake%u_%u",mode,edge);case44=label;trace44=1;ram8w(0xECF2,mode?120:0);x=ram16r(0xEF94);y=ram16r(0xEF90);r44_quake(mode?60:120,mode,&x,&y);trace44=0;log_scene42(label);
 }
 for(unsigned c=0;c<3;c++){
  unsigned ids[3][5]={{0,4,5,6,255},{0,4,5,6,3},{4,6,0,5,3}};seed44(ids[c],c?5:4,0x73,0x2F0,0x240);snprintf(label,sizeof label,"monsen%u",c);case44=label;trace44=1;x=ram16r(0xEF94);y=ram16r(0xEF90);assert(r44_event_monsen(&x,&y));trace44=0;assert(flags[0x60]&&ram8r(0xECFE)==0x80&&!ram8r(0xECF2));assert(ram16r(0xC030)==0x310&&ram16r(0xC034)==0x220);for(unsigned i=8;i<12;i++)assert(!field_objs[i].active&&!ram16r(0xC300+i*64));assert(text_count==10);log_scene42(label);
 }
 for(unsigned c=0;c<12;c++){
  unsigned extras[]={1,4,6,8,9,10,255};unsigned ids[]={0,3,5,7,c<7?extras[c]:9};if(c==11){ids[0]=7;ids[3]=0;}seed44(ids,c==6?4:5,0xE7,0x1A0,0x1D0);flags[0xA9]=c!=8;flags[0xAA]=c==10;
  for(unsigned i=0;i<40;i++)ram8w(PS4_INV_BASE+i,i<8?0x20+i:0);if(c!=9)ram8w(PS4_INV_BASE+3,0x8A);ram8w(PS4_INV_BASE+7,c==9?0x27:0x8A);uint8_t before[41];memcpy(before,work_ram+PS4_INV_BASE,41);
  snprintf(label,sizeof label,"boy%u",c);case44=label;trace44=1;x=ram16r(0xEF94);y=ram16r(0xEF90);assert(r44_event_dying_boy(&x,&y));trace44=0;
  if(c==8||c==9||c==10){assert(!memcmp(before,work_ram+PS4_INV_BASE,41));assert(flags[0xAA]==(c==10)&&tick==1);}else{assert(flags[0xAA]&&ram8r(0xECFE)==0x80&&ram8r(0xECE0)==1&&!field_objs[6].active&&!ram16r(0xC480));assert(!(ram8r(0xC002+field_event_party_slot_for_id(0)*64)&2));assert(ram16r(0xC030)==0x1A0&&ram16r(0xC034)==0x1D0&&text_count==4);assert(!memcmp(work_ram+PS4_INV_BASE,before,3)&&!memcmp(work_ram+PS4_INV_BASE+3,before+4,36));}
  log_scene42(label);fprintf(trace42,"flags,%s",label);for(unsigned i=0;i<256;i++)fprintf(trace42,",%u",flags[i]);fputc(10,trace42);
 }
 for(unsigned c=0;c<44;c++){
  fresh42();for(unsigned i=0;i<41;i++)ram8w(PS4_INV_BASE+i,(uint8_t)(i+1));if(c<40)ram8w(PS4_INV_BASE+c,0x8A);else{ram8w(PS4_INV_BASE+10,0x8A);ram8w(PS4_INV_BASE+30,0x8A);if(c==41)ram8w(PS4_INV_BASE+2,0);if(c==42)for(unsigned i=0;i<35;i++)ram8w(PS4_INV_BASE+i,0);if(c==43)memset(work_ram+PS4_INV_BASE,0,40);}
  fprintf(trace42,"invbefore,%u",c);for(unsigned i=0;i<41;i++)fprintf(trace42,",%u",ram8r(PS4_INV_BASE+i));fputc(10,trace42);int found=r44_take_alis_sword();fprintf(trace42,"invafter,%u,%d",c,found);for(unsigned i=0;i<41;i++)fprintf(trace42,",%u",ram8r(PS4_INV_BASE+i));fputc(10,trace42);
 }
 for(unsigned speed=0;speed<4;speed++)for(unsigned axis=0;axis<2;axis++)for(unsigned chain=0;chain<2;chain++)for(unsigned route=0;route<4;route++){
  fresh42();memset(work_ram+0xC000,0,0x180);field_map.row_fg=field_map.col_fg=31;ram8w(0xECE0,speed);ram8w(0xECFE,axis*2+(chain?0:1));unsigned sx=route==1?80:16,sy=route==2?96:32,tx=route==3?16:64,ty=route==3?32:80;ram32w(0xC030,sx<<16);ram32w(0xC034,sy<<16);ram16w(0xC038,tx);ram16w(0xC03A,ty);
  for(unsigned t=0;t<120;t++){unsigned moved=r44_record_move(0xC000,1);fprintf(trace42,"move,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",speed,axis,chain,route,t,ram32r(0xC030),ram32r(0xC034),ram32r(0xC020),ram32r(0xC024),ram16r(0xC028),ram16r(0xC02A),ram16r(0xC006),ram16r(0xC078),ram16r(0xC07A),moved);if(ram16r(0xC030)==tx&&ram16r(0xC034)==ty&&!ram16r(0xC028)&&!ram16r(0xC02A))break;assert(t<119);}
 }
 fresh42();field_obj_count=0;r31_actor_spawn(6,0x334,0x1D0,0x1B8,0,0x43C);unsigned n=0;
 do{r31_actor_update_one(6);r31_actor_draw(0x180,0x180);assert(((OAM16[44*4+2]>>12)&15)==2&&r31_actor_pose(&field_objs[6]));fprintf(trace42,"sword,%u,%u,%u,%u\n",n++,ram8r(0xC490),ram8r(0xC491),ram16r(0xC49C));assert(n<500);}while(ram16r(0xC49C)!=65535);
 for(unsigned family=0;family<3;family++)for(unsigned face=0;face<4;face++){fresh42();field_obj_count=0;unsigned bases[]={0x42B,0x3E3,0x39B};r31_actor_spawn(8,0x8194,0x230,0x220,face*4,bases[family]);r31_actor_update_one(8);r31_actor_draw(0x200,0x200);assert(((OAM16[44*4+2]>>12)&15)==3&&r31_actor_pose(&field_objs[8])->facing==(family+3)*16+face*4);}
 fclose(trace42);puts("PASS R44 events, source branches, record motion, actor dispatch and OAM palettes under ASan/UBSan");}
'''
c=O/'r44_scene_probe.c';c.write_text(s);subprocess.run(['gcc','-std=c11','-O1','-g','-no-pie','-fsanitize=address,undefined','-I'+str(R/'gba'),str(c),'-o',str(O/'r44_scene_probe')],check=True)
trace=O/'R44_TRACE.csv';subprocess.run([str(O/'r44_scene_probe'),str(R/'references/roms/ps4built.bin'),str(trace)],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
rows=list(csv.reader(trace.open()))
def selected(kind,label,start=2):return [list(map(int,r[start:])) for r in rows if r[:2]==[kind,label]]
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
   elif op==242:a=data[p];p+=1;p+=2 if a==0 else (1 if a in [3,4,11] else 0)
  if op==255:break
 return out
for c in range(3):
 label=f'monsen{c}';assert [tuple(r[:3]) for r in selected('text',label)]==segments(0x1E7D50,0x40)
 cues=selected('cue',label);assert [r[0] for r in cues]==[0xF3]+[0xDD]*45+[0xF3]*9
 times=[r[-1] for r in selected('text',label)];pcount=4 if c==0 else 5
 assert [times[i+1]-times[i] for i in range(4)]==[182+pcount,60,61,2],times
 assert [times[i+1]-times[i] for i in range(4,9)]==[1,85,94,94,122],times
 for phase in [9,10]:
  hold=[r for r in selected('frame',label) if r[2]==phase];hold=hold[2:] if phase==9 else hold[:120];assert len(hold)==120 and len(set(r[1] for r in hold))==1
for c in range(12):
 label=f'boy{c}';entry=0x3E if c==10 else (0x3B if c in [8,9] else 0x33);assert [tuple(r[:3]) for r in selected('text',label)]==segments(0x1F0620,entry)
 f=selected('flags',label)[0];expected=[0]*256;expected[0xA9]=int(c!=8);expected[0xAA]=int(c not in [8,9]);assert f==expected
# Source camera step tables, clamp bounds and exact before/after-VBlank counters.
for mode in range(2):
 for edge in range(3):
  pos=[([0,798,400][edge]<<16)+0x4321,([0,700,350][edge]<<16)+0x1234]*2
  for f,r in enumerate(selected('frame',f'quake{mode}_{edge}')):
   table=0x6E73A if mode and f<2 else 0x6E6B6
   for a,index in enumerate([1,0,3,2]):
    p=table+(f%2)*16+a*4;step=int.from_bytes(rom[p:p+4],'big',signed=True);pos[index]=max(0,min((1024-(320 if a%2==0 else 224))<<16,pos[index]+step))
   assert r[6:10]==pos and r[0]==f+1 and r[1]==f+1 and r[4]==((f+1)%2 if mode else f%2) and r[5]==(120+f+1 if mode else f),(mode,edge,f)
for c in range(44):
 a=selected('invbefore',str(c))[0];after=selected('invafter',str(c))[0];n=sum(q!=0 for q in a[:40]);b=a[:];found=0
 for i in range(n):
  if b[i]==0x8A:b[i]=0;found=1;break
 if found:
  for i in range(40):
   if not b[i]:b[i:40]=b[i+1:41];break
 assert after==[found]+b,c
# Independent source GetAutoInput, signed 8.8 duration and 16.16 position.
def direction(a,b):
 d=b-a
 if not d:return 0
 return (-1 if d>0 else 1) if 1024-abs(d)<=abs(d) else (1 if d>0 else -1)
def step(q,tx,ty,speed,axis,chain=False):
 x,y,vx,vy,lx,ly,face,nx,ny=q
 if not lx and not ly:
  xd,yd=direction(x>>16,tx),direction(y>>16,ty);dx=dy=0
  if axis and yd:dy=yd
  elif xd:dx=xd
  elif yd:dy=yd
  lx=4096 if dx else 0;ly=4096 if dy else 0;vx=dx*speed*65536;vy=dy*speed*65536
  if dx or dy:
   face=8 if dx>0 else (12 if dx<0 else (0 if dy>0 else 4))
   if chain:nx=x>>16;ny=y>>16
 moved=int(bool(vx or vy));lx=(lx-(abs(vx)>>8))&65535;ly=(ly-(abs(vy)>>8))&65535
 return ((x+vx)&0xFFFFFFFF,(y+vy)&0xFFFFFFFF,vx,vy,lx,ly,face,nx,ny),moved
move_frames=0
for speed in range(4):
 for axis in range(2):
  for chain in range(2):
   for route in range(4):
    q=((80 if route==1 else 16)<<16,(96 if route==2 else 32)<<16,0,0,0,0,0,0,0);tx=16 if route==3 else 64;ty=32 if route==3 else 80;expected=[]
    for t in range(120):
     q,moved=step(q,tx,ty,1<<speed,axis,chain);x,y,vx,vy,lx,ly,face,nx,ny=q;expected.append((t,x,y,vx&0xFFFFFFFF,vy&0xFFFFFFFF,lx,ly,face,nx,ny,moved))
     if x>>16==tx and y>>16==ty and not lx and not ly:break
    actual=[tuple(map(int,r[5:])) for r in rows if r[0]=='move' and tuple(map(int,r[1:5]))==(speed,axis,chain,route)];assert actual==expected,(speed,axis,chain,route);move_frames+=len(actual)
seq=int.from_bytes(rom[0x4C39C:0x4C3A0],'big');assert rom[seq]==0x8A;frame=duration=0;expected=[]
for t in range(500):
 duration=(duration-1)&255
 if duration&128:frame=(frame+1)%10;duration=rom[seq+1+frame]
 done=frame==9 and duration==1;expected.append((t,frame,duration,65535 if done else 0))
 if done:break
assert [tuple(map(int,r[1:])) for r in rows if r[0]=='sword']==expected
simultaneous_ticks=0
for c in [0,1,2,3,4,5,6,7,11]:
 label=f'boy{c}';times=[r[-1] for r in selected('text',label)];count=4 if c==6 else 5
 assert times[1]-times[0]==150+count+1 and times[2]-times[1]==len(expected)+1 and times[3]-times[2]==80,(c,times)
 frames=selected('frame',label);hold=[r for r in frames if r[2]==1][:150];assert len(hold)==150 and hold[-1][1]-hold[0][1]==149
 ids=[hold[0][10+i*8] for i in range(count)];states=[(0x1A0<<16,(0x1D0-i*16)<<16,0,0,0,0,0,0,0) for i in range(count)];targets={0:(0x1F0,0x1C0),3:(0x1E0,0x1A0),5:(0x1F0,0x1A0),7:(0x200,0x1A0)};targets.update({id:(0x210,0x1A0) for id in [1,4,6,8,9,10]})
 for r in hold:
  for i,id in enumerate(ids):
   states[i],_=step(states[i],*targets[id],1,0);a=r[10+i*8:18+i*8];q=states[i];assert (a[1],a[2],a[5],a[6],a[7])==(q[0],q[1],q[6],q[4],q[5]),(c,i,r[0]);simultaneous_ticks+=1
 anim=[r for r in frames if r[-9]==1];assert len(anim)==len(expected)
 chazslot=ids.index(0)
 for r in anim:assert r[-5+chazslot]&2
 # Authored selection cues are present at each actual Event_RunDialogue seam.
 assert [r[0] for r in selected('cue',label)].count(0xF3)==4
# Final Monsen camera runs all party records, updates both planes, and retains FG/BG steps.
for c in range(3):
 frames=selected('frame',f'monsen{c}');last=frames[-1];assert last[7]==(0x310-0x98)<<16 and last[6]==(0x220-0x58)<<16 and last[6:8]==last[8:10]
 tail=[r for r in frames if r[2]==10][120:];assert all(b[1]-a[1]==1 for a,b in zip(tail,tail[1:]));assert any(r[10+8+1]!=tail[0][10+8+1] or r[10+8+2]!=tail[0][10+8+2] for r in tail[25:]),'Camera must update followers'
(O/'R44_SOURCE_PROBE.json').write_text(json.dumps({'scope':__doc__,'monsen_party_cases':3,'dying_boy_cases':12,'inventory_cases':44,'camera_table_ticks_compared':540,'movement_cases':128,'movement_ticks_compared':move_frames,'simultaneous_party_record_ticks_compared':simultaneous_ticks,'sword_ticks_compared':len(expected),'source_text_seams_selection_cues_and_holds_match':True,'emulator_playthrough':False},indent=2)+'\n');print('PASS independent source references:',move_frames,'motion ticks,',simultaneous_ticks,'simultaneous party ticks,',len(expected),'sword ticks')
