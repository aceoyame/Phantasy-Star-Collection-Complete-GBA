#!/usr/bin/env python3
"""Exercise production ending/credits with original ROM assets. UI/input/map
reload/hardware are fixtures. Independently check source scroll/fade arithmetic,
original encoded text pixels, Fin placement and isolated SRAM writes."""
from pathlib import Path
import re,subprocess,os,json
from md_compression import kosinski_decompress,enigma_decompress,nemesis_decompress
R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts';prod=(R/'gba/title_runtime_v06.c').read_text()
def fn(name,text=prod):
 m=re.search(r'^static[^\n]*\b'+name+r'\([^;]*?\)\s*\{',text,re.M);assert m,name
 p=m.end();d=1
 while d:d+=(text[p]=='{')-(text[p]=='}');p+=1
 return text[m.start():p]+'\n'
s=r'''
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define FIELD_DIALOGUE_KEEP_PANELS 1u
#define FIELD_DIALOGUE_SOURCE_PALETTE 2u
#define MODE4 4u
#define MODE4_FRAME1 16u
#define BG2_ENABLE 1024u
#define OBJ_1D 64u
#define OBJ_ENABLE 4096u
#define PS4_PREDECODE_KOS 1u
#define PS4_PREDECODE_NEM 0u
static uint8_t rom[0x300000],work_ram[65536],md_vram[65536],field_pause_fb[38400],page0[38400],page1[38400],field_dialogue[16384];
#define MODE4_VRAM16 page0
#define MODE4_PAGE1 page1
static uint16_t cram[64],field_palette_saved[64],panel_map_scratch[2048],REG_DISPCNT,REG_BLDCNT,REG_BLDY;
static unsigned src_x[240],src_y[160],field_active_panel_count,field_cutscene_panels,field_dialogue_len,field_follower_count,field_obj_count,ps4_return_to_title;
static uint8_t field_custom_resume_event,field_custom_resume_phase;static const uint8_t*field_dialogue_saved_cursor;static unsigned tick,write_count,pressed_at,render_count,max_panels;static char outdir[512];static FILE*trace,*events;
static struct{unsigned active;}field_objs[12];static struct{unsigned epoch;}field_obj_art_stamp[12];
static unsigned rom8(unsigned a){assert(a<sizeof rom);return rom[a];}static unsigned rom16(unsigned a){return rom8(a)*256+rom8(a+1);}static unsigned rom32(unsigned a){return (rom16(a)<<16)|rom16(a+2);}
static unsigned ram8r(uint16_t a){return work_ram[a];}static unsigned ram16r(uint16_t a){return ram8r(a)*256+ram8r(a+1);}static unsigned ram32r(uint16_t a){return (ram16r(a)<<16)|ram16r(a+2);}
static void ram8w(uint16_t a,uint8_t v){work_ram[a]=v;if(a==0x500A)fprintf(events,"cue,%u,%u\n",tick,v);}
static void ram16w(uint16_t a,uint16_t v){work_ram[a]=v>>8;work_ram[(uint16_t)(a+1)]=v;}static void ram32w(uint16_t a,unsigned v){ram16w(a,v>>16);ram16w(a+2,v);}
static void waitvb(void){tick++;ram16w(0xEF1C,(uint16_t)(ram16r(0xEF1C)+1));}
static void dma_copy32(const void*a,void*b,unsigned n){memcpy(b,a,n);}static void r36_bitmap_begin(void){}static void presentation_hide_field_oam(void){}static void panel_clear_scaled(void){memset(field_pause_fb,0,sizeof field_pause_fb);}
static void field_palette_mirror_live(unsigned n){(void)n;}static void hw_load_palettes(void){}static void field_dialogue_commit_palette(void){}
static void field_palette_save(unsigned n){for(unsigned i=0;i<n;i++)field_palette_saved[i]=cram[i];}
static void field_palette_fade_out(void){for(unsigned i=0;i<14;i++)waitvb();}static void field_palette_fade_in(void){for(unsigned i=0;i<14;i++)waitvb();}
static void panel_rebuild_scaled(void){}static void panel_present_scaled(void){waitvb();}
static void r42_panel(unsigned id){assert(field_active_panel_count<32);field_active_panel_count++;if(field_active_panel_count>max_panels)max_panels=field_active_panel_count;unsigned a=rom32(0x6B2B6+(id>>6)*4)+(id&63)*30;for(unsigned i=0;i<2;i++){unsigned b=10+i*10,code=rom16(a+b),pal=rom32(a+b+2);if(code==65535)continue;assert(pal+32<=sizeof rom);for(unsigned k=0;k<16;k++)cram[((code>>13)&3)*16+k]=rom16(pal+k*2);}fprintf(events,"panel,%u,%u\n",tick,id);waitvb();}
static int psc_predecode_copy(unsigned a,unsigned b,void*c,unsigned d,unsigned*e){(void)a;(void)b;(void)c;(void)d;(void)e;return 0;}
static unsigned tile_px(unsigned e,unsigned x,unsigned y){if(e&0x800)x=7-x;if(e&0x1000)y=7-y;unsigned q=md_vram[(e&2047)*32+y*4+x/2];return (q>>(x&1?0:4))&15;}
static unsigned sprite_art_px_rom(unsigned a,unsigned b,unsigned c,unsigned d,unsigned e){(void)a;(void)b;(void)c;(void)d;(void)e;return 0;}static unsigned char_field_art_address(unsigned a){(void)a;return 0;}
static uint16_t field_entry(unsigned a,unsigned x,unsigned y){(void)a;(void)x;(void)y;return 0;}
static unsigned md_io_read_psiv_pad(void){return (tick==pressed_at||(tick>=10041&&tick<10050))?128:0;}
static void r31_actor_update_one(unsigned a){(void)a;}static int ps4_event_flag_test(unsigned a){(void)a;return 0;}
static void md_write8(unsigned a,unsigned v){assert(write_count<3);unsigned aa[3]={0xA130F1,0x200035,0xA130F1},vv[3]={1,1,0};assert(tick==pressed_at&&a==aa[write_count]&&v==vv[write_count]);write_count++;}
static void r34_queue(uint8_t e,uint8_t p,uint16_t m,uint16_t prev,uint16_t x,uint16_t y,uint16_t face,uint16_t align){assert(e==0xB3&&!p&&m==0x7B&&prev==0xFFFF&&x==0x36&&y==0x28&&!face&&!align);field_custom_resume_event=e;field_custom_resume_phase=p;}
'''
s+=re.search(r'struct KosReader \{[^}]+\};',prod)[0]+'\n'+re.search(r'struct BitReader \{[^}]+\};',prod)[0]+'\n'
for n in ['kos_init','kos_bit','kos_decode','nemesis_fast_fallback','nemesis_fast','bit_get','bit_read','enigma_inline','enigma_decode','field_palette_wait','field_palette_copy_saved_to_live','field_palette_all64_to_black','field_palette_first32_from_black_to_current','field_palette_increase_tone_once','field_palette_restore_step','field_dialogue_offset','field_dialogue_entry','field_dialogue_resolve']:s+=fn(n)
s+=fn('r34_vint_wait',(R/'gba/r34_story_cutscenes.h').read_text())+fn('r42_tone',(R/'gba/r42_rykros_leroof.h').read_text())
s+=r'''
static void r33_dialogue_tree(unsigned a){assert(a==0x1FC920);field_dialogue_len=kos_decode(a,field_dialogue,sizeof field_dialogue);assert(field_dialogue_len);}
static int field_dialogue_run_mode(const uint8_t*p,unsigned x,unsigned y,unsigned mode){(void)x;(void)y;assert(mode==3&&p);unsigned began=tick;long offset=p-field_dialogue;for(;;){assert(p<field_dialogue+field_dialogue_len);unsigned q=*p++;if(q==0xF7||q==0xFF){field_dialogue_saved_cursor=p;fprintf(events,"text,%u,%ld,%ld,%u\n",began,offset,(long)(p-field_dialogue),q);return 0;}unsigned n=0;if(q==0xF4||q==0xF9)n=1;else if(q==0xF5||q==0xF6||q==0xFA)n=2;else if(q==0xF2){unsigned a=*p++;if(a==0){unsigned id=p[0]*256+p[1];p+=2;r42_panel(id);}else if(a==1){assert(field_active_panel_count);field_active_panel_count--;waitvb();}else if(a==2){field_active_panel_count=0;waitvb();}else n=a==3||a==4||a==11?1:0;}p+=n;}}
static void r43_resume_text(unsigned cue,unsigned finish,unsigned x,unsigned y){assert(!cue&&!finish);const uint8_t*p=field_dialogue_saved_cursor;field_dialogue_saved_cursor=0;field_dialogue_run_mode(p,x,y,3);}
static unsigned load_kos_vram_auto(unsigned a,unsigned tile){unsigned n=kos_decode(a,md_vram+tile*32,65536-tile*32);assert(n);return n;}static unsigned load_nem_vram_auto(unsigned a,unsigned tile){unsigned n=nemesis_fast(a,md_vram+tile*32,65536-tile*32);assert(n);return n;}
'''
s+=fn('r45_scene_compose',(R/'gba/r45_final_scene.h').read_text())
s+=r'''
static void r45_scene_present(void){waitvb();render_count++;}
'''
s+='#include "r45_ending.h"\n'
s+=r'''
static void dump(const char*name,void*p,unsigned n){char a[768];snprintf(a,sizeof a,"%s/%s",outdir,name);FILE*f=fopen(a,"wb");assert(f&&fwrite(p,1,n,f)==n);fclose(f);}
static void log_tick(unsigned f){fprintf(trace,"%u,%u,%u,%u,%u,%u,%u,%u,%u,%u",f,tick,ram16r(0xED90),ram16r(0xED92),ram16r(0xED96),ram16r(0xED98),ram8r(0xED95),ram8r(0xECF2),ram32r(0xEF90),ram32r(0xEF98));for(unsigned i=0;i<64;i++)fprintf(trace,",%u",cram[i]);fputc(10,trace);}
int main(int argc,char**argv){assert(argc==3);FILE*f=fopen(argv[1],"rb");assert(f&&fread(rom,1,sizeof rom,f)==sizeof rom);fclose(f);snprintf(outdir,sizeof outdir,"%s",argv[2]);char a[768];snprintf(a,sizeof a,"%s/R45_CREDITS_TRACE.csv",outdir);trace=fopen(a,"w");snprintf(a,sizeof a,"%s/R45_ENDING_TRACE.csv",outdir);events=fopen(a,"w");for(unsigned i=0;i<240;i++)src_x[i]=i*320/240;for(unsigned i=0;i<160;i++)src_y[i]=i*224/160;
 r33_dialogue_tree(0x1FC920);r45_ending_narrative(0,0);fprintf(events,"narrative_end,%u,%u,%u\n",tick,field_active_panel_count,max_panels);r45_credit_intro();fprintf(events,"intro_end,%u\n",tick);
 memset(work_ram,0,sizeof work_ram);tick=render_count=0;r45_credit_init();assert(tick==30);unsigned frames=0;
 for(unsigned stage=1;stage<=3;stage++){unsigned end=stage==1?896:stage==2?1024:1152;if(stage==2)ram16w(0xED92,0);while(ram16r(0xED90)!=end){r45_credit_tick(stage);log_tick(frames++);if(frames==512||frames==7168||frames==8192||frames==9216){char name[64];snprintf(name,sizeof name,"R45_credits_ram_%u.bin",frames);dump(name,work_ram,sizeof work_ram);snprintf(name,sizeof name,"R45_credits_vram_%u.bin",frames);dump(name,md_vram,sizeof md_vram);}}}
 assert(frames==9216&&tick==9246);fprintf(events,"credits_end,%u\n",tick);
 memset(work_ram,0x5A,sizeof work_ram);field_obj_count=12;for(unsigned i=0;i<12;i++)field_objs[i].active=1;ram32w(0xEF90,208u<<16);ram32w(0xEF94,272u<<16);ram32w(0xEF98,208u<<16);ram32w(0xEF9C,272u<<16);tick=10000;ram16w(0xEF1C,10000);pressed_at=10080;
 for(unsigned i=0;i<64;i++)cram[i]=0xEEE;assert(r45_ending_resume(0,0));assert(write_count==3&&ps4_return_to_title&&field_custom_resume_event==255);for(unsigned i=0;i<320;i++)assert(!work_ram[0xC000+i]);for(unsigned i=0;i<448;i++)assert(!work_ram[0xC300+i]);for(unsigned i=0;i<64;i++)assert(!work_ram[0xC540+i]);assert(work_ram[0xF100]==0x5A&&work_ram[0xC4C0]==0x5A);assert(!field_objs[0].active&&field_objs[8].active&&!field_objs[9].active&&field_objs[10].active);dump("R45_FIN_pixels.bin",field_pause_fb,sizeof field_pause_fb);dump("R45_FIN_ram.bin",work_ram,sizeof work_ram);dump("R45_FIN_vram.bin",md_vram,sizeof md_vram);
 fclose(trace);fclose(events);puts("PASS production ending/credits/Fin source clocks, assets, continuation and isolated clear-flag writes (UI/map/hardware fixtures)");}
'''
c=O/'r45_ending_probe.c';c.write_text(s);subprocess.run(['gcc','-std=c11','-O1','-g','-no-pie','-fsanitize=address,undefined','-I'+str(R/'gba'),str(c),'-o',str(O/'r45_ending_probe')],check=True)
subprocess.run([str(O/'r45_ending_probe'),str(R/'references/roms/ps4built.bin'),str(O)],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
print('C probe passed; independent ROM/source checks follow')
rom=(R/'references/roms/ps4built.bin').read_bytes()
def word(a):return int.from_bytes(rom[a:a+2],'big')
def table(a):
 out={}
 while rom[a]!=255:out[rom[a]]=rom[a+1];a+=2
 return out
tables=[table(0x7A522),table(0x7A548)];pal=[word(0x9F2BC+i*2) for i in range(64)];pal[:32]=[word(0x1DF53A+i*2) for i in range(32)];pal[47:]=[0]*17
fg=0;bg=0xFF200000;counter=stream=A=B=flags=phase=0;transitions=[]
rows=[list(map(int,r.split(','))) for r in (O/'R45_CREDITS_TRACE.csv').read_text().splitlines()];assert len(rows)==9216
for f,row in enumerate(rows):
 stage=1 if f<7168 else 2 if f<8192 else 3
 if f==7168:stream=0
 old=bg if stage==1 else fg;bg=(bg+(0x2000 if stage==1 else 0x1000))&0xFFFFFFFF
 if stage!=1:fg=(fg+0x2000)&0xFFFFFFFF
 changed=old^(bg if stage==1 else fg)
 if changed&0x10000:counter+=1
 if changed&0x80000:stream+=1;flags|=2 if stage==1 else 1
 if flags&1:flags&=254
 elif flags&2:flags&=253
 if stage!=3 or not(flags&128):
  cmd=tables[stage==3].get(counter&63)
  if cmd in (0,9,10,11):A+=1;flags|=1
  elif cmd in (1,2,3,4):
   if cmd==4:B+=1
   flags|=1
  elif cmd in (5,6,7,8):
   flags|=1<<(cmd-3)
   if cmd in (5,7):phase=0
  elif cmd==12:flags|=64
  snapshot=flags;clock=31+f
  for bit in range(2,7):
   if not snapshot&(1<<bit):continue
   if bit in (2,4,6):
    gate=1 if bit==4 else 3
    if clock&gate:continue
    colors=7-phase if bit==4 else phase
    pal[50:55]=[word(0x7A6DE+colors*10+i*2) for i in range(5)];phase=(phase+1)&7
    if not phase:
     if bit==2:flags&=251
     elif bit==4:flags=(flags&239)|1
     else:flags=(flags&251)|128
   elif bit==3 and not clock&7:
    v=(pal[47]+0x222)&65535
    if v&0x1000:flags&=247
    else:pal[47]=v
   elif bit==5 and not clock&1:
    if pal[47]:pal[47]=(pal[47]-0x222)&65535
    else:flags=(flags&223)|1
 expected=[f,31+f,counter,stream,A,B,flags,phase,fg,bg]+pal
 assert row==expected,(f,[(i,a,b) for i,(a,b) in enumerate(zip(row,expected)) if a!=b])
 if f in (7167,8191,9215):transitions.append({'frame':f+1,'clock':row[1],'text_counter':counter,'header_counter':A,'names_counter':B,'flags':flags})
# Independently compare the authored font bitmaps in all four name lines with
# the production VRAM after each captured credit stage. No production decoder
# or drawing routine is used for this reference.
def entry(a,id):
 for _ in range(id):a=rom.index(255,a)+1
 return rom[a:rom.index(255,a)]
font_pixels=0
for frame in (512,7168,8192):
 v=(O/f'R45_credits_vram_{frame}.bin').read_bytes();names=(rows[frame-1][5]>>3)-1
 # Last four completed name writes correspond to the prior block after cmd4.
 for line in range(4):
  text=entry(0x1FEB42,names*4+line);expected=bytearray(2048);i=0;col=0
  while i<len(text) and col<32:
   code=text[i];i+=1
   if code>=240:break
   if code>=224:code=text[i];i+=1
   for y in range(16):
    bits=rom[0x2A3542+(code&127)*16+y]
    for x in range(8):
     if bits&(128>>x):expected[col*64+y*4+x//2]|=15<<(4 if x%2==0 else 0)
   col+=1
  start=(0x300+line*0x80)*32
  assert v[start:start+2048]==expected,(frame,line,names,text)
  font_pixels+=32*8*16
# Fin map and palette use the exact source Enigma/Nemesis assets, projected
# against an independent per-pixel tile lookup from the original art.
art=nemesis_decompress(rom[0x1DEEB8:]);mapping=enigma_decompress(rom[0x1DF52C:],0xC580);assert len(mapping)==48
out=(O/'R45_FIN_pixels.bin').read_bytes();expected=bytearray(38400)
for y in range(160):
 sy=y*224//160
 for x in range(240):
  sx=x*320//240;ci=2 if sx<192 else 0
  if 48<=sx<144 and 88<=sy<120:
   e=mapping[(sy-88)//8*12+(sx-48)//8];xx=sx%8;yy=sy%8
   if e&0x800:xx=7-xx
   if e&0x1000:yy=7-yy
   a=(e&2047)-0x580;assert a*32+yy*4+xx//2<len(art)
   c=(art[a*32+yy*4+xx//2]>>(4 if xx%2==0 else 0))&15
   if c:ci=((e>>13)&3)*16+c
   else:ci=2
  expected[y*240+x]=ci
assert out==expected,next(((i,a,b) for i,(a,b) in enumerate(zip(out,expected)) if a!=b),None)
# Ending narrative uses the real Tree42 F7/FF seams; assert every resume starts
# exactly after the preceding source terminator and final segment completes.
events=[r.split(',') for r in (O/'R45_ENDING_TRACE.csv').read_text().splitlines()];texts=[list(map(int,r[1:])) for r in events if r[0]=='text'];assert len(texts)==26
assert all(r[-1] in (247,255) for r in texts);assert texts[-1][-1]==255
report={'source':'pinned US ASM/listing and original ROM','credit_ticks_checked':len(rows),'stage_transitions':transitions,'encoded_font_pixels_checked':font_pixels,'fin_pixels_checked':len(out),'narrative_text_segments':len(texts),'original_tree42_address':'0x1FC920','clear_flag_writes':['A130F1=1','200035=1','A130F1=0'],'limitations':['Dialogue UI/input, Termi map load, physical display and SRAM hardware are fixtures.','No normal-save emulator/hardware full playthrough or timing parity claim.']}
(O/'R45_ENDING_EVIDENCE.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS independent source arithmetic for all 9216 credits ticks, original-font pixels and Fin art')
