#!/usr/bin/env python3
"""Focused actual vehicle/actor headers probe with original source ROM."""
from pathlib import Path
import os,re,subprocess
R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts'
s=(R/'gba/title_runtime_v06.c').read_text()
# Reuse the existing actor harness; replace the event-test main only.
pre=(O/'r31_story_probe.c').read_text().split('int main(int argc,char**argv)')[0]
pre=pre.replace('static struct {int x,y;uint8_t facing;} chaz;','static struct {int x,y;int8_t dx,dy;uint8_t facing,step_left,anim,anim_clock;} chaz;')
pre+=r'''
static struct {uint16_t map_id;uint8_t is_world;}field_map;
static int dirty_x,dirty_y,probes_x[4],probes_y[4];static unsigned dirty_n,probe_n,walk_n,follower_n;static uint8_t collisions[4],field_step_completed;
static uint8_t field_collision_at(int x,int y,int dx,int dy){assert(x==chaz.x&&y==chaz.y&&probe_n<4);probes_x[probe_n]=dx;probes_y[probe_n]=dy;return collisions[probe_n++];}
static int field_wrap_signed(int v,int span){if(v<0)v+=span;else if(v>=span)v-=span;return v;}
static void field_dirty_chunk_mark(unsigned plane,unsigned cx,unsigned cy){assert(plane==1);dirty_n++;dirty_x=cx;dirty_y=cy;}
#include "r32_world_bg.h"
static void field_sync_native_party_to_source(void){ram16w(0xC030,chaz.x);ram16w(0xC034,chaz.y);ram16w(0xC038,chaz.x+chaz.dx*chaz.step_left);ram16w(0xC03A,chaz.y+chaz.dy*chaz.step_left);}
static int field_object_blocks(int x,int y){(void)x;(void)y;return 0;}static int field_follower_blocks_lead(int x,int y){(void)x;(void)y;return 0;}
static void field_followers_begin_chain(void){follower_n++;}static void field_followers_advance(void){follower_n++;}static void field_party_anim_advance(uint8_t id,uint8_t*a,uint8_t*c){(void)id;if(++*c==5){*c=0;*a^=1;}}
'''
a=s.index('static int field_direction_vector(');b=s.index('static int field_direction_primary_rel',a);pre+=s[a:b]
pre+=r'''
static int field_resolve_move(uint8_t p,int*x,int*y,uint8_t*f){walk_n++;return field_direction_vector(p&15,x,y,f);}
#include "r32_vehicle_move.h"
'''
a=s.index('static void begin_step(');b=s.index('\n__attribute__',a);pre+=s[a:b]
pre+=r'''
static void vehicle_reset(unsigned v){reset(0);chaz.x=1024;chaz.y=1024;chaz.dx=chaz.dy=0;chaz.facing=chaz.step_left=chaz.anim=chaz.anim_clock=0;dirty_n=probe_n=walk_n=follower_n=field_step_completed=0;memset(collisions,0,sizeof collisions);field_map.map_id=v==2?1:0;field_map.is_world=1;ram16w(0xF43C,v);r32_world_bg_reset();}
int main(int argc,char**argv){assert(argc==2);FILE*f=fopen(argv[1],"rb");assert(f&&fread(rom,1,sizeof rom,f)==sizeof rom);fclose(f);
 unsigned dirs[]={1,2,4,8};int footprints[4][4]={ {0,16,-48,-32},{0,16,16,32},{-32,-16,-16,0},{32,48,-16,0} };
 for(unsigned v=1;v<=3;v++)for(unsigned di=0;di<4;di++)for(unsigned q=0;q<16;q++){
  vehicle_reset(v);collisions[2]=q;uint8_t c=r32_vehicle_collision(dirs[di]);assert(c==q&&probe_n==4);assert(probes_x[0]==footprints[di][0]&&probes_x[1]==footprints[di][1]&&probes_y[0]==footprints[di][2]&&probes_y[2]==footprints[di][3]);
  probe_n=0;begin_step(dirs[di]);unsigned allowed=q==0||q==1||q==10||(v==3&&q==9)||(v==2&&q==11);assert(chaz.step_left==(allowed?32:0));assert(!walk_n&&!follower_n);
 }
 /* Source normal/double-speed modes, complete step and source destinations. */
 for(unsigned v=1;v<=3;v++)for(unsigned mode=0;mode<2;mode++)for(unsigned di=0;di<4;di++){
  vehicle_reset(v);ram8w(0xECE0,mode);begin_step(dirs[di]);unsigned speed=(v==3?4:2)<<mode;assert(r32_vehicle_speed()==speed&&ram16r(0xC038)==1024+chaz.dx*32&&ram16r(0xC03A)==1024+chaz.dy*32);unsigned ticks=0;int dx=chaz.dx,dy=chaz.dy;while(chaz.step_left){advance_step();assert(++ticks<=16);}assert(ticks==32/speed&&chaz.x==1024+dx*32&&chaz.y==1024+dy*32&&field_step_completed);
 }
 vehicle_reset(0);begin_step(8);assert(walk_n==1&&chaz.step_left==16);advance_step();assert(chaz.x==1026&&follower_n==2);
 /* Ice mutation is at the source's doubled directional offset; three dust
  * records retain authored vectors, duration and lifetime. */
 for(unsigned di=0;di<4;di++){
  vehicle_reset(2);unsigned d=dirs[di];collisions[0]=11;begin_step(d);assert(field_obj_count==3&&ram8r(0x500A)==0xEE&&dirty_n==1);
  int dx=(int8_t)rom8(0x45CDC+d*2),dy=(int8_t)rom8(0x45CDD+d*2);unsigned cx=(1024+dx*2)/32,cy=(1024+dy*2)/32;assert(dirty_x==(int)cx&&dirty_y==(int)cy&&r32_world_bg_chunk(cx,cy)==rom8(0x4612A+d));
  for(unsigned i=0;i<3;i++){unsigned b=0xC300+i*64;assert(field_objs[i].type==0x150&&!field_objs[i].blocks);assert(ram16r(b+0x1C)==(i?30:40)&&ram8r(b+0x11)==(i?6:8));}
  for(unsigned t=0;t<40;t++){r31_actor_update();r31_actor_draw(960,960);if(t==29)assert(field_objs[0].active&&!field_objs[1].active&&!field_objs[2].active);}
  for(unsigned i=0;i<3;i++)assert(!field_objs[i].active);assert(max_tiles<=768);
 }
 /* Five-pose stop: actor motion stops after reaching frame 4. */
 vehicle_reset(2);r32_vehicle_break_ice(8);for(unsigned t=0;t<40;t++){r31_actor_update();for(unsigned i=0;i<3;i++)if(field_objs[i].active&&field_objs[i].anim==4){unsigned b=0xC300+i*64;assert(!ram32r(b+0x20)&&!ram32r(b+0x24));}}
 vehicle_reset(2);r32_world_bg_write(1024,1040,0x4F);r32_vehicle_finish_ice();assert(r32_world_bg_chunk(32,32)==0x4D);
 /* Mutable world pages retain writes within the source four-page ring and
  * reload from ROM when another page replaces the same slot. */
 vehicle_reset(2);uint8_t old=r32_world_bg_chunk(1,8);r32_world_bg_write(32,256,0x4F);assert(r32_world_bg_chunk(1,8)==0x4F);(void)r32_world_bg_chunk(1,40);assert(r32_world_bg_chunk(1,8)==old);
 vehicle_reset(1);chaz.x=4080;begin_step(8);while(chaz.step_left)advance_step();assert(chaz.x==16);
 puts("PASS R32 all vehicle terrain classes and four source footprint directions, ROM speeds/32-pixel steps, walking preservation, Ice Digger chunks/three dust clocks/OBJ bounds, page replacement and world seam (stubbed display/collision samples)");
}
'''
c=O/'r32_vehicle_probe.c';c.write_text(pre)
subprocess.run(['gcc','-std=c11','-O1','-g','-fsanitize=address,undefined','-I'+str(R/'gba'),str(c),'-o',str(O/'r32_vehicle_probe')],check=True)
subprocess.run([str(O/'r32_vehicle_probe'),str(R/'references/roms/ps4built.bin')],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
