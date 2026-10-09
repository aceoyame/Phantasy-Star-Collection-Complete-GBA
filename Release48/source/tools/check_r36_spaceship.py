#!/usr/bin/env python3
"""Run unchanged R36 scenes/actors with original ROM art/text and source clocks.
Hardware, actual map loading and battle execution are stubbed. Input, all new
scene logic, indexed-pixel composition, actor controllers and timing are real.
Requires check_r31_story.py, check_r34_scenes.py, check_r35_scenes.py first.
"""
from pathlib import Path
import json,os,re,subprocess,csv
from md_compression import nemesis_decompress,enigma_decompress
R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts';rom=(R/'references/roms/ps4built.bin').read_bytes();src=(R/'gba/title_runtime_v06.c').read_text()
def function(name):
 m=re.search(r'^static[^\n]*\b'+name+r'\([^;]*?\)\s*\{',src,re.M);assert m,name
 p=m.end();depth=1
 while depth:depth+=(src[p]=='{')-(src[p]=='}');p+=1
 return src[m.start():p]+'\n'
s=(O/'r35_scene_probe.c').read_text().split('int main(')[0]
s=re.sub(r'const uint8_t r31_field_pose_data\[\d+\]=\{0\};', 'const uint8_t r31_field_pose_data[]={'+','.join(map(str,(R/'gba/r31_field_poses.bin').read_bytes()))+'};',s)
s=s.replace('static void dma_copy32(const void* s,void* d,unsigned n){','static void dma_copy32(const void* s,void* d,unsigned n){\n if((const uint8_t*)s<r31_field_pose_data||(const uint8_t*)s>=r31_field_pose_data+sizeof r31_field_pose_data){memcpy(d,s,n);return;}')
s=re.sub(r'static int load_nem_vram_auto\(.*?\n','static int load_nem_vram_auto(unsigned,unsigned);\n',s)
s=s.replace('static struct {unsigned row_fg,col_fg,row_bg,col_bg;} field_map;','static struct {unsigned row_fg,col_fg,row_bg,col_bg;} field_map;')
s=s.replace('static unsigned in_pan,','static int trace_mode=-1;static unsigned trace_frame;static FILE* flight_trace;\nstatic unsigned in_pan,')
s=s.replace('r31_actor_draw(x,y);\n if(in_pan){','r31_actor_draw(x,y);\n if(trace_mode>=0){fprintf(flight_trace,"%d,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\\n",trace_mode,trace_frame++,ram32r(0xEF90),ram32r(0xEF98),ram32r(0xEC54),ram32r(0xEC5C),ram32r(0xC330),ram32r(0xC334),ram16r(0xC346),ram16r(0xC35C),ram8r(0xC350));}\n if(in_pan){')
s+=r'''
static uint8_t panel_art_scratch[16384],md_vram[65536],scene_vram[76800];static uint16_t field_pause_pal[256],panel_map_scratch[2048];
static unsigned REG_DMA0CNT,REG_DISPCNT,REG_BLDCNT,REG_BLDY,field_cutscene_panels,field_active_panel_count;
#define FORCE_BLANK 128u
#define MODE4_PAGE1 (scene_vram+38400)
static unsigned ui_frames,pad_index,pad_count,pad_skip_at,battle_calls,battle_id,battle_defeat,ps4_return_to_title,unresolved;
static uint8_t pad_script[64];
static uint8_t md_io_read_psiv_pad(void){if(pad_skip_at&&tick>=pad_skip_at)return 0x20;if(pad_count&&ui_frames&&ui_frames<=pad_count)return pad_script[ui_frames-1];return 0;}
static void ps4_ui_wait_buttons_up(void){}
static void ps4_ui_colors(void){field_pause_pal[80]=0x7fff;field_pause_pal[83]=0x1f;field_pause_pal[110]=0x0842;}
static void ps4_ui_box(unsigned x,unsigned y,unsigned x1,unsigned y1){for(unsigned j=y;j<y1&&j<160;j++)for(unsigned i=x;i<x1&&i<240;i++)field_pause_fb[j*240+i]=110;}
#include "ps4_window_assets.h"
'''
for n in ['ps4_window_font_index','pause_draw_window_glyph','ps4_ui_draw_rom_name']:s+=function(n)
s+=r'''
static void pause_draw_cursor(unsigned x,unsigned y,uint8_t c){for(unsigned j=0;j<8;j++)field_pause_fb[(y+j)*240+x]=c;}
static unsigned picture;static const char* capture_dir;
static void dump_picture(unsigned n){char p[1024];snprintf(p,sizeof p,"%s/R36_SCENE_%u.bin",capture_dir,n);FILE*f=fopen(p,"wb");assert(f);assert(fwrite(field_pause_fb,1,38400,f)==38400);assert(fwrite(field_pause_pal,2,256,f)==256);fclose(f);}
static void ps4_ui_present(void){waitvb();ui_frames++;if(picture&&ui_frames==1)dump_picture(picture);if(picture>=2&&ui_frames==160){assert(cram[47]==0);dump_picture(picture+2);}if(picture>=2&&ui_frames==200){assert(cram[47]==0xEEE);dump_picture(picture+4);}}
static int field_dispatch_event_index(unsigned index,unsigned*x,unsigned*y){(void)x;(void)y;unresolved=index;assert(index==0x801B);return 1;}
static int field_event_run_battle_request(uint8_t index,unsigned*x,unsigned*y){(void)x;(void)y;battle_calls++;battle_id=index;assert(index==8&&flags[0x71]&&!ram8r(0xECF4)&&ram8r(0xECEC)==0xA9&&(ram8r(0xEC4E)&0x88)==0x88);ps4_return_to_title=battle_defeat;return 1;}
'''
arts=[0x1D7FAE,0x1D8A0E,0x1D79B4,0x1D7E08,0x1D932C,0x1D8A1E,0x1D900C]
for address in arts:
 data=nemesis_decompress(rom[address:]);s+=f'static const uint8_t art_{address:x}[]={{'+','.join(map(str,data))+'};\n'
