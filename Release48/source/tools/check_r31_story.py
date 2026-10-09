#!/usr/bin/env python3
"""Focused source-clock, event-state and native OBJ bounds probe for R31."""
from pathlib import Path
import os,re,subprocess,json
R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts';O.mkdir(exist_ok=True)
s=(R/'gba/title_runtime_v06.c').read_text()
obj=re.search(r'struct FieldObjLite \{.*?\n\};',s,re.S)[0]
pre=r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static uint8_t work_ram[65536],rom[0x300000];
static uint16_t cram[64],OAM16[512],OBJ_VRAM16[16384];
static unsigned tick,cast_tick,megid_tick,layout_count,max_tiles,dialogues,resumes,uploads;
static unsigned punch_cues;static uint8_t flags[256],field_dialogue_last_stop,finished_dialogue;
static const uint8_t* field_dialogue_saved_cursor;
static uint8_t ram8r(uint16_t a){return work_ram[a];}
static void ram8w(uint16_t a,uint8_t q){work_ram[a]=q;if(a==0x500A){if(q==0xBB)cast_tick=tick;if(q==0xC0)megid_tick=tick;if(q==0xBE){punch_cues++;assert(work_ram[0xC49D]==0x82);assert(work_ram[0xC302]&2);assert(!(work_ram[0xC4C2]&2));}}}
static uint16_t ram16r(uint16_t a){return ram8r(a)*256+ram8r((uint16_t)(a+1));}
static void ram16w(uint16_t a,uint16_t q){ram8w(a,q>>8);ram8w((uint16_t)(a+1),q);}
static uint32_t ram32r(uint16_t a){return ((uint32_t)ram16r(a)<<16)|ram16r((uint16_t)(a+2));}
static void ram32w(uint16_t a,uint32_t q){ram16w(a,q>>16);ram16w((uint16_t)(a+2),q);}
static unsigned rom8(unsigned a){assert(a<sizeof rom);return rom[a];}
static unsigned rom16(unsigned a){return rom8(a)*256+rom8(a+1);}
static uint32_t rom32(unsigned a){return ((uint32_t)rom16(a)<<16)|rom16(a+2);}
#define FIELD_MAX_OBJECTS 32u
#define FIELD_OBJ_TILE_BASE 480u
#define DIALOGUE_OBJ_TILE_BASE 768u
struct FieldObjArtStamp {uint32_t key0,key1,epoch;};
static struct FieldObjArtStamp field_obj_art_stamp[32];static unsigned field_obj_art_epoch=1;
static void hw_hide_obj(unsigned i){assert(i<128);OAM16[i*4]=160;}
const uint8_t r31_field_pose_data[68800]={0};
static void dma_copy32(const void* s,void* d,unsigned n){
 assert((const uint8_t*)s>=r31_field_pose_data&&(const uint8_t*)s+n<=r31_field_pose_data+sizeof r31_field_pose_data);
 assert((uint16_t*)d>=OBJ_VRAM16&&(uint8_t*)d+n<=(uint8_t*)OBJ_VRAM16+768*32);
 unsigned end=((uint8_t*)d-(uint8_t*)OBJ_VRAM16+n)/32;if(end>max_tiles)max_tiles=end;uploads++;memcpy(d,s,n);
}
'''
pre=pre.replace('68800',str((R/'gba/r31_field_poses.bin').stat().st_size))
pre+=obj+'\nstatic struct FieldObjLite field_objs[32];static unsigned field_obj_count;\n'
pre+='static void field_obj_init_motion(struct FieldObjLite*o){o->move_kind=0;}\n'
pre+='#include "r31_field_actors.h"\n'
middle=r'''
static uint8_t lead_char_id;
static struct {int x,y;uint8_t facing;} chaz;
static int field_event_party_slot_for_id(uint8_t id){for(unsigned i=0;i<5;i++)if(ram8r(0xF40A+i)==id)return i;return -1;}
static unsigned field_event_source_speed(void){return 2;}
static void field_event_sync_party_carrier(unsigned slot,uint8_t id,uint8_t moved){(void)moved;if(id==lead_char_id){chaz.x=ram16r(0xC030+slot*64);chaz.y=ram16r(0xC034+slot*64);chaz.facing=ram16r(0xC006+slot*64)>>2;}}
static void waitvb(void){tick++;}
static void hw_update_objects(unsigned x,unsigned y){r31_actor_draw(x,y);}
static void field_event_choreography_tick(unsigned x,unsigned y){r31_actor_update();hw_update_objects(x,y);waitvb();}
static void field_event_wait_updates(unsigned n,unsigned*x,unsigned*y){for(unsigned i=0;i<=n;i++)field_event_choreography_tick(*x,*y);}
static void field_event_move_party_id(uint8_t id,uint16_t x,uint16_t y,unsigned*cx,unsigned*cy){(void)cx;(void)cy;int i=field_event_party_slot_for_id(id);assert(i>=0);ram16w(0xC030+i*64,x);ram16w(0xC034+i*64,y);ram16w(0xC038+i*64,x);ram16w(0xC03A+i*64,y);field_event_sync_party_carrier(i,id,1);}
static void field_event_set_party_dest(uint8_t id,uint16_t x,uint16_t y){int i=field_event_party_slot_for_id(id);if(i<0)return;ram16w(0xC038+i*64,x);ram16w(0xC03A+i*64,y);}
static void field_event_set_party_facing(uint8_t id,uint8_t f){int i=field_event_party_slot_for_id(id);if(i<0)return;ram16w(0xC006+i*64,f);if(id==lead_char_id)chaz.facing=f>>2;}
static void field_event_set_source_object_facing(unsigned i,uint8_t f,unsigned*x,unsigned*y){(void)x;(void)y;assert(i<field_obj_count);field_objs[i].facing=f;ram16w(0xC306+i*64,f);}
static void field_event_move_camera_to(uint16_t x,uint16_t y,unsigned step,unsigned*cx,unsigned*cy){(void)step;*cx=x-0x98;*cy=y-0x58;}
static void field_dialogue_show(unsigned id,unsigned x,unsigned y){(void)id;(void)x;(void)y;dialogues++;field_dialogue_last_stop=finished_dialogue?0xFF:0xF7;field_dialogue_saved_cursor=finished_dialogue?0:work_ram;}
static void field_dialogue_resume_saved(unsigned x,unsigned y){(void)x;(void)y;resumes++;}
static int load_nem_vram_auto(unsigned a,unsigned b){(void)a;(void)b;return 1;}
static void field_palette_mirror_live(unsigned n){(void)n;}static void hw_load_palettes(void){}
static int field_layout_write_world(unsigned x,unsigned y,unsigned p,uint8_t q){assert(p==1);unsigned a=0xB000+(y/32)*32+x/32;ram8w(a,q);layout_count++;return 1;}
static void ps4_event_flag_set(unsigned f){flags[f]=1;}
#include "r31_story_events.h"
static void reset(unsigned lead){memset(work_ram,0,sizeof work_ram);memset(field_objs,0,sizeof field_objs);memset(field_obj_art_stamp,0,sizeof field_obj_art_stamp);memset(flags,0,sizeof flags);tick=cast_tick=megid_tick=layout_count=dialogues=resumes=punch_cues=uploads=max_tiles=field_obj_count=0;finished_dialogue=0;field_dialogue_saved_cursor=0;lead_char_id=lead;for(unsigned i=0;i<5;i++)ram8w(0xF40A+i,i<4?i:255);if(lead==1){ram8w(0xF40A,1);ram8w(0xF40B,0);}for(unsigned i=0;i<4;i++){ram16w(0xC030+i*64,0x210);ram16w(0xC034+i*64,0x260);ram16w(0xC038+i*64,0x210);ram16w(0xC03A+i*64,0x260);}chaz.x=0x210;chaz.y=0x260;chaz.facing=1;}
int main(int argc,char**argv){assert(argc==2);FILE*f=fopen(argv[1],"rb");assert(f&&fread(rom,1,sizeof rom,f)==sizeof rom);fclose(f);
 reset(0);r31_actor_spawn(0,0x214,0x1F0,0x240,0,0x179);
 for(unsigned t=1;t<=96;t++){r31_actor_update();tick=t;assert((t>=70)==(cast_tick!=0||t==70));if(t==70)assert(ram16r(0xC35C)==49);if(t==95)assert(!ram8r(0xECF2));}
 assert(ram8r(0xECF2)==1&&ram16r(0xC35C)==23);assert(cast_tick==69);
 for(unsigned lead=0;lead<2;lead++){
  reset(lead);unsigned x=0x150,y=0x200;assert(r31_event_rune_flaeli(&x,&y));assert(flags[0x13]&&layout_count==4);assert(work_ram[0xB1CF]==12&&work_ram[0xB1D0]==13&&work_ram[0xB1EF]==20&&work_ram[0xB1F0]==21);assert(chaz.x==0x1F0&&chaz.y==0x210);assert(!ram16r(0xECF2)&&!(ram8r(0xECFE)&7));assert(megid_tick-cast_tick==27);assert(max_tiles<=768);for(unsigned i=0;i<field_obj_count;i++)assert(!field_objs[i].active);for(unsigned i=0;i<4;i++)assert(!(ram8r(0xC002+i*64)&2));
 }
 reset(0);field_obj_count=9;for(unsigned i=0;i<9;i++){field_objs[i].type=i<3?0x38:0x74;field_objs[i].active=1;field_objs[i].x=0x1E0;field_objs[i].y=0x1E0;field_objs[i].gba_tile=488+i*8;field_objs[i].render_w=i<3?16:0;field_objs[i].render_h=i<3?32:0;}field_objs[8].type=0x1B0;field_objs[8].base_tile=0x48F;field_objs[8].facing=8;r31_actor_load();
 unsigned x=0x148,y=0x160;assert(r31_event_meeting_dorin(&x,&y));assert(flags[0x36]&&punch_cues==1&&resumes==4&&max_tiles<=768);assert(!field_objs[6].active&&!field_objs[9].active);assert(!(ram8r(0xC002+field_event_party_slot_for_id(1)*64)&2));assert(!(ram8r(0xC302)&2));
 reset(0);field_obj_count=1;field_objs[0].active=1;field_objs[0].type=0x38;finished_dialogue=1;assert(r31_event_meeting_dorin(&x,&y));assert(!flags[0x36]&&field_obj_count==1&&!punch_cues);
 /* Reallocation must invalidate a dormant pose whose destination is reused. */
 reset(0);int a=r31_actor_spawn(0,0x214,0x1F0,0x180,0,0x179),b=r31_actor_spawn(1,0x220,0x1F0,0x180,20,0x20C);r31_actor_draw(0x148,0x160);r31_actor_hidden(a,1);r31_actor_draw(0x148,0x160);r31_actor_hidden(a,0);unsigned before=uploads;r31_actor_draw(0x148,0x160);assert(uploads>before);r31_actor_hidden(b,1);r31_actor_draw(0x148,0x160);before=uploads;r31_actor_draw(0x148,0x160);assert(uploads==before);
 puts("PASS R31 source cast/explosion clocks, Rune/Dorin event state, visibility cleanup, tile reuse and OBJ bounds (stubbed movement/dialogue/display)");
}
'''
c=O/'r31_story_probe.c';c.write_text(pre+middle)
subprocess.run(['gcc','-std=c11','-O1','-g','-fsanitize=address,undefined','-I'+str(R/'gba'),str(c),'-o',str(O/'r31_story_probe')],check=True)
subprocess.run([str(O/'r31_story_probe'),str(R/'references/roms/ps4built.bin')],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
report=json.loads((R/'generated/r31_field_poses_report.json').read_text());blob=(R/'gba/r31_field_poses.bin').read_bytes()
assert report['bytes']==len(blob)
assert len([p for p in report['poses'] if p['type'] not in (0x334,0x280,0x284,0x288,0x28C,0x290,0x338,0x374,0x78,0x7C,0x84,0x150,0x1F8,0x1FC,0x20C,0x1A0,0x8194,0x298,0xC,0x24,0x1F4,0x200,0x204,0x208,0x210,0x1B8,0x1BC,0x1C0,0x1C4,0x1C8,0x1CC,0x1D0)])==54
for p in report['poses']:
 assert p['offset']+p['w']*p['h']//2<=len(blob)
 assert p['h']<=128 and p['w']<=(128 if p['type']==0x210 else 64)
print('PASS R31/R32 and R33 immutable pose bounds; prefix bytes are checked by check_r33_preservation.py')
