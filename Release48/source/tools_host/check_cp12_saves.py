#!/usr/bin/env python3
"""Eight EEPROM slots: power-cycle and reload a valid non-default save word."""
from pathlib import Path
import json,sys
R=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(R/'source/tools_host/cp09_prof'))
import gtest
from gtest import GBA,L
rom=R/'binaries/RUN_THIS_R48_CP12_TEST.gba';ram=gtest.sym('md_ram')
def word(g,a):
 return (g.rd8(ram+((a&65535)^1))<<8)|g.rd8(ram+(((a+1)&65535)^1))
def boot(index,sram=None):
 g=GBA(rom)
 if sram is not None:g.put(0x0e000000,sram)
 g.run(120);L.mg_break(0x080005f0);g.run(10,'START');g.run(900);assert L.mg_hit();L.mg_break(0xffffffff);g.run(50)
 for _ in range(4):g.press('DOWN',1,60)
 L.mg_break(0x08005000);g.run(10,'A');g.run(500);assert L.mg_hit();L.mg_break(0xffffffff);g.run(80)
 for _ in range(index):g.press('DOWN',1,8)
 L.mg_break(0x086e2000);g.press('A',5,0);assert L.mg_hit();L.mg_break(0xffffffff)
 return g
rows=[]
for index in range(2,10):
 g=boot(index);before=g.mem(0x0e000000,32768);g.run(1100)
 sram=bytearray(g.mem(0x0e000000,32768));base=0x7000+(index-2)*128
 assert sram[0x7f00:0x7f05]==b'MDEE\1'
 assert sram[:0x7000]==before[:0x7000] and sram[0x7400:0x7f00]==before[0x7400:0x7f00]
 img=bytearray(sram[base:base+128]);img[4:6]=b'\x12\x34';img[13]=(sum(img[2:13])+0x5a)&255;sram[base:base+128]=img
 g=boot(index,bytes(sram));g.run(1100)
 assert word(g,0xf964)==0x1234,(index,'reload word')
 after=g.mem(0x0e000000,32768)
 changes=[i for i in range(128) if after[base+i]!=img[i]]
 assert after[base+4:base+6]==b'\x12\x34',(index,'reloaded save word not retained',changes)
 assert after[:0x7000]==sram[:0x7000] and after[0x7400:0x7f00]==sram[0x7400:0x7f00]
 rows.append({'slot':index-2,'loaded_word':'1234','other_save_areas_untouched':True,'game_save_updates_during_boot':changes})
 print('EEPROM slot',index-2,'power-cycle/reload PASS',flush=True)
out=R.parent/'validation_cp12/SAVE_RELOAD_CHECK.json';out.write_text(json.dumps({'all_pass':True,'slots':rows},indent=2)+'\n')