s+='static int load_nem_vram_auto(unsigned address,unsigned tile){const uint8_t*a=0;unsigned n=0;switch(address){\n'
for address in arts:s+=f'case {address}u:a=art_{address:x};n=sizeof art_{address:x};break;\n'
s+='default:assert(0);}assert(tile*32+n<=65536);memcpy(md_vram+tile*32,a,n);return n;}\n'
for address,base in [(0x1DDB88,0x100),(0x1DDC2A,0x2100),(0x1DDBFE,0x100),(0x1DDC06,0x100),(0x1DDC20,0x100),(0x1DDC12,0x100),(0x1DDBF2,0x100),(0x1DD922,0x6400)]:
 data=enigma_decompress(rom[address:],base);s+=f'static const uint16_t map_{address:x}[]={{'+','.join(map(str,data))+'};\n'
s+='static unsigned enigma_decode(unsigned address,uint16_t base,uint16_t*d,unsigned cap){(void)base;const uint16_t*a=0;unsigned n=0;switch(address){\n'
for address in [0x1DDB88,0x1DDC2A,0x1DDBFE,0x1DDC06,0x1DDC20,0x1DDC12,0x1DDBF2,0x1DD922]:s+=f'case {address}u:a=map_{address:x};n=sizeof map_{address:x}/2;break;\n'
s+='default:assert(0);}assert(n<=cap);memcpy(d,a,n*2);return n;}\n'
for name in ['tile_px','md2gba']:
 if name=='md2gba':s+='static const uint8_t cvt[8]={0,4,9,13,18,22,27,31};\n'
 s+=function(name)
