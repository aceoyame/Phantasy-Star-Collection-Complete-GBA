#!/usr/bin/env python3
"""Production money transaction and Daughter palette probe; dialogue/hardware are fixtures."""
from pathlib import Path
import importlib.util, json, os, re, subprocess

R = Path(__file__).resolve().parents[1]
O = R.parent / 'build_artifacts'
prod = (R/'gba/title_runtime_v06.c').read_text()
def fn(name):
    m = re.search(r'^static[^\n]*\b'+name+r'\([^;]*?\)\s*\{', prod, re.M)
    assert m, name
    p=m.end(); depth=1
    while depth:
        depth+=(prod[p]=='{')-(prod[p]=='}');p+=1
    return prod[m.start():p]+'\n'

spec=importlib.util.spec_from_file_location('simple_events', R/'tools/generate_simple_event_descriptors.py')
gen=importlib.util.module_from_spec(spec);spec.loader.exec_module(gen)
rom=(R/'references/roms/ps4built.bin').read_bytes()
for address,size,polarity in [(0x70702,114,0),(0x72C7A,98,1)]:
    row=gen.classify_money_dialogue_transaction(rom[address:address+size])
    assert row and row[6]==polarity,(hex(address),row)

pre=r'''
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "generated_simple_events_v0982.h"
static uint8_t ram[65536],flags[256],field_dialogue[64];
static unsigned field_dialogue_len=64,choice,marker,patches,face_slot,dialogue_calls;
static const uint8_t* field_dialogue_saved_cursor;
static uint16_t cram[64],source_pal[4][2],snapshots[100][64];
static unsigned ticks,loads,selected_fifth,move_count;
static unsigned ram8r(uint16_t a){return ram[a];}
static void ram8w(uint16_t a,uint8_t v){ram[a]=v;}
static uint32_t ram32r(uint16_t a){return ((uint32_t)ram[a]<<24)|((uint32_t)ram[a+1]<<16)|((uint32_t)ram[a+2]<<8)|ram[a+3];}
static void ram32w(uint16_t a,uint32_t v){for(unsigned i=0;i<4;i++)ram[a+i]=(uint8_t)(v>>(24-i*8));}
static void ps4_event_flag_set(unsigned a){flags[a]=1;}
static void ps4_flag_clear_type(unsigned t,unsigned a){assert(!t);flags[a]=0;}
static void field_event_face_source_object(unsigned a,unsigned*x,unsigned*y){(void)x;(void)y;face_slot=a;}
static void field_dialogue_show(unsigned id,unsigned x,unsigned y){(void)x;(void)y;dialogue_calls++;
 if(id==0x12||id==0x51){ram8w(0xED53,choice);field_dialogue[2]=marker?0x3A:0;field_dialogue_saved_cursor=field_dialogue+4;}}
static void field_event_set_source_object_dialogue(unsigned slot,unsigned id){assert(slot==16&&id==0x15);patches++;}
static int field_event_party_slot_for_id(unsigned id){return id==selected_fifth?4:-1;}
static void field_event_move_party_targets(const uint8_t*ids,const uint16_t*x,const uint16_t*y,unsigned n,unsigned hold,unsigned*cx,unsigned*cy){(void)x;(void)y;(void)cx;(void)cy;assert(hold==120);move_count=n;assert(n==4||(n==5&&ids[4]==selected_fifth));}
static void field_event_wait_updates(unsigned n,unsigned*x,unsigned*y){(void)n;(void)x;(void)y;}
static void field_palette_mirror_live(unsigned n){assert(n==64);}
static void hw_load_palettes(void){loads++;}
static void waitvb(void){assert(ticks<100);memcpy(snapshots[ticks++],cram,sizeof cram);}
static void field_event_set_party_facing(unsigned id,unsigned face){assert(id==7&&!face);}
static void field_dialogue_resume_saved(unsigned x,unsigned y){(void)x;(void)y;}
static void field_event_move_party_id(unsigned id,unsigned x,unsigned y,unsigned*cx,unsigned*cy){(void)cx;(void)cy;assert(id==0&&x==0x2E0&&y==0x2F0);}
static void field_event_move_camera_to(unsigned x,unsigned y,unsigned speed,unsigned*cx,unsigned*cy){(void)x;(void)y;(void)cx;(void)cy;assert(speed==2);}
static struct {int x,y;}chaz;
'''
body=fn('field_event_money_dialogue_transaction')+fn('field_event_dominators_defeated')
main=r'''
int main(int argc,char**argv){
 assert(argc==2);FILE*f=fopen(argv[1],"rb");assert(f);fseek(f,0x7339A,SEEK_SET);uint8_t bytes[18];assert(fread(bytes,1,18,f)==18);fclose(f);
 for(unsigned k=0;k<4;k++){source_pal[k][0]=bytes[k*2]*256+bytes[k*2+1];source_pal[k][1]=bytes[10+k*2]*256+bytes[11+k*2];}
 unsigned cases=0,x=0,y=0;
 for(unsigned j=0;j<2;j++){const struct Ps4MoneyDialogueTransactionDescriptor*d=&ps4_money_dialogue_transaction_descriptors[j];
  for(unsigned delta=0;delta<3;delta++)for(choice=0;choice<2;choice++)for(marker=0;marker<2;marker++){
   memset(ram,0,sizeof ram);memset(flags,0,sizeof flags);patches=0;dialogue_calls=0;field_dialogue_saved_cursor=0;
   uint32_t money=d->cost+delta-1;ram32w(0xF438,money);flags[d->money_flag]=1;
   assert(field_event_money_dialogue_transaction(d->event_index,&x,&y));
   assert(flags[d->money_flag]==(j?money>=d->cost:money<d->cost));assert(face_slot==d->object_slot&&dialogue_calls==1);
   unsigned paid=money>=d->cost&&!choice&&marker;
   assert(ram32r(0xF438)==money-(paid?d->cost:0));assert(flags[d->success_flag]==paid);assert(patches==(paid&&j==0));cases++;
  }
 }
 static const unsigned fifth[7]={255,2,4,6,8,9,10};
 for(unsigned p=0;p<7;p++){
  selected_fifth=fifth[p];ticks=loads=0;field_dialogue_saved_cursor=0;memset(flags,0,sizeof flags);
  for(unsigned i=0;i<64;i++)cram[i]=(uint16_t)(0x1000+i);
  assert(field_event_dominators_defeated(&x,&y));assert(ticks==96&&loads==5&&move_count==(p?5:4));
  for(unsigned t=0;t<96;t++)for(unsigned i=0;i<64;i++){
   unsigned phase=t==0?0:(t-1)/16;if(phase>3)phase=3;
   uint16_t want=i==12?source_pal[phase][0]:i==13?source_pal[phase][1]:(uint16_t)(0x1000+i);
   assert(snapshots[t][i]==want);
  }
  assert(flags[0xBB]&&!flags[0xB6]);
 }
 printf("PASS production quest probe: %u transactions, 7 party branches, 43008 palette words\n",cases);
}
'''
O.mkdir(exist_ok=True)
c=O/'r46_quest_probe.c';c.write_text(pre+body+main)
exe=O/'r46_quest_probe'
subprocess.run(['gcc','-std=c99','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie','-I'+str(R/'gba'),str(c),'-o',str(exe)],check=True)
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0')
result=subprocess.run([str(exe),str(R/'references/roms/ps4built.bin')],check=True,env=env,capture_output=True,text=True)
print(result.stdout,end='')
(O/'R46_QUEST_CHECK.log').write_text(result.stdout)
(O/'R46_QUEST_EVIDENCE.json').write_text(json.dumps({'result':'PASS','production_transactions':24,'optional_party_branches':7,'palette_words_compared':43008,'source_classifier_polarities':[0,1],'scope':'Production executor and original ROM palette/classifier comparison; dialogue choice/input and hardware/map updates are fixtures.'},indent=2)+'\n')
