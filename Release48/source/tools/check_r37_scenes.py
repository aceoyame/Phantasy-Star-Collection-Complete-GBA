#!/usr/bin/env python3
"""Compile unchanged R37 handlers under ASan/UBSan with actual ROM text/art.
Hardware, map loading, panel drawing, dialogue display and combat are stubs.
New scene state, source snow controllers, indexed space drawing and clocks
are real. A separate instruction reference checks every snow/camera update.
Requires check_r31_story/r34_scenes/r35_scenes/r36_spaceship first.
"""
from pathlib import Path
import json,os,re,subprocess,csv
from md_compression import nemesis_decompress
R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts'
rom=(R/'references/roms/ps4built.bin').read_bytes();src=(R/'gba/title_runtime_v06.c').read_text()
def function(name):
 m=re.search(r'^static[^\n]*\b'+name+r'\([^;]*?\)\s*\{',src,re.M);assert m,name
 p=m.end();d=1
 while d:d+=(src[p]=='{')-(src[p]=='}');p+=1
 return src[m.start():p]+'\n'
s=(O/'r36_spaceship_probe.c').read_text().split('int main(')[0]
s=s.replace('REG_BLDY,field_cutscene_panels,field_active_panel_count;', 'REG_BLDY;')
s='static unsigned field_cutscene_panels,field_active_panel_count;\n'+s
# Observe the source-published state, before this frame's new launches.
s=s.replace('static unsigned in_pan,','static unsigned place_frame,place_enabled,place_visible_peak;static FILE*place_trace;\nstatic void place_publish(unsigned,unsigned);\nstatic unsigned in_pan,',1)
s=s.replace('r31_actor_draw(x,y);\n if(trace_mode','r31_actor_draw(x,y);if(place_enabled)place_publish(x,y);\n if(trace_mode',1)
s+=r'''
#define PS4_INV_BASE 0xF410u
static unsigned palette_count,palette_at[16],palette_color[16],palette_value[16],fade_outs,fade_ins,clears;
static void field_dialogue_commit_palette(void){
 assert(palette_count<16);unsigned i=palette_count++;palette_at[i]=tick;
 palette_color[i]=i>=3?25:24;palette_value[i]=cram[palette_color[i]];
 for(unsigned j=0;j<64;j++)field_pause_pal[j]=md2gba(cram[j]);
}
'''
# Inventory helpers are the actual native code, not approximations.
for name in ['ps4_inventory_add','ps4_inventory_remove','ps4_inventory_remove_first']:s+=function(name)
arts=[0x1D3C1E,0x1D4234,0x1D46C6]
for a in arts:
 data=nemesis_decompress(rom[a:]);assert len(data)<=0x800
 s+=f'static const uint8_t place_art_{a:x}[]={{'+','.join(map(str,data))+'};\n'