for name,size in [('src_x',240),('src_y',160)]:s+=re.search(r'static const uint16_t '+name+r'\['+str(size)+r'\]=\{[^;]*;',src)[0]+'\n'
s+='#include "r36_spaceship.h"\n'
s+=r'''
static void fresh(void){incoming();ui_frames=pad_count=pad_skip_at=picture=battle_calls=battle_defeat=ps4_return_to_title=unresolved=0;field_cutscene_panels=field_active_panel_count=0;memset(scene_vram,0,sizeof scene_vram);memset(md_vram,0,sizeof md_vram);memset(field_pause_fb,0,sizeof field_pause_fb);memset(field_pause_pal,0,sizeof field_pause_pal);field_map.row_fg=field_map.col_fg=field_map.row_bg=field_map.col_bg=63;ram16w(0xEC28,0x18C);ram8w(0xF400,3);ram16w(0xC030,400);ram16w(0xC034,480);ram32w(0xEF94,100u<<16);ram32w(0xEF9C,100u<<16);ram32w(0xEF90,200u<<16);ram32w(0xEF98,200u<<16);}
int main(int argc,char**argv){assert(argc==5);FILE*f=fopen(argv[1],"rb");assert(f&&fread(rom,1,sizeof rom,f)==sizeof rom);fclose(f);capture_dir=argv[4];flight_trace=fopen(argv[3],"w");assert(flight_trace);unsigned x=100,y=200;
 // Masks have source priority and exclude current WorldIndex.
 fresh();assert(r36_destinations(0)==2&&ram8r(0xED43)==0&&ram8r(0xED44)==4&&ram8r(0xED42)==0x88);flags[rom8(0x63EFA)]=1;assert(r36_destinations(0)==1&&ram8r(0xED42)==0x90&&ram8r(0xED43)==0); // AlysFound $63EFA
 flags[rom8(0x63EF2)]=1;assert(r36_destinations(0)==4&&ram8r(0xED42)==0xF8);assert(r36_destinations(1)==1&&ram8r(0xED43)==4);
 // Camp/Speak confirm, Cancel only bit4, up/down wrap; source planet art.
 fresh();flags[rom8(0x63EF2)]=1;unsigned n=r36_destinations(0);pad_script[0]=1;pad_script[1]=0;pad_script[2]=0x40;pad_count=3;picture=1;assert(r36_select_destination(n)==4&&ram8r(0xEC9E)==3);assert(ram8r(0xF400)==3&&field_hscroll_mode==0&&!REG_DMA0CNT);assert(cram[14]==0x400&&cram[15]==0x400);
 fresh();r36_destinations(1);pad_script[0]=0x10;pad_count=1;assert(r36_select_destination(1)==-1&&ram8r(0xF400)==3);
 // Exact actor timers: small jet phases, frozen wings, hidden beams/jets.
 fresh();r31_actor_spawn(1,0x204,400,480,4,0x580);for(unsigned i=1;i<=228;i++){r31_actor_update_one(1);assert(field_objs[1].facing==(i<223?4:(i<227?8:12)));}assert(ram16r(0xC35C)==228);
 fresh();r31_actor_spawn(0,0x1BC,400,480,4,0x580);for(unsigned i=0;i<127;i++)r31_actor_update_one(0);assert(ram16r(0xC31C)==1&&ram8r(0xC311)==126);
 fresh();r31_actor_spawn(0,0x1C8,400,480,20,0x580);for(unsigned i=0;i<226;i++)r31_actor_update_one(0);assert(ram8r(0xC302)&2);r31_actor_update_one(0);assert(!(ram8r(0xC302)&2)&&ram16r(0xC31C)==227);
 fresh();r31_actor_spawn(0,0x1D0,400,480,28,0x580);for(unsigned i=0;i<24;i++)r31_actor_update_one(0);assert(ram8r(0xC302)&2);for(unsigned i=0;i<24;i++)r31_actor_update_one(0);assert(!(ram8r(0xC302)&2));
 // Full source ascent clocks for both ships, every frame traced independently.
 for(unsigned landale=0;landale<2;landale++)for(unsigned mode=0;mode<3;mode++){
  fresh();flags[0x80]=landale;x=100;y=200;if(mode==0){ram16w(0xEC28,0);field_map.col_fg=field_map.col_bg=127;}
  r36_hide_party();r36_spawn_ship(0,&x,&y);unsigned before=tick,updates=map_updates;trace_frame=0;trace_mode=mode+landale*3;r36_ascent(mode,&x,&y);trace_mode=-1;
  assert(tick-before==(mode==0?479:(mode==1?228:112)));assert(map_updates-updates==(mode==0?419:(mode==1?228:112)));
  if(mode==2){unsigned cues=0;for(unsigned i=0;i<sound_count;i++)cues+=sounds[i].cue==0xD1;assert(cues==112);assert(ram32r(0xC330)==(landale?404u:400u)*65536u+6216u);}
 }
 // Source permitted skipping does not skip sabotage frames.
 fresh();r36_spawn_ship(0,&x,&y);unsigned before=tick;pad_skip_at=tick+10;r36_ascent(1,&x,&y);assert(tick-before==10);
 fresh();r36_spawn_ship(0,&x,&y);before=tick;pad_skip_at=tick+10;r36_ascent(2,&x,&y);assert(tick-before==112);
 // Every planet uses its own original art; space pass lasts 300 full frames.
 for(unsigned world=0;world<6;world++)for(unsigned landale=0;landale<2;landale++){
  fresh();ram8w(0xF400,world);flags[0x80]=landale;picture=world==0?(landale?3:2):0;before=tick;r36_space_pass();assert(tick-before==301&&ui_frames==301&&field_hscroll_mode==0&&!REG_DMA0CNT);assert(field_objs[0].type==0x210&&field_objs[0].facing==world*4&&cram[47]==0xEEE);assert(ram32r(0xC320)==0xFFFF2000u+301u*0xC0u);assert(ram16r(0xC31C)==0);
 }
 // Landing: original Y camera only, one settle frame, 257 deceleration updates.
 fresh();r36_field_camera_begin(100,200);assert(field_hscroll_mode==4&&REG_BG0CNT==((16u<<8)|(1u<<14)));ram16w(0xEC48,0x43);ram16w(0xEC4A,0x1C);ram16w(0xC030,0x218);ram16w(0xC034,0xE0);x=100;y=200;before=tick;unsigned updates=map_updates;r36_land(&x,&y);assert(x==100&&y==392);assert(tick-before==1+96+1+257&&map_updates-updates==96+257);assert(ram32r(0xC334)==(0xE0u<<16)+0x1010000u);
 // Cancel restores each source entry coordinates and map/previous semantics.
 for(unsigned sabotage=0;sabotage<2;sabotage++){
  fresh();ram16w(0xEC28,rom16(0x64B02+3*8));ram16w(0xEC2A,0x123);pad_script[0]=0x10;pad_count=1;assert(r36_spaceship_begin(sabotage,&x,&y));assert(field_custom_resume_phase==(sabotage?6:5)&&ram16r(0xEC48)==0x3E&&ram16r(0xEC4A)==0x5A&&ram8r(0xF400)==3);assert(r36_spaceship_resume(&x,&y)&&field_custom_resume_event==255&&ram16r(0xEC2A)==(sabotage?0x123:0xFFFF)&&!flags[0x71]);
 }
 // Confirm queues real takeoff, real landing, and real final entry.
 fresh();ram16w(0xEC28,rom16(0x64B02+3*8));pad_script[0]=0x20;pad_count=1;assert(r36_spaceship_begin(0,&x,&y));assert(field_custom_resume_phase==1&&ram16r(0xEC28)==0x18C&&ram8r(0xF400)==0);pad_count=0;ui_frames=0;ram16w(0xC030,0x218);ram16w(0xC034,0x1E0);x=100;y=200;assert(r36_spaceship_resume(&x,&y)&&field_custom_resume_phase==3&&ram16r(0xEC28)==0&&ram16r(0xEC48)==0x68&&ram16r(0xEC4A)==0x94);
 field_event_reload_pending=0;ram16w(0xC030,0x340);ram16w(0xC034,0x4A0);ram32w(0xEF90,200u<<16);x=100;y=200;assert(r36_spaceship_resume(&x,&y)&&field_custom_resume_phase==4&&ram16r(0xEC28)==0xBF&&ram16r(0xEC48)==0x3C&&ram16r(0xEC4A)==0x28);assert(r36_spaceship_resume(&x,&y)&&field_custom_resume_event==255&&!ram8r(0xECF4));
 // Sabotage: actual Tree35 entry2 F7, 4 panels, genuine battle8, no fake crash.
 for(unsigned defeat=0;defeat<2;defeat++){
  fresh();ram16w(0xEC28,rom16(0x64B02+3*8));pad_script[0]=0x20;pad_count=1;assert(r36_spaceship_begin(1,&x,&y));assert(field_custom_resume_phase==2&&ram16r(0xEC28)==0x18C&&ram16r(0xEC2A)==0xFFFF&&ram8r(0xF400)==4);pad_count=0;battle_defeat=defeat;x=100;y=200;assert(r36_spaceship_resume(&x,&y));assert(battle_calls==1&&battle_id==8&&ps4_return_to_title==defeat&&field_custom_resume_event==255);assert(panel_count==4&&panel_id[0]==0x7A&&panel_id[1]==0x7C&&panel_id[2]==0x7D&&panel_id[3]==0x7E);assert(text_count==2&&texts[0].tree==0x1F81B0&&texts[0].stop==0xF7&&texts[1].offset>texts[0].offset&&texts[1].stop==0xFF);assert(ram16r(0xEC28)==0x18C&&flags[0x71]&&!flags[0x85]&&!flags[0x88]);
 }
 fclose(flight_trace);puts("PASS R36 original destination priority/cancel/confirm, ship counters, all full ascent clocks and source-permitted skips, 12 real-art space passes, landing camera/deceleration, real map continuations, Tree35 seams/panels and battle8 handoff (map load/hardware/battle stubbed)");
}
'''
c=O/'r36_spaceship_probe.c';c.write_text(s)
subprocess.run(['gcc','-std=c11','-O1','-g','-fsanitize=address,undefined','-I'+str(R/'gba'),str(c),'-o',str(O/'r36_spaceship_probe')],check=True)
trace=O/'R36_FLIGHT_TRACE.csv'
subprocess.run([str(O/'r36_spaceship_probe'),str(R/'references/roms/ps4built.bin'),str(R/'gba/r31_field_poses.bin'),str(trace),str(O)],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
# Independent source instruction reference: compare every camera/velocity and
# ship position in the 112/228/419+60-frame sequences for both ship variants.
actual=[list(map(int,row)) for row in csv.reader(trace.open())];expected=[]
for landale in range(2):
 for mode in range(3):
  y=by=200*65536;vy=camera=-65536;ox=(404 if landale else 400)*65536;oy=(528 if landale else 480)*65536
  for frame in range(479 if mode==0 else 228 if mode==1 else 112):
   if frame==419:ox=oy=0
   if frame<419:
    if mode==2:ov=vy;vx=frame;vy-=0x600;camera-=0x800
    elif frame<224:ov=vy;vx=0;vy-=0x800;camera-=0x800
    elif frame<228:ov=vy;vx=0;vy-=0x18000;camera-=0x20000
    elif frame<245:ov=vy;vx=0;vy-=0x18000
    else:ov=camera;vx=0
    ox=(ox+vx)&0xffffffff;oy=(oy+ov)&0xffffffff
   y+=camera;by+=camera;step=bstep=camera
   if mode==0:y%=4096*65536;by%=4096*65536
   else:
    if y<0:y=0;step=0
    if by<0:by=0;bstep=0
   expected.append([mode+landale*3,frame,*[q&0xffffffff for q in [y,by,step,bstep,ox,oy]]])
assert len(actual)==len(expected)==1638
for a,e in zip(actual,expected):assert a[:8]==e,(a,e)
# Generated battle result condition and active takeoff map really own $800F.
h=(R/'gba/generated_simple_events_v0982.h').read_text();assert '{0x29u,0x71u,0x800Fu,1u}' in h
plans=json.loads((R/'generated/psc_runtime_plan_report.json').read_text())['plans'];plan=plans[0x18C];assert 0x29 in plan['events'],plan
report={'ascent_reference_frames':len(actual),'small_ship_ascent':[419,228,112],'landale_ascent':[419,228,112],'white_frames':60,'landing_deceleration_updates':257,'space_pass_object_updates':300,'space_initial_publish':1,'source_caption_palette_index':47,'caption_reveal_after_updates':191,'space_art_cases':12,'takeoff_map_after_battle_run_id':0x29,'crash_event_index':0x800F,'scope':'New scene/actor logic and source ROM data real; actual map loading, screen DMA and battle execution stubbed. No emulator/hardware playthrough.'}
(O/'R36_SPACESHIP_PROBE.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS independent source flight reference:',len(actual),'frames')
from PIL import Image
for p in O.glob('R36_SCENE_*.bin'):
 data=p.read_bytes();palette=[int.from_bytes(data[38400+i*2:38402+i*2],'little') for i in range(256)]
 image=Image.new('RGB',(240,160));image.putdata([tuple((q>>s&31)*255//31 for s in [0,5,10]) for q in [palette[i] for i in data[:38400]]]);image.resize((720,480),Image.Resampling.NEAREST).save(p.with_suffix('.png'));p.unlink()
