#!/usr/bin/env python3
"""Production final scene/source art against independent palette/column clocks
and original ROM pixels. UI, map loading, hardware and combat are fixtures."""
from pathlib import Path
import re,subprocess,os,json
from md_compression import nemesis_decompress,enigma_decompress
R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts';prod=(R/'gba/title_runtime_v06.c').read_text()
def fn(name,text=prod):
 m=re.search(r'^static[^\n]*\b'+name+r'\([^;]*?\)\s*\{',text,re.M);assert m,name
 p=m.end();d=1
 while d:d+=(text[p]=='{')-(text[p]=='}');p+=1
 return text[m.start():p]+'\n'
s=r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define FIELD_DIALOGUE_KEEP_PANELS 1u
static uint8_t rom[0x300000],work_ram[65536],md_vram[65536],field_pause_fb[38400];static uint16_t cram[64],field_palette_saved[64],panel_map_scratch[2048];
static unsigned src_x[240],src_y[160],field_cutscene_panels,tick,frames,rng,flags[256],battle,dialogue,lead_char_id,tree;
static struct{int x,y;unsigned facing,anim;}chaz;static struct{unsigned palette;}field_map;static FILE*trace;static char outdir[512];
static unsigned rom8(unsigned a){assert(a<sizeof rom);return rom[a];}static unsigned rom16(unsigned a){return rom8(a)*256+rom8(a+1);}
static unsigned ram8r(uint16_t a){return work_ram[a];}static unsigned ram16r(uint16_t a){return ram8r(a)*256+ram8r(a+1);}static unsigned ram32r(uint16_t a){return (ram16r(a)<<16)|ram16r(a+2);}
static void ram8w(uint16_t a,uint8_t q){work_ram[a]=q;if(a==0x500A)fprintf(trace,"cue,%u,%u\n",tick,q);}
static void ram16w(uint16_t a,uint16_t q){work_ram[a]=q>>8;work_ram[(uint16_t)(a+1)]=q;}
static void ram32w(uint16_t a,unsigned q){ram16w(a,q>>16);ram16w(a+2,q);}
#define native_main_frame_count ram16r(0xEF1C)
static void waitvb(void){tick++;ram16w(0xEF1C,ram16r(0xEF1C)+1);}
static void field_palette_mirror_live(unsigned n){(void)n;}static void hw_load_palettes(void){}
static void field_palette_save(unsigned n){for(unsigned i=0;i<n;i++)field_palette_saved[i]=cram[i];}
static void field_palette_live_word(unsigned i,unsigned c){assert(i<64);cram[i]=c;}
static unsigned ps4_rng2_next(void){return ++rng*137u;}
static void r36_bitmap_begin(void){}static void presentation_hide_field_oam(void){}
static uint16_t field_entry(unsigned a,unsigned x,unsigned y){(void)a;(void)x;(void)y;return 0;}
static void field_followers_sync(unsigned q){(void)q;}
static void ps4_event_flag_set(unsigned i){flags[i]=1;}
static void field_event_set_mapload_bit(unsigned b){ram8w(0xEC4E,ram8r(0xEC4E)|(1u<<b));}
static int field_event_run_battle_request(unsigned i,unsigned*x,unsigned*y){(void)x;(void)y;assert(flags[0xE8]&&ram8r(0xEC4E)==0x88&&ram8r(0xECEC)==0xFE);battle=i;return 1;}
static int field_dialogue_show2(unsigned i,unsigned x,unsigned y){assert(i==3&&x==0&&y==0&&ram8r(0xECFD)==0);dialogue++;return 1;}
#define PS4_PREDECODE_NEM 0u
static int psc_predecode_copy(unsigned a,unsigned b,void*c,unsigned d,unsigned*e){(void)a;(void)b;(void)c;(void)d;(void)e;return 0;}
'''
s+=fn('nemesis_fast_fallback')+fn('nemesis_fast')+re.search(r'struct BitReader \{[^}]+\};',prod)[0]+'\n'
for n in ['bit_get','bit_read','enigma_inline','enigma_decode','sprite_art_px_rom','char_field_art_address','field_source_sine8','field_source_palette_update30','field_palette_wait','field_palette_all64_to_black','field_palette_first32_from_black_to_current','field_palette_copy_saved_to_live']:s+=fn(n)
s+=r'''
static unsigned tile_px(unsigned e,unsigned x,unsigned y){if(e&0x800)x=7-x;if(e&0x1000)y=7-y;unsigned q=md_vram[(e&2047)*32+y*4+x/2];return (q>>(x&1?0:4))&15;}
static int load_nem_vram_auto(unsigned a,unsigned t){unsigned n=nemesis_fast(a,md_vram+t*32,65536-t*32);assert(n);return 1;}
static void r45_scene_present(void){
 if(frames>0&&frames<=600){fprintf(trace,"frame,%u,%u,%u",frames-1,native_main_frame_count,ram8r(0xECF3));for(unsigned i=0;i<64;i++)fprintf(trace,",%u",cram[i]);for(unsigned i=0;i<20;i++)fprintf(trace,",%u",ram16r(i*4));fputc(10,trace);
 if(frames==1||frames==13||frames==600){char path[768];snprintf(path,sizeof path,"%s/R45_PD_pixels_%u.bin",outdir,frames);FILE*f=fopen(path,"wb");assert(f&&fwrite(field_pause_fb,1,sizeof field_pause_fb,f)==sizeof field_pause_fb);fclose(f);}}
 waitvb();frames++;
}
'''
h=(R/'gba/r45_final_scene.h').read_text();h=h.replace(fn('r45_scene_present',h),'');s+=h
s+=r'''
int main(int argc,char**argv){assert(argc==3);FILE*f=fopen(argv[1],"rb");assert(f&&fread(rom,1,sizeof rom,f)==sizeof rom);fclose(f);snprintf(outdir,sizeof outdir,"%s",argv[2]);char path[768];snprintf(path,sizeof path,"%s/R45_FINAL_SCENE_TRACE.csv",outdir);trace=fopen(path,"w");assert(trace);
 for(unsigned i=0;i<240;i++)src_x[i]=i*320/240;for(unsigned i=0;i<160;i++)src_y[i]=i*224/160;
 field_map.palette=0x1000;for(unsigned i=0;i<100;i++){rom[0x1000+i*2]=(i*73>>8)&14;rom[0x1001+i*2]=(i*73)&0xEE;}
 for(unsigned i=0;i<64;i++)cram[i]=0xEEE;for(unsigned i=0;i<5;i++)ram8w(0xF40A+i,(unsigned[]){0,3,5,7,9}[i]);
 unsigned x=999,y=888;assert(r45_cutscene_pd(&x,&y));assert(battle==0x1A&&dialogue==1&&frames==601);
 for(unsigned i=0;i<5;i++){unsigned ex[5]={160,144,176,128,192},ey[5]={160,176,176,192,192};assert(ram16r(0xC030+i*64)==ex[i]&&ram16r(0xC034+i*64)==ey[i]&&ram16r(0xC038+i*64)==ex[i]&&ram16r(0xC03A+i*64)==ey[i]&&ram16r(0xC006+i*64)==4);}
 fclose(trace);puts("PASS final scene source palette/column motion, arena art, party coordinates and real battle request");}
