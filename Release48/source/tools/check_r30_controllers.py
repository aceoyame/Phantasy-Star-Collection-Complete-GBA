#!/usr/bin/env python3
"""One focused host probe for the new status/tile controllers, then build gates."""
from pathlib import Path
import subprocess,re,json,struct,sys,os
R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts';s=(R/'gba/title_runtime_v06.c').read_text()
pre=r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static uint8_t work_ram[65536],md_vram[65536],rom[0x300000];static uint16_t cram[64];
static uint8_t field_step_completed=1,ps4_return_to_title;static unsigned uploads;
#define PS4_CHAR_BASE 0xF500u
static uint8_t ram8r(uint16_t a){return work_ram[a];}static void ram8w(uint16_t a,uint8_t q){work_ram[a]=q;}
static uint16_t ram16r(uint16_t a){return ((unsigned)work_ram[a]<<8)|work_ram[(uint16_t)(a+1)];}
static void ram16w(uint16_t a,uint16_t q){ram8w(a,q>>8);ram8w(a+1,q);}
static unsigned rom8(unsigned a){assert(a<sizeof rom);return rom[a];}
static unsigned rom16(unsigned a){return rom8(a)*256+rom8(a+1);}
static unsigned rom32(unsigned a){return (rom16(a)<<16)|rom16(a+2);}
static void rom32w(unsigned a,unsigned q){for(unsigned n=0;n<4;n++)rom[a+n]=(q>>(24-8*n))&255;}
static unsigned skip_word_records(unsigned p,unsigned n){while(rom16(p)!=65535)p+=n;return p+2;}
static uint16_t field_gba_ready_tile[2048];
static void hw_publish_canonical_tiles(unsigned t,unsigned n){assert(t+n<=2048);uploads++;}
static void field_palette_live_word(unsigned i,uint16_t v){assert(i<64);cram[i]=v;ram16w(0xFB00+i*2,v);}
static void field_palette_save(unsigned n){for(unsigned i=0;i<n;i++)ram16w(0xFB80+i*2,cram[i]);}
static unsigned field_npc_rng_next(void){return 1;}
static void ps4_field_popup_begin(void){}static void dialogue_obj_window_source_glyph6(unsigned x,unsigned y,unsigned c){(void)x;(void)y;(void)c;}
static void dialogue_obj_window_ascii_text6(unsigned x,unsigned y,const char*s){(void)x;(void)y;(void)s;}
static void ps4_field_popup_wait(void){}static void hw_field_init(unsigned x,unsigned y){(void)x;(void)y;}
static void ps4_field_popup_text(const char*a,const char*b){(void)a;(void)b;}
static void field_palette_fade_out(void){}static void ps4_audio_stop(void){}
'''
body=(R/'gba/r30_tile_animation.h').read_text()+(R/'gba/r30_field_status.h').read_text()
main=r'''
static void step(void){ram8w(0xECE5,ram8r(0xECE5)|2);r30_field_status_update(0,0);}
static void party(unsigned id,unsigned hp,unsigned max,unsigned status){memset(work_ram,0,sizeof work_ram);memset(cram,0,sizeof cram);for(unsigned s=0;s<5;s++)ram8w(0xF40A+s,255);ram8w(0xF40A,id);unsigned a=PS4_CHAR_BASE+id*128;ram16w(a+14,hp);ram16w(a+16,max);ram8w(a+22,status);ram8w(0xECE6,1);ps4_return_to_title=0;field_step_completed=1;}
int main(void){
 party(0,2,20,1);for(unsigned i=0;i<3;i++)step();assert(ram16r(0xF50E)==2);step();assert(ram16r(0xF50E)==1&&cram[0]==14);r30_field_status_restore();assert(!cram[0]);
 for(unsigned i=0;i<4;i++)step();assert(ram16r(0xF50E)==0&&ram8r(0xF516)==4&&ps4_return_to_title);
 party(0,20,20,2);ram8w(0xECE3,24);step();assert(!(ram8r(0xF516)&2));
 party(6,5,10,0);step();assert(ram16r(PS4_CHAR_BASE+6*128+14)==6);ram16w(0xF43C,1);step();assert(ram16r(PS4_CHAR_BASE+6*128+14)==6);ram16w(0xF43C,0);field_step_completed=0;step();assert(ram16r(PS4_CHAR_BASE+6*128+14)==6);
 party(0,5,10,1);ram8w(0xECE6,0);for(unsigned i=0;i<8;i++)step();assert(ram16r(0xF50E)==5);
 for(unsigned kind=0;kind<6;kind++){
  memset(work_ram,0,sizeof work_ram);memset(md_vram,0,sizeof md_vram);for(unsigned i=0;i<8192;i++)work_ram[i]=(uint8_t)(i*7+3);
  unsigned list=kind==4?0:(kind==5?4:kind),seq=0x51772+list*6;rom32w(0x5175Eu +list*4,seq);rom[seq]=4;rom[seq+1]=kind<2?8:6;for(unsigned k=0;k<4;k++)rom[seq+2+k]=k*16;
  ram16w(0xE800,(kind+1)*4);uploads=0;r30_tile_animation_update();assert(uploads==1&&ram8r(0xE80E)==1);
  unsigned before=uploads;for(unsigned i=0;i<rom[seq+1];i++)r30_tile_animation_update();assert(uploads==before);r30_tile_animation_update();assert(uploads==before+1&&ram8r(0xE80E)==2);
 }
 party(0,20,20,0);step();unsigned counter=ram8r(0xECE2);r30_field_status_update(0,0);assert(ram8r(0xECE2)==counter);
 puts("PASS status cadence/death/paralysis/regen/vehicle guards and six tile-animation clocks");
}
'''
c=O/'r30_controllers_probe.c';c.write_text(pre+body+main)
subprocess.run(['gcc','-std=c11','-O1','-g','-fsanitize=address,undefined',str(c),'-o',str(O/'r30_controllers_probe')],check=True)
subprocess.run([str(O/'r30_controllers_probe')],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
# Validate only the new animation parser against every compiled map descriptor.
rom=(R/'references/roms/ps4built.bin').read_bytes();u16=lambda p:int.from_bytes(rom[p:p+2],'big')
records=0;maps=0
for row in re.findall(r'^\{([^}]+)\},', (R/'gba/psc_map_desc.c').read_text(),re.M):
 vals=[int(q.rstrip('u'),0) for q in row.split(',')]
 if not vals[16]:continue
 p=vals[8]
 while u16(p)!=65535:p+=6
 p+=2;n=0
 while u16(p)!=65535:
  assert rom[p] in (0,4,8,12,16,20,24),(rom[p],hex(p));p+=2;n+=1
 assert n<=16,n
 if n:maps+=1
 records+=n
print('PASS authored tile-animation records:',records,'in',maps,'maps; source slot bounds and controller IDs valid')
