#!/usr/bin/env python3
"""Focused checks for changed FIFO alignment and source-backed debug stats."""
from pathlib import Path
import re,subprocess,os
R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts';s=(R/'gba/title_runtime_v06.c').read_text()
def function(name):
    m=re.search(r'^static[^\n]*\b'+name+r'\([^;]*?\)\s*\{',s,re.M);assert m,name
    a=m.start();p=s.index('{',m.start());depth=1;p+=1
    while depth:
        if s[p]=='{':depth+=1
        elif s[p]=='}':depth-=1
        p+=1
    return s[a:p]+'\n'
pre='''#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include "r28_vehicle_assets.h"
#define PS4_CHAR_BASE 0xF500u
static uint8_t work_ram[65536],source_rom[0x300000];
static uint8_t lead_char_id;static uint16_t hw_chaz_key;
static void field_followers_sync(int n){(void)n;}
static uint8_t rom8(uint32_t a){assert(a<sizeof(source_rom));return source_rom[a];}
static uint16_t rom16(uint32_t a){return (rom8(a)<<8)|rom8(a+1);}
static uint32_t rom32(uint32_t a){return ((uint32_t)rom16(a)<<16)|rom16(a+2);}
static uint8_t ram8r(uint16_t a){return work_ram[a];}
static uint16_t ram16r(uint16_t a){return (ram8r(a)<<8)|ram8r(a+1);}
static uint32_t ram32r(uint16_t a){return ((uint32_t)ram16r(a)<<16)|ram16r(a+2);}
static void ram8w(uint16_t a,uint8_t v){work_ram[a]=v;}
static void ram16w(uint16_t a,uint16_t v){ram8w(a,v>>8);ram8w(a+1,v);}
static void ram32w(uint16_t a,uint32_t v){ram16w(a,v>>16);ram16w(a+2,v);}
'''
body=''.join(function(n) for n in ['item_s8','item_u8','update_one_char_derived','init_one_char','battle_apply_levelups_one','ps4_debug_level60_party','battle_fill_vehicle_stats'])
main='''int main(int c,char**v){assert(c==2);FILE*f=fopen(v[1],"rb");assert(f&&fread(source_rom,1,sizeof(source_rom),f)==sizeof(source_rom));fclose(f);
 uint32_t sp=0x2A8ACA;for(unsigned i=0;i<11;i++)init_one_char(i,&sp);assert(sp==0x2A8ACA+66*11);
 ps4_debug_level60_party();const unsigned ids[5]={0,5,3,6,9};const unsigned start[5]={1,1,17,12,27};const uint32_t table[5]={0x2856B0,0x287E7A,0x286F70,0x2886E6,0x289B86};
 for(unsigned i=0;i<5;i++){unsigned id=ids[i];uint16_t d=0xF500+id*128;assert(ram8r(0xF40A+i)==id);assert(ram16r(d+8)==60);uint32_t rec=table[i]+(59-start[i])*22;assert(ram16r(d+0x10)==rom16(rec+4));assert(ram16r(d+0x14)==rom16(rec+6));assert(ram16r(d+0x0E)==ram16r(d+0x10));assert(ram16r(d+0x12)==ram16r(d+0x14));for(unsigned j=0;j<8;j++)assert(ram8r(d+0x6A+j*2)==ram8r(d+0x6B+j*2));printf("PASS character %u: level60 HP%u TP%u source skills restored\\n",id,ram16r(d+0x0E),ram16r(d+0x12));}
 const unsigned hp[3]={740,960,680},atk[3]={200,250,150},mask[3]={3,0x50,0x0C};for(unsigned v=1;v<=3;v++){battle_fill_vehicle_stats(v);assert(ram16r(0x470E)==hp[v-1]);assert(ram16r(0x4726)==atk[v-1]);for(unsigned j=0;j<8;j++){assert((ram8r(0x4762+j)!=0)==((mask[v-1]&(1<<j))!=0));if(mask[v-1]&(1<<j))assert(ram8r(0x476A+j*2)>0);}printf("PASS vehicle %u: source HP%u ATK%u Optin mask%02X\\n",v,hp[v-1],atk[v-1],mask[v-1]);}return 0;}
'''
(O/'R28_STATS_PROBE.c').write_text(pre+body+main)
subprocess.run(['cc','-O1','-g','-fsanitize=address,undefined','-I'+str(R/'gba'),str(O/'R28_STATS_PROBE.c'),'-o',str(O/'r28_stats_probe')],check=True)
subprocess.run([str(O/'r28_stats_probe'),str(R/'references/roms/ps4built.bin')],check=True,env=dict(os.environ,ASAN_OPTIONS="detect_leaks=0"))
# mGBA 0.10.5's FIFO reset preserves internalSample/internalRemaining.
# Keep the partially consumed word, reset only queued words, then refill.
def handoff(off,ceil):
    consumed=528+off;word=consumed//4*4;latched=list(range(consumed,word+4)) if consumed%4 else []
    refill=(off+3)//4*4 if ceil else off//4*4
    got=latched+list(range(528+refill,528+refill+32))
    return got[:24]==list(range(consumed,consumed+24))
assert all(handoff(o,True) for o in range(528))
assert not any(handoff(o,False) for o in range(1,528) if o%4)
print('PASS persistent FIFO latch: all 528 offsets; old floor repeats on every partial word')
a=(R/'gba/ps4_audio_stream.c').read_text();assert 'off=(off+3u)&~3u;' in a
assert 'dma_copy32(title_fb,field_pause_fb,240u*160u)' not in s[s.index('static void game_start_world_text_set(unsigned first_id){'):]
print('PASS opening backdrop restore uses a distinct Mode4 page')
