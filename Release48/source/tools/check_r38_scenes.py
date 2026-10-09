#!/usr/bin/env python3
"""Actual-ROM R38 story/party/vehicle state and Esper controller probe.
The unchanged R38 header and actor code run under ASan/UBSan. Map loading,
ordinary NPC wandering, dialogue display, panel drawing and hardware are
stubs. Source text/actions, carrier copies, camera and new actor updates are
real. No emulator or finishability claim.
Requires check_r31_story, r34_scenes, r35_scenes, r36_spaceship, r37_scenes.
"""
from pathlib import Path
import re,subprocess,json,os,csv
from md_compression import kosinski_decompress,nemesis_decompress
R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts';src=(R/'gba/title_runtime_v06.c').read_text();rom=(R/'references/roms/ps4built.bin').read_bytes()
def function(name):
 m=re.search(r'^static[^\n]*\b'+name+r'\([^;]*?\)\s*\{',src,re.M);assert m,name
 p=m.end();d=1
 while d:d+=(src[p]=='{')-(src[p]=='}');p+=1
 return src[m.start():p]+'\n'
s=(O/'r37_scene_probe.c').read_text().split('int main(')[0]
# Use the current immutable actor bank and native follower synchronization.
blob=(R/'gba/r31_field_poses.bin').read_bytes()
s=re.sub(r'const uint8_t r31_field_pose_data\[\]=\{[^}]+\};','const uint8_t r31_field_pose_data[]={'+','.join(map(str,blob))+'};',s)
s=s.replace('static struct {int x,y;uint8_t facing;} chaz;','static struct {int x,y;uint8_t facing,anim,anim_clock;} chaz;\n'+re.search(r'struct PartyFollowerLite \{.*?\n\};',src,re.S)[0]+'\nstatic struct PartyFollowerLite field_followers[4];static unsigned field_follower_count;static uint16_t hw_chaz_key;\n'+function('field_event_follower_for_id')+function('field_party_anim_period')+function('field_party_anim_advance'))
for name in ['field_event_sync_party_carrier','field_event_set_party_facing']:
 s=s.replace(re.search(r'^static[^\n]*\b'+name+r'\([^\n]+',s,re.M)[0],function(name).strip())
# Ordinary NPC rendering/art refresh is intentionally outside this probe.
s+='static void hw_refresh_field_object_art(unsigned n){assert(n<field_obj_count);}\n'+function('r38_clinic_npc_sync')
s=s.replace('static void field_update_npcs(unsigned x,unsigned y){(void)x;(void)y;r31_actor_update();}', 'static void r38_clinic_npc_sync(void);\nstatic void field_update_npcs(unsigned x,unsigned y){(void)x;(void)y;r31_actor_update();r38_clinic_npc_sync();}')
s+='static unsigned vehicle_palette;static void r30_vehicle_palette(unsigned n){vehicle_palette=n;}\n'
for address in [0x1EC870,0x1F9580]:
 data=kosinski_decompress(rom[address:]);name=f'tree_{address:x}'
 pos=s.index('static unsigned kos_decode(')
 s=s[:pos]+f'static const uint8_t {name}[]={{'+','.join(map(str,data))+'};\n'+s[pos:]
 pos=s.index('default:assert(0);return 0;}',s.index('static unsigned kos_decode('))
 s=s[:pos]+f'case {address}u:assert(cap>=sizeof {name});memcpy(d,{name},sizeof {name});return sizeof {name};\n'+s[pos:]
