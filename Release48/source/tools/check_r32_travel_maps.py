#!/usr/bin/env python3
"""Focused actual-header probe: travel state/menu, source rates, eye cadence, HBlank projection."""
from pathlib import Path
import os,re,subprocess,json
R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts';O.mkdir(exist_ok=True)
s=(R/'gba/title_runtime_v06.c').read_text()
pre=r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static uint8_t work_ram[65536],rom[0x300000],md_vram[65536];
static uint8_t flags[256],field_event_reload_pending,field_hscroll_mode,field_hscroll_prepared,field_hscroll_prepared_index,field_hblank_active_index;
static uint16_t hblank[2][161*8];static unsigned native_main_frame_count,removed,draws,published,edges[24],edge_n,edge_i;
static uint8_t field_runtime_plan_valid=1,field_update_plan_count;static const uint8_t*field_update_plan_ids;
static struct {uint32_t map_updates;uint8_t row_fg,row_bg,col_bg;}field_map;
static int field_hscroll_pending_atx,field_hscroll_pending_btx,field_hscroll_pending_ty;
static uint8_t ram8r(uint16_t a){return work_ram[a];}static void ram8w(uint16_t a,uint8_t q){work_ram[a]=q;}
static uint16_t ram16r(uint16_t a){return ram8r(a)*256+ram8r((uint16_t)(a+1));}static void ram16w(uint16_t a,uint16_t q){ram8w(a,q>>8);ram8w((uint16_t)(a+1),q);}
static uint32_t ram32r(uint16_t a){return ((uint32_t)ram16r(a)<<16)|ram16r((uint16_t)(a+2));}static void ram32w(uint16_t a,uint32_t q){ram16w(a,q>>16);ram16w((uint16_t)(a+2),q);}
static unsigned rom8(unsigned a){assert(a<sizeof rom);return rom[a];}static unsigned rom16(unsigned a){return rom8(a)*256+rom8(a+1);}
static void ps4_ui_wait_buttons_up(void){}static void ps4_ui_clear(void){}static void ps4_ui_box(unsigned a,unsigned b,unsigned c,unsigned d){assert(a<c&&b<d&&c<=240&&d<=160);}
static void battle_draw_text(unsigned a,unsigned b,const char*c,unsigned d){(void)a;(void)b;(void)c;(void)d;}
static void pause_draw_cursor(unsigned x,unsigned y,unsigned c){(void)c;assert(x<240&&y+16<=160);}
static void ps4_ui_draw_rom_name(unsigned x,unsigned y,unsigned table,unsigned index,unsigned delimiter,unsigned color){(void)color;assert(x<240&&y+16<=160);unsigned p=table;while(index--){while(rom8(p++)!=delimiter){}}unsigned n=0;while(rom8(p++)!=delimiter){assert(++n<80);}draws++;}
static void ps4_ui_present(void){}static uint8_t ps4_ui_wait_edge(uint8_t*p){(void)p;assert(edge_i<edge_n);return edges[edge_i++];}
static void ps4_ui_message(const char*a,const char*b){(void)a;(void)b;}
static void ps4_inventory_remove(unsigned slot){assert(slot<40);removed++;}
static int ps4_flag_test_type(unsigned t,unsigned id){assert(t==2);return flags[id];}
static void dma_copy32(const void*s,void*d,unsigned n){assert(n==0x9E0);assert((const uint8_t*)s>=work_ram&&(const uint8_t*)s+n<=work_ram+65536);assert(d==md_vram+0x4060);memcpy(d,s,n);}
static void field_gba_ready_invalidate(unsigned t,unsigned n){assert(t==0x203&&n==0x4F);}
static void hw_publish_canonical_tiles(unsigned t,unsigned n){assert(t==0x203&&n==0x4F);published++;}
#include "r32_travel_state.h"
#include "r32_travel_menu.h"
#include "r32_map_updates.h"
static uint16_t*field_hblank_table(unsigned i){assert(i<2);return hblank[i];}
static unsigned field_wrap_pixel_int(int x,unsigned span){assert(span);int q=x%(int)span;return q<0?q+span:q;}
'''
a=s.index('static void field_hscroll_source_pair(');b=s.index('__attribute__',a);pre+=s[a:b]
a=s.index('static void field_hscroll_prepare(');b=s.index('\n__attribute__',a);pre+=s[a:b]
pre+=r'''
static void reset(void){memset(work_ram,0,sizeof work_ram);memset(flags,0,sizeof flags);removed=draws=published=edge_n=edge_i=0;field_event_reload_pending=field_hscroll_mode=field_hblank_active_index=0;field_runtime_plan_valid=1;}
static void input(unsigned e){edges[edge_n++]=e;}
int main(int argc,char**argv){assert(argc==2);FILE*f=fopen(argv[1],"rb");assert(f&&fread(rom,1,sizeof rom,f)==sizeof rom);fclose(f);
 uint8_t choices[16];uint32_t table;
 /* Every authored place-name match registers exactly its town/escape bit. */
 for(unsigned p=0x664BC;rom16(p)!=0xFFFF;p+=6){reset();unsigned flag=rom8(p+5);r32_visit_place(rom16(p),rom16(p+2),0x100);if(flag<128)assert(ram8r(0xF160+(flag>>3))==(128u>>(flag&7)));assert(ram8r(0xED51)==(flag>=128&&flag!=254?(flag&127):0));if(flag==135){r32_visit_place(rom16(p),rom16(p+2),0x520);assert(ram8r(0xED51)==8);}}
 /* Planet catalogs, blocked contexts and unvisited choices. */
 reset();ram16w(0xF160,0xA000);assert(r32_town_destinations(choices,&table)==2&&choices[0]==0&&choices[1]==2&&table==0x6115A);assert(!r32_teleport_destination(1,table,1));
 for(unsigned world=0;world<3;world++){reset();ram8w(0xF400,world);if(world==0)ram16w(0xF160,0x8000);if(world==1)ram16w(0xF162,0x8000);if(world==2)ram8w(0xF163,0x40);assert(r32_town_destinations(choices,&table)==1);assert(r32_teleport_destination(1,table,0));assert(ram16r(0xEC28)==world&&ram16r(0xEC2A)==rom16(table+2)&&ram16r(0xEC48)==rom16(table+4)&&ram16r(0xEC4A)==rom16(table+6));ram8w(0xED50,1);assert(!r32_town_destinations(choices,&table));ram8w(0xED50,0);ram8w(0xED51,1);assert(!r32_town_destinations(choices,&table));}
 reset();ram8w(0xF400,3);assert(!r32_town_destinations(choices,&table));
 /* Cancel/invalid uses are free; success changes state and consumes once. */
 reset();ram16w(0xF160,0x8000);input(0x10);assert(!r32_use_teleport(1,3,0,0)&&!removed&&!field_event_reload_pending);
 reset();assert(!r32_use_teleport(1,3,0,0)&&!removed);assert(!r32_use_teleport(0,3,0,0)&&!removed);
 reset();ram16w(0xF160,0xA000);input(2);input(0x20);assert(r32_use_teleport(1,3,0,0)&&removed==1&&field_event_reload_pending);assert(ram16r(0xEC2A)==rom16(0x6115A+18));
 reset();ram16w(0xF160,0x8000);ram16w(0xC812,7);input(0x20);assert(!r32_use_teleport(1,-1,0xC800,8)&&ram16r(0xC812)==7&&!field_event_reload_pending);
 reset();ram8w(0xED51,7);ram16w(0xEC28,0x80);ram16w(0xC812,10);assert(r32_use_teleport(0,-1,0xC800,4)&&ram16r(0xC812)==6&&ram16r(0xEC2A)==0x80);unsigned p=0x61212+56;assert(ram16r(0xEC28)==rom16(p)&&ram16r(0xEC44)==rom8(p+6)&&ram16r(0xEC46)==rom8(p+7));
 /* Full list wrapping and scrolling use actual names and bounded rows. */
 reset();ram16w(0xF160,0xFFF8);unsigned n=r32_town_destinations(choices,&table);assert(n==13);input(1);input(0x20);assert(r32_select_town(choices,n,table)==12);assert(draws==10);
 /* Independent expected rate arithmetic, including signed rounding. */
 unsigned ids[]={5,7,13,15,16};int32_t probes[]={0,65536,131072,-65536,-1,17,-17};
 for(unsigned i=0;i<5;i++)for(unsigned k=0;k<7;k++){int32_t v=probes[k];reset();ram32w(0xEC58,(uint32_t)v);ram32w(0xEC5C,(uint32_t)v);r32_map_rate(ids[i]);unsigned sh=i<2?4:i==2?2:i==3?3:1,sy=i==1?2:sh;int32_t ax=v>=0?v/(1<<sh):-(((-v)+(1<<sh)-1)/(1<<sh)),ay=v>=0?v/(1<<sy):-(((-v)+(1<<sy)-1)/(1<<sy));assert((int32_t)ram32r(0xEC58)==v-ax&&(int32_t)ram32r(0xEC5C)==v-ay);}
 /* Fractional camera movement persists and reaches the HBlank table. */
 reset();static const uint8_t rate[]={7};field_update_plan_ids=rate;field_update_plan_count=1;field_map.row_fg=field_map.row_bg=31;field_map.col_bg=31;r32_camera_delta(100,100,0);ram16w(0xEF94,100);ram16w(0xEF90,100);r32_camera_delta(102,102,1);assert(ram32r(0xEF9C)==(101u*65536+57344)&&ram32r(0xEF98)==(101u*65536+32768));ram16w(0xEF94,102);ram16w(0xEF90,102);field_hscroll_prepare(102,102);for(unsigned y=0;y<161;y++){assert(hblank[1][y*8]==102&&hblank[1][y*8+1]==102&&hblank[1][y*8+2]==101&&hblank[1][y*8+3]==101);}assert(field_hscroll_pending_btx==12);
 assert(r32_bg_axis(0,-65536,32,240)==0);assert(r32_bg_axis(0,1000*65536,32,240)==784*65536);
 /* Original eye frame sequence 1,2,2,1,0,0 every eighth update. */
 reset();for(unsigned frame=0;frame<3;frame++)memset(work_ram+frame*0x9E0,frame+1,0x9E0);ram16w(0xECF2,0);unsigned expected[]={1,2,2,1,0,0};for(unsigned i=0;i<6;i++){native_main_frame_count=i*8;r32_garuberk_update();assert(ram8r(0xECF2)==expected[i]);for(unsigned j=0;j<0x9E0;j++)assert(md_vram[0x4060+j]==expected[i]+1);}assert(published==6);native_main_frame_count=49;r32_garuberk_update();assert(published==6);native_main_frame_count=56;flags[0x15]=1;r32_garuberk_update();assert(published==6);flags[0x15]=0;flags[0x17]=1;r32_garuberk_update();assert(published==6);
 puts("PASS R32 authored place records, three planet catalogs, cancel/cost handling, menu bounds, signed rates, fractional HBlank projection, Garuberk source cadence/flags/tile bounds (stubbed UI/display)");
}
'''
c=O/'r32_travel_maps_probe.c';c.write_text(pre)
subprocess.run(['gcc','-std=c11','-O1','-g','-fsanitize=address,undefined','-I'+str(R/'gba'),str(c),'-o',str(O/'r32_travel_maps_probe')],check=True)
subprocess.run([str(O/'r32_travel_maps_probe'),str(R/'references/roms/ps4built.bin')],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
assert 'id==0x27u||id==0x28u' in s and 'item==0x84u||item==0x85u' in s
q=json.loads((R/'generated/psc_runtime_plan_report.json').read_text());active=set(q['active_update_ids']);assert {5,7,13,15,16,34}<=active
for plan in q['plans']:
 if plan['valid']:assert plan['updates']==[x for x in plan['authored_updates'] if x in active]
print('PASS travel dispatch IDs and authored map continuation ordering; no asset hashes')