'''
c=O/'r45_final_scene_probe.c';c.write_text(s);subprocess.run(['gcc','-std=c11','-O1','-g','-no-pie','-fsanitize=address,undefined','-I'+str(R/'gba'),str(c),'-o',str(O/'r45_final_scene_probe')],check=True)
subprocess.run([str(O/'r45_final_scene_probe'),str(R/'references/roms/ps4built.bin'),str(O)],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
rows=[r.split(',') for r in (O/'R45_FINAL_SCENE_TRACE.csv').read_text().splitlines()];frames=[list(map(int,r[1:])) for r in rows if r[0]=='frame'];assert len(frames)==600
rom=(R/'references/roms/ps4built.bin').read_bytes();pal=[0xEEE]*64;q=phase=draw=0;count=16;rng=0;expected=[]
for f,row in enumerate(frames):
 # Palette frame values start after source fade14*2 + publish1 + restore15*8.
 clock=149+f
 assert row[:2]==[f,clock],row[:2]
 if not clock%4:
  colors=[(((q+i+48)*73>>8)&14)*256+(((q+i+48)*73)&0xEE) for i in range(14)]
  pal[0]=colors[0];pal[16:30]=colors;q=(q+1)%14
 phase=(phase+1)%256
 def sine(a):return rom[0x423CC+a] if a<128 else -rom[0x423CC+255-a]
 ys=[(sine((phase+i*44)%256)>>5)&65535 for i in range(20)]
 count-=1
 if count<8:
  pal[0]=(pal[0]+0x222)&65535;pal[16:32]=[(c+0x222)&0xEEE for c in pal[16:32]]
  if count==7:expected.append((clock,0xCE))
  if count==0:rng+=1;count=((rng*137)&127)+9
 assert row[2]==q and row[3:67]==pal and row[67:]==ys,(f,row[:3])
expected=[(0,0xDE),(29,0xDE),(149,0xFE)]+expected+[(749,0xA8),(750,0xF3)]
assert [tuple(map(int,r[1:])) for r in rows if r[0]=='cue']==expected
art=nemesis_decompress(rom[0x1DD566:]);mapping=enigma_decompress(rom[0x1DE03A:],0x620E);assert len(mapping)==256
for fr in [1,13,600]:
 out=bytearray(38400);ys=frames[fr-1][67:];ids=[0,3,5,7,9];xy=[(160,160),(144,176),(176,176),(128,192),(192,192)]
 for y in range(160):
  sy=y*224//160
  for x in range(240):
   sx=x*320//240;ay=(sy+(ys[sx//16] if ys[sx//16]<32768 else ys[sx//16]-65536))&255;col,row=sx//8-12,ay//8-3
   q=0
   if 0<=col<16 and 0<=row<16:
    e=mapping[row*16+col];px=sx%8;py=ay%8
    if e&0x800:px=7-px
    if e&0x1000:py=7-py
    off=((e&2047)-0x20E)*32+py*4+px//2
    if 0<=off<len(art):ci=(art[off]>>(0 if px&1 else 4))&15;q=48+ci if ci else 0
   for i in reversed(range(5)):
    px,py=sx-xy[i][0]+16,sy-xy[i][1]+2
    if 0<=px<16 and 0<=py<32:
     off=0x290000+ids[i]*0x900+(0x30+(px//8)*4+py//8)*32+(py%8)*4+(px%8)//2;ci=(rom[off]>>(0 if px&1 else 4))&15
     if ci:q=32+ci
   out[y*240+x]=q
 assert (O/f'R45_PD_pixels_{fr}.bin').read_bytes()==out,fr
report={'scope':__doc__,'source_reveal_ticks':600,'source_palette_and_twenty_column_ticks_compared':600,'source_arena_pixels_compared':115200,'source_arena_map_dimensions':[16,16],'source_dialogue_entry':3,'source_battle_request':0x1A,'sound_writes':len(expected)}
(O/'R45_FINAL_SCENE_EVIDENCE.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS 600 independent palette/20-column ticks and 115200 original ROM pixels')
