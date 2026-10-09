#!/usr/bin/env python3
"""Compile R35 handlers unchanged with real-ROM text, tables and actors.
Map loading, dialogue presentation, party walking and hardware DMA are stubs.
Camera arithmetic, snow simulation, source object copy, seams and clocks are real.
Run check_r31_story.py and check_r34_scenes.py first to prepare the base harness.
"""
from pathlib import Path
import json,os,re,subprocess
from md_compression import kosinski_decompress
R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts'
rom=(R/'references/roms/ps4built.bin').read_bytes()
src=(R/'gba/title_runtime_v06.c').read_text()
s=(O/'r34_scene_probe.c').read_text().split('#include "r34_story_cutscenes.h"')[0]
s=s.replace('assert(work_ram[0xC49D]==0x82);assert(work_ram[0xC302]&2);assert(!(work_ram[0xC4C2]&2));','')
s=s.replace('static unsigned map_updates,','static unsigned sound_count;static struct {unsigned cue,at;} sounds[2048];\nstatic unsigned map_updates,',1)
s=s.replace('if(a==0x500A){','if(a==0x500A){assert(sound_count<2048);sounds[sound_count].cue=q;sounds[sound_count++].at=tick;',1)
s=s.replace('static void hw_update_objects(unsigned x,unsigned y){r31_actor_draw(x,y);}','static void hw_update_objects(unsigned x,unsigned y);')
s=s.replace('memset(work_ram+0xC000,0,320);return 0;','memset(work_ram+0xC000,0,0xB00);memset(field_objs,0,sizeof field_objs);field_obj_count=0;return 0;')
trees={a:kosinski_decompress(rom[a:]) for a in [0x1E99F0,0x1F81B0]}
for a,data in trees.items():
 pos=s.index('static unsigned kos_decode')
 s=s[:pos]+f'static const uint8_t tree_{a:x}[]={{'+','.join(map(str,data))+'};\n'+s[pos:]
 pos=s.index('default:assert(0);return 0;}}')
 s=s[:pos]+f'case {a}u:assert(cap>={len(data)});memcpy(d,tree_{a:x},{len(data)});return {len(data)};\n'+s[pos:]