s+='static unsigned nemesis_fast(unsigned a,uint8_t*d,unsigned cap){const uint8_t*p=0;unsigned n=0;switch(a){\n'
for a in arts:s+=f'case {a}u:p=place_art_{a:x};n=sizeof place_art_{a:x};break;\n'
s+='default:assert(0);}assert(n<=cap);memcpy(d,p,n);return n;}\n'
s+='#include "r37_landale.h"\n'
s+=r'''
static void place_publish(unsigned x,unsigned y){
 assert(place_frame<600);fprintf(place_trace,"%u,-1,%u,%u,0,0,0,0,0,0,0\n",place_frame,x,y);
 unsigned actual=0,expected=0;
 for(unsigned i=0;i<field_obj_count;i++){
  struct FieldObjLite*o=&field_objs[i];if(!o->active||!r37_place_type(o->type))continue;unsigned b=0xC300+i*64;
  const struct R31FieldPose*p=r31_actor_pose(o);assert(p&&p->w==32&&p->h==32);
  fprintf(place_trace,"%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",place_frame,i,x,y,o->type,ram32r(b+0x30),ram32r(b+0x34),ram16r(b+0x1C),o->anim,ram8r(b+0x11),o->base_tile);
  int px=(((int)o->x-(int)x+p->x+0x800)&0xFFF)-0x800,py=(((int)o->y-(int)y+p->y+0x800)&0xFFF)-0x800;
  if(px>-(int)p->w&&px<240&&py>-(int)p->h&&py<160)expected++;
 }
 for(unsigned i=44;i<108;i++)if(OAM16[i*4]!=160){
  actual++;assert((OAM16[i*4+2]>>12)==0);unsigned priority=(OAM16[i*4+2]>>10)&3;assert(priority<=1);
 }
 if(actual!=expected)fprintf(stderr,"place frame %u cam %u,%u actual %u expected %u max_tiles %u\n",place_frame,x,y,actual,expected,max_tiles);assert(actual==expected);if(actual>place_visible_peak)place_visible_peak=actual;place_frame++;
}
static void story_fresh(void){fresh();palette_count=fade_outs=fade_ins=clears=place_frame=place_visible_peak=0;capture_dir=".";ram16w(0xEC28,0x15F);ram8w(0xF400,1);}
static void enter_dezo(unsigned*x,unsigned*y){
 assert(!ps4_load_field_map(1));ram16w(0xEC28,1);field_map.row_fg=field_map.col_fg=field_map.row_bg=field_map.col_bg=127;
 ram16w(0xC030,0x90);ram16w(0xC034,0x490);chaz.x=0x90;chaz.y=0x490;*x=0xFF8;*y=0x438;
}
int main(int argc,char**argv){
 assert(argc==3);FILE*f=fopen(argv[1],"rb");assert(f&&fread(rom,1,sizeof rom,f)==sizeof rom);fclose(f);unsigned x=0,y=0;
 story_fresh();for(unsigned i=44;i<108;i++)hw_hide_obj(i);r33_dialogue_tree(0x1E99F0);assert(r37_cutscene_landale(&x,&y));
 assert(field_custom_resume_event==0xAE&&field_custom_resume_phase==0&&field_event_reload_pending&&!flags[0x82]);
 assert(ram16r(0xEC28)==1&&ram16r(0xEC2A)==0x15F&&ram16r(0xEC48)==0x12&&ram16r(0xEC4A)==0x92&&ram16r(0xEC44)==8&&ram16r(0xEC46)==12&&!(ram8r(0xEC4E)&8));
 assert(panel_count==3&&panel_id[0]==0x8B&&panel_id[1]==0x8C&&panel_id[2]==0x8D&&panel_frames[0]==30);
 assert(text_count==1&&texts[0].tree==0x1E99F0&&texts[0].stop==0xFF&&fade_outs==2&&!field_cutscene_panels);
 enter_dezo(&x,&y);field_event_reload_pending=0;unsigned old_updates=map_updates;place_trace=fopen(argv[2],"w");assert(place_trace);place_enabled=1;
 assert(r37_story_resume(&x,&y));place_enabled=0;fclose(place_trace);
 assert(flags[0x82]&&field_custom_resume_event==255&&ram8r(0xECEC)==0x99&&x==0xFF8&&y==0x438);
 assert(place_frame==465&&map_updates-old_updates==465&&place_visible_peak>=4&&max_tiles<=768);
 assert(edit_count==1&&edits[0].x==192&&edits[0].y==1152&&edits[0].plane==1&&edits[0].value==0x2C&&edits[0].at-old_updates==372);
 unsigned grave=0;for(unsigned i=0;i<sound_count;i++)if(sounds[i].cue==0xDD)grave++;assert(grave==47);
 for(unsigned i=0;i<field_obj_count;i++)assert(!field_objs[i].active);
 assert(!memcmp(work_ram,place_art_1d3c1e,sizeof place_art_1d3c1e)&&!memcmp(work_ram+0x800,place_art_1d4234,sizeof place_art_1d4234)&&!memcmp(work_ram+0x1000,place_art_1d46c6,sizeof place_art_1d46c6));
 // Earlier Motavia effects never enter the new Dezolis actor bank.
 ram16w(0xEC28,0);assert(!r31_actor_type(0x78)&&!r31_actor_type(0x7C)&&!r31_actor_type(0x84));
 // Dark Force aftermath uses the actual space renderer and real map request.
 story_fresh();ram16w(0xEC28,0x198);r33_dialogue_tree(0x1F81B0);flags[0x80]=1;memset(work_ram+0xCB00,0xA5,0x800);
 assert(r37_cutscene_dark_force1(&x,&y));assert(map_updates==120&&ui_frames==301&&ram8r(0xF400)==3);
 assert(field_custom_resume_event==0xAE&&field_custom_resume_phase==1&&field_event_reload_pending&&ram16r(0xEC28)==0x18E&&ram16r(0xEC2A)==0xFFFF&&ram16r(0xEC48)==0x3E&&ram16r(0xEC4A)==0x20&&ram16r(0xEC44)==0&&ram16r(0xEC46)==8);
 for(unsigned i=0xC000;i<0xC300;i++)assert(!ram8r(i));for(unsigned i=0xCB00;i<0xD000;i++)assert(!ram8r(i));for(unsigned i=0xD000;i<0xD300;i++)assert(ram8r(i)==0xA5);
 assert(ram32r(0xEF90)==0&&!flags[0x89]&&!battle_calls&&texts[0].tree==0x1F81B0&&texts[0].stop==0xFF);
 unsigned opening_panels[]={0x8E,0x8F,0x180,0x90,0x91};assert(panel_count==5);for(unsigned i=0;i<5;i++)assert(panel_id[i]==opening_panels[i]);
 // Load overwrites dialogue RAM, exactly as a real map handoff does.
 assert(!ps4_load_field_map(0x18E));r33_dialogue_tree(0x1F81B0);field_event_reload_pending=0;
 for(unsigned i=0;i<40;i++)ram8w(PS4_INV_BASE+i,(uint8_t)(i==17?0x9A:1+i));
 assert(r37_story_resume(&x,&y));assert(flags[0x89]&&field_custom_resume_event==255&&ram8r(0xECEC)==0x89&&!field_event_reload_pending&&!field_cutscene_panels);
 assert(palette_count==6);unsigned values[]={0x682,0x8C4,0xCE6,0x68,0x28C,0x4CE},colors[]={24,24,24,25,25,25},holds[]={4,4,60,4,4,120};
 for(unsigned i=0;i<6;i++){assert(palette_value[i]==values[i]&&palette_color[i]==colors[i]);if(i<5)assert(palette_at[i+1]-palette_at[i]==holds[i]);}
 unsigned radar_start=0,radar_stop=0;for(unsigned i=0;i<sound_count;i++){if(sounds[i].cue==0xF8)radar_start=sounds[i].at;if(sounds[i].cue==0xFD)radar_stop=sounds[i].at;}
 assert(radar_start&&radar_stop-radar_start==196&&cram[24]==0xCE6&&cram[25]==0x4CE);
 assert(text_count==2&&texts[1].tree==0x1F81B0&&texts[1].stop==0xFF&&ram8r(PS4_INV_BASE+39)==0x97);
 for(unsigned i=0;i<39;i++)assert(ram8r(PS4_INV_BASE+i)==(uint8_t)(i<17?1+i:2+i));
 unsigned late_panels[]={0x92,0x93,0x94,0x95,0x96,0x97};assert(panel_count==11);for(unsigned i=0;i<6;i++)assert(panel_id[i+5]==late_panels[i]);assert(panel_frames[5]==60&&panel_frames[6]==120);
 // Remove just one Canceller, preserve another, and append Ice Digger.
 story_fresh();ram16w(0xEC28,0x18E);r33_dialogue_tree(0x1F81B0);field_custom_resume_event=0xAE;field_custom_resume_phase=1;ram8w(PS4_INV_BASE,0x9A);ram8w(PS4_INV_BASE+1,4);ram8w(PS4_INV_BASE+2,0x9A);
 assert(r37_story_resume(&x,&y));assert(ram8r(PS4_INV_BASE)==4&&ram8r(PS4_INV_BASE+1)==0x9A&&ram8r(PS4_INV_BASE+2)==0x97&&flags[0x89]);
 story_fresh();field_custom_resume_event=0xAE;field_custom_resume_phase=9;assert(!r37_story_resume(&x,&y)&&!flags[0x89]);
 puts("PASS R37 real map continuations, original text/panel actions, full snow scene/camera, source radar holds/cues, genuine inventory exchange and flags (hardware/map/dialogue display/combat stubbed)");
}
'''
# Instrument the old shared stubs. The production handlers remain unchanged.
s=s.replace('static void field_palette_fade_in(void){}static void field_palette_fade_out(void){}','static unsigned fade_outs,fade_ins,clears;\nstatic void field_palette_fade_in(void){fade_ins++;}static void field_palette_fade_out(void){fade_outs++;}')
s=s.replace('palette_value[16],fade_outs,fade_ins,clears;','palette_value[16];')
s=s.replace('static void pause_panel_action(unsigned a,unsigned n){(void)a;(void)n;}','static void pause_panel_action(unsigned,unsigned);')
s=s.replace('static void field_cutscene_panels_clear(void){}','static void field_cutscene_panels_clear(void);')
pos=s.index('#include "r37_landale.h"')
s=s[:pos]+'''static void pause_panel_action(unsigned a,unsigned n){
 if(a==0){field_cutscene_panels=1;field_active_panel_count++;assert(panel_count<32);panel_id[panel_count]=n;panel_frames[panel_count++]=0;}
 else if(a==1){if(field_active_panel_count)field_active_panel_count--;}
 else if(a==2)field_active_panel_count=0;
}
static void field_cutscene_panels_clear(void){clears++;field_active_panel_count=0;}
'''+s[pos:]
s=s.replace('panel_frames[panel_count++]=n;tick+=n;','panel_frames[panel_count++]=n;tick+=n;field_cutscene_panels=1;field_active_panel_count++;')
# Execute only the F2 actions used by these actual entries, rejecting others.
s=s.replace('else if(op==0xF2&&p<end){unsigned a=*p++;n=a==0?2:((a==3||a==4||a==11)?1:0);}', '''else if(op==0xF2&&p<end){unsigned a=*p++;n=a==0?2:((a==3||a==4||a==11)?1:0);
 assert((unsigned)(end-p)>=n);if(a==0)pause_panel_action(a,p[0]*256+p[1]);else if(a==1||a==2)pause_panel_action(a,0);else if(a==3||a==4)ram8w(0x500A,p[0]);else if(a==11)ps4_event_flag_set(p[0]);else assert(0);}''')
