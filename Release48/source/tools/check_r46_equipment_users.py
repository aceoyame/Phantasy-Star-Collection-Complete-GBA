#!/usr/bin/env python3
"""Production eligibility against explicit original-ASM equipment owners."""
from pathlib import Path
import json,os,re,subprocess
R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts';s=(R/'gba/title_runtime_v06.c').read_text()
def fn(name):
 m=re.search(r'^static[^\n]*\b'+name+r'\([^;\n]*\)\{',s,re.M);assert m
 p=m.end();depth=1
 while depth:depth+=(s[p]=='{')-(s[p]=='}');p+=1
 return s[m.start():p]+'\n'
code=r'''
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#define PS4_ITEM_DATA 0x2a8e28u
static uint8_t rom[0x300000];
static uint16_t rom16(uint32_t p){assert(p+1<sizeof rom);return (uint16_t)((rom[p]<<8)|rom[p+1]);}
'''+fn('ps4_item_ptr')+fn('ps4_item_char_allowed')+r'''
int main(int argc,char**argv){
 assert(argc==2);FILE*f=fopen(argv[1],"rb");assert(f&&fread(rom,1,sizeof rom,f)==sizeof rom);fclose(f);
 /* Source InventoryData declarations: Dagger/HuntKnife = Chaz,Hahn,Seth;
  * Boomerang = Alys,Kyra; WoodCane = Rune,Raja; FlameSword = Chaz;
  * SilvrTusk = Rika. Expected owners are semantic IDs, not native mask math. */
 static const struct {uint8_t item,owners[4];} cases[]={
 {1,{0,2,10,255}},{2,{0,2,10,255}},{3,{1,9,255,255}},
 {0x10,{3,8,255,255}},{0x4e,{0,255,255,255}},{0x53,{5,255,255,255}}};
 for(unsigned i=0;i<6;i++)for(unsigned cid=0;cid<11;cid++){
  int expected=0;for(unsigned k=0;k<4;k++)if(cases[i].owners[k]==cid)expected=1;
  assert(ps4_item_char_allowed(cases[i].item,cid)==expected);
 }
 assert(!ps4_item_char_allowed(0,0)&&!ps4_item_char_allowed(1,11)&&!ps4_item_char_allowed(1,255));
 puts("PASS 66 original equipment-owner cases and three invalid-ID guards");
}
'''
f=O/'r46_equipment_users_probe.c';f.write_text(code);exe=O/'r46_equipment_users_probe'
subprocess.run(['gcc','-std=c99','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie',str(f),'-o',str(exe)],check=True)
p=subprocess.run([str(exe),str(R/'references/roms/ps4built.bin')],capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'));assert p.returncode==0,p.stdout+p.stderr
(O/'R46_EQUIPMENT_USERS_EVIDENCE.json').write_text(json.dumps({'result':'PASS','output':p.stdout.strip(),'scope':'Actual eligibility C against six explicit original-ASM owner lists and real ROM bytes; full equip/menu/save workflow remains open.'},indent=2)+'\n');print(p.stdout,end='')