s+=r'''
static uint8_t field_pause_fb[38400];
static uint8_t field_hblank_active_index,field_hscroll_mode,hw_hscroll_ring_valid,hw_last_cam_valid,field_hscroll_dirty,field_hscroll_prepared;
static unsigned hw_last_camx,hw_last_camy;static int hw_hsa_tx,hw_hsa_ty,hw_hsb_tx,hw_hsb_ty;
#define FIELD_HBLANK_ROWS 161u
static uint16_t physical_regs[2][161*8],REG_BG0CNT,REG_BG1CNT,REG_BG2CNT,REG_BG3CNT;
static uint16_t* field_hblank_table(unsigned n){assert(n<2);return physical_regs[n];}
static void field_publish_layout_dirty(int x,int y){(void)x;(void)y;}
static int field_near_tile_origin(int x,int old,unsigned span){(void)old;(void)span;return x;}
static void hw_stream_hscroll_plane(unsigned p,int x,int y){assert(p<2);if(p){hw_hsb_tx=x;hw_hsb_ty=y;}else{hw_hsa_tx=x;hw_hsa_ty=y;}}
static void hw_fill_hscroll_plane(unsigned p,int x,int y){hw_stream_hscroll_plane(p,x,y);}
static void field_hscroll_rearm(void){}
static void field_update_npcs(unsigned x,unsigned y){(void)x;(void)y;r31_actor_update();}
static void field_palette_copy_saved_to_live(void){}
static unsigned in_pan,pan_frames,visible_peak;static FILE* trace;
static void hw_update_objects(unsigned x,unsigned y){
 r31_actor_draw(x,y);
 if(in_pan){
  assert(pan_frames<1000);pan_frames++;
  fprintf(trace,"%u,%u,%u,%u,%u,%u,%u,%u,%u\n",ram32r(0xEF94),ram32r(0xEF90),ram32r(0xEF9C),ram32r(0xEF98),ram32r(0xEC50),ram32r(0xEC54),ram32r(0xEC58),ram32r(0xEC5C),cram[1]);
  unsigned expected=0,actual=0;
  for(unsigned i=0;i<field_obj_count;i++){
   struct FieldObjLite*o=&field_objs[i];if(!o->active||!r31_actor_type(o->type)||(ram8r(0xC302+i*64)&2))continue;
   const struct R31FieldPose*p=r31_actor_pose(o);assert(p);int px=(int)o->x-(int)x+p->x,py=(int)o->y-(int)y+p->y;
   if(px>-(int)p->w&&px<240&&py>-(int)p->h&&py<160)expected++;
  }
  for(unsigned oi=44;oi<108;oi++)if(OAM16[oi*4]!=160)actual++;
  assert(actual==expected);if(expected>visible_peak)visible_peak=expected;
  uint16_t* r=physical_regs[field_hblank_active_index];
  for(unsigned row=0;row<161;row++){assert(r[row*8]==(x&511)&&r[row*8+1]==(y&255));assert(r[row*8+2]==(ram16r(0xEF9C)&511)&&r[row*8+3]==(ram16r(0xEF98)&255));}
  assert(hw_hsa_ty==(int)(y>>3)&&hw_hsb_ty==(int)(ram16r(0xEF98)>>3));
 }
}
#include "r34_story_cutscenes.h"
#include "r35_story_cutscenes.h"
static void incoming(void){
 reset(0);sound_count=text_count=edit_count=panel_count=map_updates=reloads=field_event_reload_pending=field_custom_resume_phase=0;field_custom_resume_event=255;
 uint8_t ids[5]={0,5,3,7,255};for(unsigned i=0;i<5;i++){ram8w(0xF40A+i,ids[i]);if(i<4)ps4_macro_write(0,ids[i],1,0);}
 ram8w(0xECE0,1);memset(cram,0,sizeof cram);
}
'''
# One independent reference implements the source instructions and viewport.
x=bx=0;y=by=280*65536;vx=0x40000;vy=-0x20000;bvx=0x60000;bvy=-0x40000
counter=0;blink=0;rows=[];launches=[]
records=[tuple(int.from_bytes(rom[p+i:p+i+2],'big') for i in range(0,12,2)) for p in range(0x76EDA,0x770EA,12)]
while x>>16<0x558:
 if counter in {q[0] for q in records}:launches.append((len(rows),counter))
 x=min(1728*65536,max(0,x+vx));y=min(1824*65536,max(0,y+vy))
 bx=min(1728*65536,max(0,bx+bvx));by=min(1824*65536,max(0,by+bvy))
 # Camera step is cleared by clamp; the local velocity still survives.
 steps=[vx,vy,bvx,bvy]
 if y==0:steps[1]=0
 if by==0:steps[3]=0
 if (len(rows)+1)%10==0:blink^=1
 rows.append([q&0xffffffff for q in [x,y,bx,by,*steps,0 if blink else 14 if len(rows)>=10 else 0]])
 vx=max(0,vx-0x178);bvy=min(0,bvy+0x1000)
 if y>>16<=0x78:y=(0x78<<16)|(y&65535);vy=0;counter+=1
 if x>>16>=0x1F8:bvy=0;bvx=vx;bx=x;by=y