c=O/'r37_scene_probe.c';c.write_text(s)
subprocess.run(['gcc','-std=c11','-O1','-g','-fsanitize=address,undefined','-I'+str(R/'gba'),str(c),'-o',str(O/'r37_scene_probe')],check=True)
trace=O/'R37_DEZO_TRACE.csv'
subprocess.run([str(O/'r37_scene_probe'),str(R/'references/roms/ps4built.bin'),str(trace)],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))

# Independent instruction reference: original 12-bit camera / 16.16 actors.
u16=lambda p:int.from_bytes(rom[p:p+2],'big');u32=lambda p:int.from_bytes(rom[p:p+4],'big')
records=[tuple(u16(p+j) for j in (0,2,4,6)) for p in range(0x6C1BC,0x6C27C,8)]
actors={};frames=[];cam=[0xFF8,0x438];launches=[]
def update():
 for i,a in list(actors.items()):
  a['life']-=1;a['x']=(a['x']+a['vx'])&0xffffffff;a['y']=(a['y']+a['vy'])&0xffffffff
  a['duration']-=1
  if a['duration']<0:
   a['frame']=(a['frame']+1)%(rom[a['sequence']]&127)
   a['duration']=rom[a['sequence']+1+(a['frame'] if rom[a['sequence']]&128 else 0)]
  if not a['life']:del actors[i]
 rows=[[-1,*cam,0,0,0,0,0,0,0]]
 rows.extend([i,*cam,a['type'],a['x'],a['y'],a['life'],a['frame'],a['duration'],a['base']] for i,a in sorted(actors.items()))
 frames.append(rows)
