#!/usr/bin/env python3
"""Production discard/take transaction against original loc_35F6 ordering.

ROM prices determine protected items; all 40 slots and adjacent native state
are checked under ASan/UBSan. Interactive confirmation has a separate mGBA test.
"""
from pathlib import Path
import json, os, re, subprocess
R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts'
s=(R/'gba/title_runtime_v06.c').read_text()
def fn(name):
 m=re.search(r'^static[^\n]*\b'+name+r'\([^;\n]*\)\{',s,re.M);assert m,name
 p=m.end();depth=1
 while depth:depth+=(s[p]=='{')-(s[p]=='}');p+=1
 return s[m.start():p]+'\n'
code=r'''
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#define PS4_INV_BASE 0xf410u
#define PS4_ITEM_DATA 0x2a8e28u
static uint8_t ram[65536],rom[0x300000];
static uint8_t ram8r(uint16_t a){return ram[a];}
static void ram8w(uint16_t a,uint8_t v){ram[a]=v;}
static uint16_t rom16(uint32_t a){assert(a+1<sizeof rom);return (rom[a]<<8)|rom[a+1];}
'''+''.join(fn(n) for n in ['ps4_item_ptr','ps4_item_price','ps4_inventory_add','ps4_inventory_remove','battle_take_full_drop'])+r'''
int main(int argc,char**argv){
 assert(argc==2);FILE*f=fopen(argv[1],"rb");assert(f&&fread(rom,1,sizeof rom,f)==sizeof rom);fclose(f);
 unsigned cases=0;uint8_t before[65536],expected[65536];
 const uint8_t drops[]={1,0x4e,0x8c};
 for(unsigned id=1;id<152;id++)for(unsigned slot=0;slot<40;slot++)for(unsigned d=0;d<3;d++){
  memset(ram,0xa5,sizeof ram);for(unsigned i=0;i<40;i++)ram[PS4_INV_BASE+i]=(uint8_t)(1+i%30);
  ram[PS4_INV_BASE+slot]=(uint8_t)id;memcpy(before,ram,sizeof ram);memcpy(expected,ram,sizeof ram);
  // loc_3D84 tests source ItemData+$14; loc_35F6 removes, compacts,
  // then appends the drop at Inventory's final byte. No rewards rerun here.
  unsigned price=(rom[PS4_ITEM_DATA+(id-1)*22+20]<<8)|rom[PS4_ITEM_DATA+(id-1)*22+21];
  if(price){for(unsigned i=slot;i<39;i++)expected[PS4_INV_BASE+i]=before[PS4_INV_BASE+i+1];expected[PS4_INV_BASE+39]=drops[d];}
  assert(battle_take_full_drop(drops[d],slot)==!!price);assert(!memcmp(ram,expected,sizeof ram));cases++;
 }
 const unsigned invalid[]={40,41,255,0xffffffffu};
 for(unsigned k=0;k<4;k++){memset(ram,0xa5,sizeof ram);memcpy(before,ram,sizeof ram);assert(!battle_take_full_drop(1,invalid[k]));assert(!memcmp(ram,before,sizeof ram));cases++;}
 for(unsigned slot=0;slot<40;slot++){
  memset(ram,1,sizeof ram);memcpy(before,ram,sizeof ram);assert(!battle_take_full_drop(0,slot));assert(!memcmp(ram,before,sizeof ram));cases++;
  ram[PS4_INV_BASE+slot]=0;memcpy(before,ram,sizeof ram);assert(!battle_take_full_drop(1,slot));assert(!memcmp(ram,before,sizeof ram));cases++;
 }
 printf("PASS %u source-priced full-drop transactions: every item ID/slot, priceless refusal, compact-then-append, invalid bounds and unchanged surrounding native RAM\n",cases);
}
'''
src=O/'r46_full_drop_probe.c';src.write_text(code);exe=O/'r46_full_drop_probe'
subprocess.run(['cc','-std=c11','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie',str(src),'-o',str(exe)],check=True)
p=subprocess.run([str(exe),str(R/'references/roms/ps4built.bin')],capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
assert p.returncode==0,p.stdout+p.stderr
(O/'R46_CP19_FULL_DROP_TRANSACTIONS.json').write_text(json.dumps({'result':'PASS','cases':18204,'output':p.stdout.strip(),'source':['loc_3D84 price gating','loc_3882 default No','loc_35F6 clear/ReorderInventory/append'],'scope':__doc__,'interactive_UI_in_this_probe':False},indent=2)+'\n')
print(p.stdout,end='')