data=trees[0x1E99F0];entry=data.split(b'\xff')[8];assert entry.count(b'\xf7')==5
base=sum(len(q)+1 for q in data.split(b'\xff')[:8]);offsets=[base]+[base+i+1 for i,q in enumerate(entry) if q==0xF7]
s+='static const unsigned raja_offsets[]={'+','.join(map(str,offsets))+'};\n'
s+=r'''
int main(int argc,char**argv){assert(argc==3);FILE*f=fopen(argv[1],"rb");assert(f&&fread(rom,1,sizeof rom,f)==sizeof rom);fclose(f);unsigned x=0,y=280;
 incoming();ram8w(0xF40D,255);r31_actor_spawn(0,0x24,0x1F0,0xC0,0,0x1EB);ram16w(0xC006,8);r33_dialogue_tree(0x1F81B0);
 assert(r35_cutscene_meeting_wren(&x,&y));assert(ram8r(0xF40D)==7&&ram8r(0xF40E)==255&&flags[0x70]);assert(panel_count==1&&panel_id[0]==0x76&&panel_frames[0]==20);
 assert(text_count==1&&texts[0].tree==0x1F81B0&&texts[0].stop==0xFF&&ram16r(0xC0F0)==0x1F0&&ram16r(0xC0F4)==0xC0);
 for(unsigned q=0;q<32;q++)assert(!ram8r(0xC300+q));assert(ram16r(0xC330)==0x1F0&&!field_objs[0].active);assert(ram8r(PS4_MACROS+3*4)==7&&ram8r(PS4_MACROS+3*4+1)==1);
 // Snow pre-animation recycling, literal fractions and the final clear.
 incoming();r31_actor_spawn(3,0x1F4,0x2B0,0x180,0,0x2C3);ram32w(0xC3E0,0x28000);ram32w(0xC3E4,0u-0x48000u);ram8w(0xC3D0,4);ram8w(0xC3D1,1);
 r31_actor_update_one(3);assert(ram16r(0xC3DC)==2&&ram32r(0xC3F0)==0x2F28000&&ram32r(0xC3F4)==0x17B8000&&sound_count==1);
 ram16w(0xC3DC,1);ram8w(0xC3D0,4);ram8w(0xC3D1,1);r31_actor_update_one(3);assert(!field_objs[3].active&&!ram16r(0xC3C0)&&ram32r(0xC3F0)==0x2F50000);
 // Exact original crash trajectory and visible OBJ allocation on every frame.
 incoming();field_map.row_fg=field_map.col_fg=field_map.row_bg=field_map.col_bg=63;
 r31_actor_spawn(0,0x24,0x5D0,0x1C0,8,0x1EB);x=0;y=280;pan_frames=visible_peak=0;trace=fopen(argv[2],"w");assert(trace);in_pan=1;
 r35_crash_pan(&x,&y);in_pan=0;fclose(trace);assert(pan_frames==599&&x>=0x558&&y==0x78&&visible_peak>0&&max_tiles<=768);assert(!map_updates);
 // Renderer culls only presentation: off-screen snow still advances.
 incoming();r31_actor_spawn(3,0x1F4,0x900,0x180,0,0x2C3);ram32w(0xC3E0,0x10000);r31_actor_update();assert(field_objs[3].x==0x901);unsigned before=uploads;r31_actor_draw(0,0);assert(uploads==before);
 // Every source and native object byte survives the bit-3 map reload.
 incoming();for(unsigned i=0;i<0xB00;i++)ram8w(0xC000+i,(i*37+11)&255);r31_actor_spawn(0,0x24,0x5D0,0x190,8,0x1EB);
 uint8_t saved[0xB00],objects[sizeof field_objs];memcpy(saved,work_ram+0xC000,sizeof saved);memcpy(objects,field_objs,sizeof objects);unsigned count=field_obj_count;
 assert(!r35_load_preserving_objects(0x14C));assert(!memcmp(saved,work_ram+0xC000,sizeof saved)&&!memcmp(objects,field_objs,sizeof objects)&&field_obj_count==count);
 // Direct source panel/sound script and real-map continuation requests.
 incoming();r33_dialogue_tree(0x1F81B0);x=0;y=0;assert(r35_cutscene_crash_landing(&x,&y));
 assert(panel_count==3&&panel_id[0]==0x80&&panel_id[1]==0x86&&panel_id[2]==0x87);
 unsigned cues[]={0xBE,0xCE,0xD3,0xD3,0xCE,0xBF,0xBE,0xC0,0xBF,0xBF,0xAD,0xC0,0xC0,0xFB,0x99};
 assert(sound_count==15);for(unsigned i=0;i<15;i++)assert(sounds[i].cue==cues[i]);
 assert(ram16r(0xEC28)==1&&ram16r(0xEC2A)==0x18C&&ram16r(0xEC48)==0x20&&ram16r(0xEC4A)==0xBA&&field_custom_resume_phase==0);
 field_event_reload_pending=0;assert(r35_crash_resume(&x,&y));assert(ram8r(0xF400)==1&&ram16r(0xEC28)==0x14C&&ram16r(0xEC48)==6&&ram16r(0xEC4A)==0x2E&&field_custom_resume_phase==1);
 field_event_reload_pending=0;r31_actor_spawn(0,0x24,0x5D0,0x1C0,8,0x1EB);x=0;y=280;unsigned old_texts=text_count,old_panels=panel_count;
 r33_dialogue_tree(0x1E99F0);assert(r35_crash_resume(&x,&y));assert(flags[0x85]&&!flags[0x88]&&(ram8r(0xEC4E)&8)&&field_custom_resume_phase==2);
 assert(text_count==old_texts+1&&texts[old_texts].offset==raja_offsets[0]&&texts[old_texts].stop==0xF7&&panel_count==old_panels+1&&panel_id[old_panels]==0x88);
 assert(edit_count==20);for(unsigned n=0;n<20;n++){assert(edits[n].plane==1&&edits[n].x==(47+(n&1))*32&&edits[n].y==(9+((n%4)>>1))*32&&edits[n].value==rom8(0x76E58+n));if(n>=4)assert(edits[n].at-edits[n-4].at==6);}
 assert(field_objs[0].x==0x5D0&&field_objs[0].y==0x190&&field_objs[0].facing==8);
 uint8_t raja[64];memcpy(raja,work_ram+0xC300,64);assert(!r35_load_preserving_objects(0x14C));r34_sync_preserved_party();r33_dialogue_tree(0x1E99F0);field_event_reload_pending=0;
 assert(r35_crash_resume(&x,&y));assert(flags[0x85]&&flags[0x88]&&ram8r(0xF40E)==8&&field_custom_resume_event==255&&!ram8r(0xECF4)&&!(ram8r(0xECFE)&1));
 for(unsigned n=0;n<6;n++){assert(texts[old_texts+n].tree==0x1E99F0&&texts[old_texts+n].offset==raja_offsets[n]);assert(texts[old_texts+n].stop==(n==5?0xFF:0xF7));}
 assert(!memcmp(raja,work_ram+0xC100,64));for(unsigned i=0;i<32;i++)assert(!ram8r(0xC300+i));assert(ram16r(0xC330)==0x5D0&&!field_objs[0].active);
 assert(chaz.x==0x600&&chaz.y==0x1A0&&ram8r(PS4_MACROS+4*4)==8&&ram8r(PS4_MACROS+4*4+1)==1&&tree==0x1E99F0);
 puts("PASS R35 Wren join, original text seams, crash panels/cues, all map continuations, per-frame crash trajectory/parallax, snow fractions/recycling and visible OBJ bounds, original BG door clocks, preserved objects and full Raja carrier/macro join (map/UI/party walking hardware stubbed)");
}
'''
# Give the unchanged handler the actual descriptor fields it reads.
s=s.replace('static uint8_t field_pause_fb[38400];','static struct {unsigned row_fg,col_fg,row_bg,col_bg;} field_map;\nstatic uint8_t field_pause_fb[38400];')
c=O/'r35_scene_probe.c';c.write_text(s)
subprocess.run(['gcc','-std=c11','-O1','-g','-fsanitize=address,undefined','-I'+str(R/'gba'),str(c),'-o',str(O/'r35_scene_probe')],check=True)
trace=O/'R35_CRASH_CAMERA.csv'
subprocess.run([str(O/'r35_scene_probe'),str(R/'references/roms/ps4built.bin'),str(trace)],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
actual=[[int(q) for q in line.split(',')] for line in trace.read_text().splitlines()]
assert actual==rows,next(((i,a,b) for i,(a,b) in enumerate(zip(actual,rows)) if a!=b),('length',len(actual),len(rows)))
(O/'R35_SOURCE_SEAMS.json').write_text(json.dumps({'crash_camera_frames':len(rows),'snow_table_records':44,'reference_launches':launches,'raja_tree':'0x1E99F0','entry':8,'f7_seams':5,'scope':__doc__},indent=2)+'\n')
print('PASS independent original-camera reference:',len(rows),'frames,',len(launches),'snow launches')