def camera(x,y,speed):
 for axis,target in enumerate([(x-152)&4095,(y-88)&4095]):
  delta=target-cam[axis]
  if delta>2047:delta-=4096
  elif delta<-2047:delta+=4096
  step=-speed if delta<0 else speed
  for _ in range(abs(delta)//speed):cam[axis]=(cam[axis]+step)&4095;update()
def spawn(t,x,y,base):
 i=0
 while i in actors:i+=1
 face={0x78:0,0x7C:4,0x84:12}[t]
 actors[i]={'type':t,'x':x*65536,'y':y*65536,'vx':-16384 if t==0x7C else 0,'vy':16384 if t==0x7C else 0,'life':{0x78:59,0x7C:31,0x84:188}[t],'frame':0,'duration':{0x78:15,0x7C:8,0x84:24}[t],'base':base,'sequence':u32(0x4DE24+face)}
camera(192,1152,1)
for counter in range(368,-1,-1):
 update()
 for timer,x,y,base in records:
  if timer==counter:spawn(0x7C,x,y,base);launches.append((len(frames)-1,counter,x,y,base));break
 if counter==248:spawn(0x78,192,1152,0x4AE);spawn(0x84,192,1152,0x49E)
camera(144,1168,2)
assert len(frames)==465 and len(launches)==22 and not actors
actual=[]
for row in csv.reader(trace.open()):
 q=list(map(int,row));frame=q.pop(0)
 while len(actual)<=frame:actual.append([])
 actual[frame].append(q)
assert actual==frames,next(((i,a,b) for i,(a,b) in enumerate(zip(actual,frames)) if a!=b),('length',len(actual),len(frames)))
evidence={'scope':__doc__,'camera_and_effect_frames':len(frames),'spaceport_effect_frames':369,'snow_table_rows':24,'first_match_launches':launches,'radar_holds':[4,4,60,4,4,120],'radar_vblanks':196,'new_poses':14,'retained_r36_pose_bytes':148960,'appended_pose_bytes':7168,'maps':['Dezolis 1 / Hangar 15F','Zelan F1 18E / FFFF'],'ice_digger_full_inventory_exchange':True,'emulator_playthrough':False}
(O/'R37_SOURCE_PROBE.json').write_text(json.dumps(evidence,indent=2)+'\n')
print('PASS independent source snow/camera reference:',len(frames),'frames,',len(launches),'first-match launches')
