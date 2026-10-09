#!/usr/bin/env python3
"""R41 unchanged native Torch events/actors with actual ROM dialogue and clocks.
ASan/UBSan; independent source actor/camera/palette references. Native field
snapshot, general NPCs, panel display, map loading and hardware are fixtures.
Actual native pose uploads execute; pixel fidelity is checked separately.
Requires the R31..R40 probe chain. No normal-save/hardware certification.
"""
from pathlib import Path
import re,subprocess,json,os,csv
from md_compression import nemesis_decompress
R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts';src=(R/'gba/title_runtime_v06.c').read_text();rom=(R/'references/roms/ps4built.bin').read_bytes()
def function(name,text=src):
 m=re.search(r'^static[^\n]*\b'+name+r'\([^;]*?\)\s*\{',text,re.M);assert m,name
 p=m.end();d=1
 while d:d+=(text[p]=='{')-(text[p]=='}');p+=1
 return text[m.start():p]+'\n'
s=(O/'r40_scene_probe.c').read_text().split('int main(')[0]
s=s.replace(function('hw_field_init',s),'static void hw_field_init(unsigned,unsigned);\n')
s+=function('field_palette_restore_step')+function('sprite_frame_offset')+function('char_field_art_address')+function('lead_art_address')
s+='static uint8_t ps4_menu_base_cached;\n'
s+='static unsigned hw_ring_layout_dirty,field_inits,snapshot_count,party_draws,chunk_copies;\n'
s+='static void hw_field_init(unsigned x,unsigned y){(void)x;(void)y;assert(!field_cutscene_panels);field_inits++;}\n'
s+=function('field_manager_copy_ram_words').replace('{','{chunk_copies++;assert(src==0x6100&&dst==0x6960&&count==16);',1)
s+=r'''
static void ps4_ui_clear_field(unsigned x,unsigned y){(void)x;(void)y;assert(!ps4_menu_base_cached);snapshot_count++;}
static void pause_draw_sprite_rom(uint32_t a,unsigned f,int x,int y,unsigned pal,int flip){(void)f;(void)x;(void)y;(void)flip;assert(a>=0x290000&&a<=0x295A00&&pal==2);party_draws++;}
static void panel_capture_base_scaled(void){assert(field_cutscene_panels&&!field_active_panel_count);waitvb();}
'''
# The scene loads original compressed streams; fixture output is source bytes.
for address,tile in [(0x1B1D06,0x36E),(0x1D712C,0x3AF)]:
 data=nemesis_decompress(rom[address:]);name=f'art_{address:x}';pos=s.index('static int load_nem_vram_auto(')
 s=s[:pos]+f'static const uint8_t {name}[]={{'+','.join(map(str,data))+'};\n'+s[pos:]
 pos=s.index('default:assert(0);}',s.index('static int load_nem_vram_auto('));s=s[:pos]+f'case {address}u:assert(tile=={tile});a={name};n=sizeof {name};break;\n'+s[pos:]