data=nemesis_decompress(rom[0x1D78CA:]);pos=s.index('static int load_nem_vram_auto(')
s=s[:pos]+'static const uint8_t collapsed_raja_art[]={'+','.join(map(str,data))+'};\n'+s[pos:]
pos=s.index('default:assert(0);}',s.index('static int load_nem_vram_auto('))
s=s[:pos]+'case 0x1D78CAu:assert(tile==0x37F);a=collapsed_raja_art;n=sizeof collapsed_raja_art;break;\n'+s[pos:]
s+='#include "r38_raja_kyra.h"\n'
s+=r'''
static void followers(void){field_follower_count=0;memset(field_followers,0,sizeof field_followers);lead_char_id=ram8r(0xF40A);for(unsigned i=1;i<5;i++){unsigned id=ram8r(0xF40A+i);if(id==255)break;struct PartyFollowerLite*p=&field_followers[field_follower_count++];p->id=id;p->active=1;field_event_sync_party_carrier(i,id,0);}}
static void clinic_objects(void){
 assert(!ps4_load_field_map(0x134));ram16w(0xEC28,0x134);
 unsigned t[]={0x104,0x104,0x248,0x254,0x320,0x74,0x74},base[]={0x2DF,0x2DF,0x327,0x36F,0x377,0,0};
 field_obj_count=7;for(unsigned i=0;i<7;i++){struct FieldObjLite*o=&field_objs[i];o->active=1;o->type=t[i];o->base_tile=base[i];o->facing=0;o->x=0x180;o->y=0x1C0;}
 followers();r33_dialogue_tree(0x1EC870);
}
static void raja_setup(unsigned rslot){
 story_fresh();field_follower_count=0;unsigned ordered[]={0,3,5,7},n=0;
 for(unsigned i=0;i<5;i++){unsigned id=i==rslot?8:ordered[n++];ram8w(0xF40A+i,id);ram16w(0xC000+i*64,id*4+4);ram16w(0xC030+i*64,0x190+i*16);ram16w(0xC034+i*64,0x1C0);ram16w(0xC038+i*64,0x190+i*16);ram16w(0xC03A+i*64,0x1C0);ram16w(0xC006+i*64,8);}
 memset(work_ram+PS4_MACROS,0,160);for(unsigned m=0;m<8;m++)for(unsigned i=0;i<5;i++)ps4_macro_write(m,ram8r(0xF40A+i),1,0);
 ram8w(PS4_CHAR_BASE+8*128+0x16,0x7F);followers();field_obj_count=3;for(unsigned i=0;i<3;i++){field_objs[i].active=1;field_objs[i].type=0x248;field_objs[i].base_tile=0x327;field_objs[i].x=0x180;field_objs[i].y=0x1C0;}
 ram16w(0xEC28,0x132);r33_dialogue_tree(0x1EC870);
}
static void trace_actor(FILE*f,unsigned frame){unsigned b=0xC500;fprintf(f,"%u,%u,%u,%u,%u,%u,%u,%u,%u\n",frame,ram32r(b+0x30),ram32r(b+0x34),ram16r(b+6),ram16r(b+0x28),ram16r(b+0x2A),ram8r(b+0x10),ram8r(b+0x11),ram8r(b+0x13));}
int main(int argc,char**argv){
 assert(argc==3);FILE*f=fopen(argv[1],"rb");assert(f&&fread(rom,1,sizeof rom,f)==sizeof rom);fclose(f);unsigned x=0xF8,y=0x168;
 for(unsigned rslot=1;rslot<5;rslot++){
  raja_setup(rslot);unsigned initial_tick=tick,initial_updates=map_updates;
  assert(r38_cutscene_raja_sick(&x,&y));assert(flags[0x94]&&!flags[0xA0]&&field_custom_resume_event==0xAF&&field_custom_resume_phase==0&&field_event_reload_pending);
  assert(tick-initial_tick==129&&map_updates-initial_updates==120);assert(ram16r(0xEC28)==0x134&&ram16r(0xEC2A)==0x133&&ram16r(0xEC48)==0x34&&ram16r(0xEC4A)==0x38&&ram16r(0xEC44)==0&&ram16r(0xEC46)==0);
  unsigned expected[]={0,3,5,7,255};for(unsigned i=0;i<5;i++)assert(ram8r(0xF40A+i)==expected[i]);for(unsigned q=0;q<64;q++)assert(!ram8r(0xC100+q));assert(!ram8r(PS4_CHAR_BASE+8*128+0x16));
  for(unsigned m=0;m<8;m++)for(unsigned i=0;i<5;i++){unsigned b=PS4_MACROS+m*20+i*4;if(i<4)assert(ram8r(b)==expected[i]&&ram8r(b+1)==1);else assert(!ram8r(b+1));}
  assert(field_objs[7].type==0x374&&field_objs[7].base_tile==0x37F&&r31_actor_pose(&field_objs[7]));assert(text_count==2&&texts[0].tree==0x1EC870&&texts[0].stop==0xFF&&texts[1].stop==0xF7);
  unsigned offset62=field_dialogue_entry(0x62)-field_dialogue;const uint8_t*seam62=field_dialogue_saved_cursor;assert(!seam62); // queue intentionally clears pre-reload cursor
  clinic_objects();field_event_reload_pending=0;x=0x108;y=0x168;initial_tick=tick;initial_updates=map_updates;
  assert(r38_story_resume(&x,&y));assert(tick-initial_tick==147&&map_updates-initial_updates==143);assert(field_custom_resume_event==255&&!field_event_reload_pending&&ram8r(0xECEC)==0xA9);
  assert(text_count==5&&texts[2].tree==0x1EC870&&texts[2].offset>offset62&&texts[2].stop==0xFF&&texts[3].stop==0xF7&&texts[4].stop==0xFF);
  unsigned positions[4][2]={{0x1A0,0x1C0},{0x1B0,0x1C0},{0x1B0,0x1D0},{0x1A0,0x1E0}};for(unsigned i=0;i<4;i++){assert(ram16r(0xC030+i*64)==positions[i][0]&&ram16r(0xC034+i*64)==positions[i][1]&&!ram16r(0xC006+i*64));}
  assert(x==0x108&&y==0x168&&field_objs[8].type==0x8194&&field_objs[8].x==0x1C0&&field_objs[8].y==0x210&&field_objs[8].base_tile==0x327&&r31_actor_pose(&field_objs[8])->facing==20);
  assert(ram32r(0xC508)==0x4A504&&ram8r(0xC513)==0x40&&ram16r(0xC516)==0x327);
 }
 // Live clinic NPC state is synchronized before the copied record is consumed.
 clinic_objects();ram32w(0xC388,0x4A504);field_objs[2].x=0x188;field_objs[2].y=0x1E0;field_objs[2].facing=4;field_objs[2].anim=3;ram8w(0xC390,3);ram8w(0xC391,4);r38_clinic_npc_sync();assert(ram16r(0xC3B0)==0x188&&ram16r(0xC3B4)==0x1E0&&ram16r(0xC386)==4&&!ram8r(0xC390)&&!ram8r(0xC391));
 // Literal 64-byte copy must retain every source field not explicitly overwritten.
 story_fresh();field_obj_count=3;field_objs[2].type=0x248;field_objs[2].base_tile=0x327;field_objs[2].blocks=1;field_objs[2].dialog=0x64;
 uint8_t before[64];for(unsigned q=0;q<64;q++)before[q]=(uint8_t)(q*3+1);before[0x10]=1;before[0x11]=2;before[0x13]=0x40;before[0x16]=3;before[0x17]=0x27;before[0x32]=before[0x33]=before[0x36]=before[0x37]=0;memcpy(work_ram+0xC380,before,64);
 r38_messenger_copy();assert(!memcmp(work_ram+0xC380,before,64));for(unsigned q=0;q<64;q++){unsigned changed=q<2||(q>=6&&q<8)||(q>=0x20&&q<0x2C)||(q>=0x30&&q<0x32)||(q>=0x34&&q<0x36)||(q>=0x38&&q<0x3C);if(!changed)assert(ram8r(0xC500+q)==before[q]);}
 f=fopen(argv[2],"w");assert(f);ram8w(0xECE0,1);for(unsigned i=0;i<48;i++){r31_actor_update_one(8);trace_actor(f,i);}fclose(f);assert(field_objs[8].x==0x1C0&&field_objs[8].y==0x210);
 // Saya and Esper resolve separate immutable source art at identical faces/frames.
 struct FieldObjLite o=field_objs[8];o.facing=0;o.anim=0;o.base_tile=0x2B8;const struct R31FieldPose*saya=r31_actor_pose(&o);o.base_tile=0x327;const struct R31FieldPose*esper=r31_actor_pose(&o);assert(saya&&esper&&saya->offset!=esper->offset&&esper->facing==16);
 // Source equal-distance facing leaves orientation unchanged, without a VB.
 raja_setup(4);r33_party_position(0,0x100,0x100,12);unsigned old=tick;r38_face_raja(0,0x110,0x110,x,y);assert(ram16r(0xC006)==12&&tick==old&&!ram32r(0xC020)&&!ram32r(0xC024));
 // Real Kyra F7 pause, all source panel actions, join/macro/flag, exact map/vehicle handoff.
 story_fresh();followers();assert(r38_cutscene_meeting_kyra(&x,&y));assert(field_custom_resume_event==0xAF&&field_custom_resume_phase==1&&field_event_reload_pending&&flags[0xA0]&&ram8r(0xF40E)==9);
 assert(ram16r(0xEC28)==1&&ram16r(0xEC2A)==0xFFFF&&ram16r(0xEC48)==0x174&&ram16r(0xEC4A)==0x1C&&ram16r(0xEC44)==0&&ram16r(0xEC46)==0);
 assert(text_count==3&&texts[0].tree==0x1F9580&&texts[0].stop==0xFF&&texts[1].stop==0xF7&&texts[2].stop==0xFF&&!field_cutscene_panels);
 unsigned panels[]={0x140,0x99,0x9A,0x9B,0x9C,0x9D};assert(panel_count==6);for(unsigned i=0;i<6;i++)assert(panel_id[i]==panels[i]);assert(panel_frames[0]==1&&panel_frames[1]==1&&panel_frames[3]==120);
 assert(ram8r(PS4_MACROS+16)==9&&ram8r(PS4_MACROS+17)==1);
 assert(!ps4_load_field_map(1));field_event_reload_pending=0;chaz.x=0xBA0;chaz.y=0xE0;ram16w(0xC030,0xBA0);ram16w(0xC034,0xE0);followers();assert(field_follower_count==4);unsigned px=chaz.x,py=chaz.y;
 assert(r38_story_resume(&x,&y));assert(field_custom_resume_event==255&&ram16r(0xF43C)==2&&ram16r(0xC000)==0xB4&&(ram8r(0xEC4E)&1)&&ram8r(0xECEC)==0x8D&&vehicle_palette==2&&!field_follower_count);
 assert(chaz.x==(int)px&&chaz.y==(int)py&&ram16r(0xC030)==px&&ram16r(0xC034)==py);for(unsigned i=1;i<5;i++)assert(!ram16r(0xC000+i*64));
 field_custom_resume_event=0xAF;field_custom_resume_phase=9;assert(!r38_story_resume(&x,&y));
 puts("PASS R38 real-map requests, actual text/seams/actions, Raja slot/macro compaction, copied Esper record/controller/art, Kyra join and exact Ice Digger handoff (map/UI/ordinary NPCs/hardware stubbed)");
}
'''
c=O/'r38_scene_probe.c';c.write_text(s)
subprocess.run(['gcc','-std=c11','-O1','-g','-fsanitize=address,undefined','-I'+str(R/'gba'),str(c),'-o',str(O/'r38_scene_probe')],check=True)
trace=O/'R38_ESPER_TRACE.csv';subprocess.run([str(O/'r38_scene_probe'),str(R/'references/roms/ps4built.bin'),str(trace)],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
# Independent source FieldObj_GetAutoInput/Move/UpdateStepDuration/Animate2.
u32=lambda p:int.from_bytes(rom[p:p+4],'big');x,y=0x190<<16,0x240<<16;face=8;steps=[0,0];vx=vy=0;frame,duration=1,2;expected=[]
for i in range(48):
 if not any(steps):
  if x!=0x1C0<<16:vx=2*65536;vy=0;face=8;steps[0]=16*256
  elif y!=0x210<<16:vx=0;vy=-2*65536;face=4;steps[1]=16*256
  else:vx=vy=0;frame=0;duration=1
 for a,v in enumerate([vx,vy]):steps[a]-=abs(v)>>8
 x+=vx;y+=vy;seq=u32(0x4A504+face);duration-=1
 if duration<0:frame=(frame+1)%(rom[seq]&127);duration=rom[seq+1+(frame if rom[seq]&128 else 0)]
 expected.append([i,x,y,face,*steps,frame,duration,0x40])
actual=[list(map(int,q)) for q in csv.reader(trace.open())];assert actual==expected,next(((i,a,b) for i,(a,b) in enumerate(zip(actual,expected)) if a!=b),None)
report={'scope':__doc__,'raja_positions_tested':[1,2,3,4],'opening_map_updates':120,'opening_facing_vblanks':9,'clinic_map_updates':143,'clinic_non_dialogue_vblanks':147,'esper_reference_frames':48,'literal_64_byte_copy':True,'kyra_panels':[0x140,0x99,0x9A,0x9B,0x9C,0x9D],'ice_digger_authored_xy':[0xBA0,0xE0],'new_poses':17,'appended_pose_bytes':4352,'emulator_playthrough':False}
(O/'R38_SOURCE_PROBE.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS independent original Esper movement/step/animation reference: 48 frames')