s=s.replace(function('capture_tick',s),'static void capture_tick(void){if(capture==3)capture_flash41();}\n')
s='static void capture_flash41(void);\n'+s
s+='#include "r41_torch_events.h"\n'
s+='static FILE* flash41_file;static unsigned flash41_phase;\nstatic void capture_flash41(void){fprintf(flash41_file,"colors,%u,%u",flash41_phase,tick);for(unsigned i=0;i<64;i++)fprintf(flash41_file,",%u",cram[i]);fputc(10,flash41_file); }\n'
s+=r'''
static void fresh41(void){reset40();hw_ring_layout_dirty=field_inits=snapshot_count=party_draws=chunk_copies=0;capture=0;ram16w(0xEC28,0);}
static void actor_trace(FILE*f,unsigned k){unsigned b=0xC300;fprintf(f,"%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",k,ram16r(b),ram32r(b+0x30),ram32r(b+0x34),ram32r(b+0x20),ram16r(b+0x38),ram16r(b+6),ram16r(b+0x1C),ram8r(b+0x10),ram8r(b+0x11),ram8r(b+2)&6,ram8r(b+0x13));}
int main(int argc,char**argv){
 assert(argc==4);FILE*f=fopen(argv[1],"rb");assert(f&&fread(rom,1,sizeof rom,f)==sizeof rom);fclose(f);unsigned x=0x1D8,y=0xB8;
 // Real Tree37 theft entry: all3 F7 offsets interleaved with native actors/cues.
 fresh41();uint8_t ids[]={0,5,3,7,9};for(unsigned i=0;i<5;i++){ram8w(0xF40A+i,ids[i]);r33_party_position(ids[i],0x270,0x160+i*16,0);}followers();
 field_obj_count=7;for(unsigned i=0;i<7;i++){struct FieldObjLite*o=&field_objs[i];o->active=1;o->type=i==0?0x15C:(i==1?0x74:0x3C);o->x=0x270;o->y=0x150;o->base_tile=i==6?0x326:0x296;}
 field_objs[6].type=0x38;field_objs[6].move_kind=1;ram32w(0xC4B0,0x2700000);ram32w(0xC4B4,0x1500000);r33_dialogue_tree(0x1FA480);ram8w(PS4_INV_BASE,0x81);
 assert(r41_event_torch_stolen(&x,&y));assert(flags[0x98]&&!flags[0x9C]&&tree==0x1FA480&&x==0x1D8&&y==0x108);
 assert(field_objs[6].type==0x38&&field_objs[6].move_kind==1&&ram16r(0xC480)==0x8038&&field_objs[6].base_tile==0x326);
 assert(!field_objs[0].active&&!field_objs[1].active&&ram8r(PS4_INV_BASE)==0x81&&!ram8r(PS4_INV_BASE+1));
 assert(text_count==4);for(unsigned i=0;i<4;i++)assert(texts[i].tree==0x1F9580&&texts[i].stop==(i<3?0xF7:255));
 for(unsigned i=8;i<18;i++){if(field_objs[i].active)fprintf(stderr,"residual %u type%X raw%X x%X target%X timer%u f%u d%u\n",i,field_objs[i].type,ram16r(0xC300+i*64),field_objs[i].x,ram16r(0xC338+i*64),ram16r(0xC31C+i*64),ram8r(0xC310+i*64),ram8r(0xC311+i*64));assert(!field_objs[i].active&&!ram16r(0xC300+i*64));}
 unsigned cues[]={0xF3,0xA8,0xDE,0xF3,0xDE,0xF3,0xDE,0xFB,0xA6,0xF3,0xFB,0x93};expect_cues(cues,12);assert(sounds[3].at-sounds[2].at==98&&sounds[5].at-sounds[4].at==106);
 assert(sounds[8].at-sounds[7].at==10&&sounds[9].at-sounds[8].at==1&&sounds[11].at-sounds[10].at==10);
 // Native pose drawing tests simultaneous visible actor carriers/VRAM bounds.
 assert(max_tiles<=DIALOGUE_OBJ_TILE_BASE&&uploads>0);for(unsigned i=0;i<5;i++)assert(ram16r(0xC006+i*64)==4);
 // Every frame of source object families, including type transitions/fractional ghosts.
 f=fopen(argv[2],"w");assert(f);
 unsigned types[]={0x280,0x284,0x288,0x28C,0x290,0x294,0x294,0x294,0x294,0x338,0x338,0x338};
 for(unsigned c=0;c<12;c++){
  fresh41();uint16_t start=c>=7&&c<=8?0x290:0x250;
  r31_actor_spawn(0,types[c],start,0xF0,c>=10?(c==10?4:8):0,types[c]==0x338?0x3AF:0x36E);
  if(c==1){ram32w(0xC320,0x20000);ram16w(0xC338,0x290);}
  if(c>=5&&c<=8){uint32_t v[]={0x18000,0x10000,0xFFFE8000,0xFFFF0000};ram32w(0xC320,v[c-5]);ram16w(0xC338,c==7?0x24F:(c==8?0x250:0x290));}
  if(c>=9)ram8w(0xC311,c==9?7:(c==10?11:15));
  for(unsigned k=0;k<140;k++){r31_actor_update();fprintf(f,"%u,",c);actor_trace(f,k);if(field_objs[0].active){r31_actor_draw(0x1D8,0x98);if(!(ram8r(0xC302)&2))assert(r31_actor_pose(&field_objs[0]));}}
 }fclose(f);
 // Source shortest-wrap X-before-Y camera, including both directions/tie/wrap.
 unsigned cams[][4]={{0,0,0x270,0xF0},{0xFF8,0xFFA,0xA0,0x60},{8,8,0x88,0x48},{0x800,0x800,0x98,0x58},{0xFFE,2,0x9A,0x56},{0x300,0x240,0x270,0xF0}};
 f=fopen(argv[3],"w");assert(f);for(unsigned c=0;c<6;c++){fresh41();x=cams[c][0];y=cams[c][1];r41_camera(cams[c][2],cams[c][3],&x,&y);fprintf(f,"camera,%u,%u,%u,%u\n",c,x,y,tick);assert(map_updates==tick);}fclose(f);
 // Both exact flashes: every possible source Main_Frame_Count phase, incl wrap.
 f=fopen(argv[3],"a");assert(f);
 for(unsigned phase=0;phase<4;phase++){
  fresh41();ram16w(0xEF1C,(uint16_t)(0xFFFC+phase));for(unsigned i=0;i<64;i++)cram[i]=(uint16_t)((i%8)*0x222);
  capture=3;flash41_file=f;flash41_phase=phase;r41_torch_flash();capture=0;for(unsigned i=0;i<64;i++)assert(cram[i]==(i%8)*0x222);assert(tick==42&&map_updates==2);
  fprintf(f,"flash,%u,%u,%u,%u\n",phase,tick,map_updates,ram16r(0xEF1C));
 }fclose(f);
 // Walking and every vehicle kind restore exact stacked source carrier slots.
 for(unsigned vehicle=0;vehicle<=3;vehicle++){
  fresh41();for(unsigned i=0;i<5;i++){ram8w(0xF40A+i,ids[i]);r33_party_position(ids[i],0xBA0,0xE0-i*16,4);}followers();
  ram16w(0xF43C,vehicle);if(vehicle)ram16w(0xC000,vehicle==1?0xB0:(vehicle==2?0xB4:0xB8));
  for(unsigned i=0;i<16;i++){ram16w(0x6100+i*2,(uint16_t)(0x8000+i*17));ram16w(0x6960+i*2,0xFFFF);}ram16w(0x695E,0x1357);ram16w(0x6980,0x2468);
  ram8w(PS4_INV_BASE,0x8E);x=0xB08;y=0x88;r33_dialogue_tree(0x1FA480);unsigned initial_cues=sound_count;
  assert(r41_event_torch_used(&x,&y));assert(flags[0x9C]&&!flags[0x98]&&!ram16r(0xF43C)&&field_inits==1&&snapshot_count==1&&party_draws==5&&chunk_copies==2&&hw_ring_layout_dirty);
  assert(ram8r(PS4_INV_BASE)==0x8E&&!ram8r(PS4_INV_BASE+1)&&text_count==1&&texts[0].tree==0x1F9580&&texts[0].stop==255&&tree==0x1F9580);
  assert(panel_count==1&&panel_id[0]==0x10D&&!field_cutscene_panels&&!field_active_panel_count&&field_follower_count==4);
  assert(map_updates==204&&ram16r(0x695E)==0x1357&&ram16r(0x6980)==0x2468);for(unsigned i=0;i<16;i++)assert(ram16r(0x6960+i*2)==ram16r(0x6100+i*2));
  unsigned expected[]={0xFD,0x99,0xDC,0xDC,0xBC,0xF3};expect_cues(expected+(vehicle?0:2),vehicle?6:4);
  for(unsigned i=0;i<9;i++)assert(!field_objs[i].active&&!ram16r(0xC300+i*64));
  if(vehicle){assert(ram8r(0xECEC)==0x99);for(unsigned i=0;i<5;i++)assert(ram16r(0xC000+i*64)==ids[i]*4+4&&ram16r(0xC030+i*64)==0xBA0&&ram16r(0xC034+i*64)==0xE0&&ram16r(0xC038+i*64)==0xBA0&&ram16r(0xC03A+i*64)==0xE0&&ram16r(0xC006+i*64)==4);}
 }
 puts("PASS R41 actual Torch text/seams/cues/flags, native actors/pose upload bounds, stacked disembark, phase-gated flashes and exact chunk copy; UI/map/hardware fixtures");
}
'''
c=O/'r41_scene_probe.c';c.write_text(s);subprocess.run(['gcc','-std=c11','-O1','-g','-no-pie','-fsanitize=address,undefined','-I'+str(R/'gba'),str(c),'-o',str(O/'r41_scene_probe')],check=True)
trace=O/'R41_ACTOR_TRACE.csv';summary=O/'R41_CLOCK_TRACE.csv'
subprocess.run([str(O/'r41_scene_probe'),str(R/'references/roms/ps4built.bin'),str(trace),str(summary)],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
# Independent source object instruction reference. No native header extraction.
def u32(p):return int.from_bytes(rom[p:p+4],'big')
def animate(state,table):
 p=u32(table+state['face']);n=rom[p]&127;v=bool(rom[p]&128);duration=(state['duration']-1)&255;frame=state['frame']
 if duration>=128:
  frame=(frame+1)&255
  if frame>=n:frame=0;state['flags']|=4
  duration=rom[p+1+(frame if v else 0)]
 state.update(frame=frame,duration=duration)
types=[0x280,0x284,0x288,0x28C,0x290,*([0x294]*4),*([0x338]*3)]
rows=list(csv.reader(trace.open()));assert len(rows)==12*140
for case,kind in enumerate(types):
 vx=[0x18000,0x10000,0xFFFE8000,0xFFFF0000][case-5] if 5<=case<=8 else (0x20000 if case==1 else 0)
 start=0x290 if 7<=case<=8 else 0x250;dest=0x24F if case==7 else (0x250 if case==8 else (0x290 if case==1 or 5<=case<=8 else 0))
 st=dict(word=kind,x=start<<16,y=0xF0<<16,vx=vx,dest=dest,face=4 if case==10 else (8 if case==11 else 0),timer=0,frame=0,duration=7 if case==9 else (11 if case==10 else (15 if case==11 else 0)),flags=0,pal=0)
 for k in range(140):
  word=st['word'];t=word&0x7FFF
  if word:
   if not word&0x8000:
    st['word']=word|0x8000;st['flags']&=~8;st['pal']=0 if t==0x338 else (0x40 if t in (0x28C,0x290) else 0x60)
    if t!=0x338:
     st['face']={0x280:0,0x284:8,0x288:12,0x28C:4,0x290:16,0x294:8}[t];st['frame']=0;st['duration']=4 if t in (0x288,0x28C,0x290) else 6
     if t in (0x280,0x288,0x28C,0x290):st['timer']={0x280:32,0x288:31,0x28C:97,0x290:85}[t]
   run=True;position=True
   if t in (0x280,0x288):
    st['flags']&=~2;st['timer']=(st['timer']-1)&65535;position=False
    if st['timer']:
     if t==0x280:st['flags']|=2
     run=False
    else:
     st['timer']=1
     if st['frame']==6 and st['duration']==1:
      st['word']=0x284 if t==0x280 else 0
      if t==0x288:run=False
   elif t in (0x28C,0x290):
    st['timer']=(st['timer']-1)&65535
    if not st['timer']:st['word']=0;run=False
   elif t==0x294:
    st['flags']^=2
    if st['dest']==st['x']>>16:st['word']=0;run=False
   elif t==0x338 and st['frame']==3 and st['duration']==0:st['word']=0;run=False
   if run:
    if position:st['x']=(st['x']+st['vx'])&0xFFFFFFFF
    animate(st,0x502C2 if t==0x338 else 0x502F6)
    if t==0x284 and st['dest']==st['x']>>16:st['vx']=0
  expected=[case,k,st['word'],st['x'],st['y'],st['vx'],st['dest'],st['face'],st['timer'],st['frame'],st['duration'],st['flags']&6,st['pal']]
  assert list(map(int,rows[case*140+k]))==expected,(case,k,rows[case*140+k],expected)
cams=[(0,0,0x270,0xF0),(0xFF8,0xFFA,0xA0,0x60),(8,8,0x88,0x48),(0x800,0x800,0x98,0x58),(0xFFE,2,0x9A,0x56),(0x300,0x240,0x270,0xF0)]
allclock=list(csv.reader(summary.open()));clock=[r for r in allclock if r[0]!='colors']
colors=[r for r in allclock if r[0]=='colors'];assert len(colors)==168
for phase in range(4):
 live=[0xEEE]*64;target=[(i%8)*0x222 for i in range(64)]
 for k in range(42):
  if k>=2 and ((0xFFFC+phase+k)&3)==0:
   for i,t in enumerate(target):
    for mask,step in [(0xE,2),(0xE0,0x20),(0xE00,0x200)]:
     if live[i]&mask!=t&mask:live[i]-=step
  row=colors[phase*42+k];assert row[:3]==['colors',str(phase),str(k+1)] and list(map(int,row[3:]))==live,(phase,k,row,live)
for case,(x,y,wx,wy) in enumerate(cams):
 tx=(wx-0x98)&4095;ty=(wy-0x58)&4095;cost=sum(min((t-p)&4095,(p-t)&4095) for p,t in [(x,tx),(y,ty)])
 assert clock[case]==['camera',str(case),str(tx),str(ty),str(cost)]
for phase in range(4):assert clock[6+phase]==['flash',str(phase),'42','2',str((0xFFFC+phase+42)&65535)]
(O/'R41_SOURCE_PROBE.json').write_text(json.dumps({'scope':__doc__,'ordinary_events':[0x46,0x47],'theft_f7_seams':3,'original_actor_reference_frames':1680,'shortest_wrap_camera_cases':6,'flash_main_frame_phases':4,'vehicles_tested':[0,1,2,3],'torch_retained':True,'chunk_copy_words':16,'emulator_playthrough':False},indent=2)+'\n')
print('PASS independent 1680 source actor frames,6 camera routes and4 flash clock phases')
