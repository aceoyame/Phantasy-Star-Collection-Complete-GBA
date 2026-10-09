#!/usr/bin/env python3
from pathlib import Path
import sys,json,zlib
R=Path(__file__).resolve().parents[2];sys.path.insert(0,str(R/'source/tools_host/r46_mgba'));from mgba import GBA,L,run_until_break
symbols={}
for line in (R/'source/build-psc-module/ps4_psc_module.map').read_text().splitlines():
 p=line.split()
 if len(p)>=5:
  try:symbols[p[-1]]=int(p[0],16)
  except ValueError:pass
rom=R/'binaries/RUN_THIS_RELEASE48_PSC.gba';g=GBA(rom);g.run(120)
def call(name,value):
 L.mg_cpsr(0x1f)
 for i,v in [(0,value),(13,0x03007d80),(14,0x08800000),(15,symbols[name])]:L.mg_set_reg(i,v)
 assert run_until_break(g,0x08800000,30)
def init(ids,alive):
 g.put(0x02000000,bytes(65536));g.put(0x0200f40a,bytes(ids+[255]*(5-len(ids))));g.wr8(symbols['lead_char_id'],0)
 for i in range(11):
  g.put(0x0200f500+i*128+14,(40 if i in alive else 0).to_bytes(2,'big')+(120+i).to_bytes(2,'big')+(17).to_bytes(2,'big'))
  g.wr8(0x0200f500+i*128+22,0 if i in alive else 4)
def hp(i):return int.from_bytes(g.mem(0x0200f500+i*128+14,2),'big')
# Real production removals compact party, source carriers, macros and status.
for last in [4,6,8,9,10]:
 init([0,5,last],[last]);call('r33_party_remove',last);assert hp(0)==120 and hp(5)==0 and g.rd8(0x0200f516)==0
 assert g.mem(0x0200f40a,5)==bytes([0,5,255,255,255])
 assert g.mem(0x0200f512,2)==bytes([0,17])
init([0,4,5,6,3],[6]);call('r33_party_remove',4);assert hp(0)==0;call('r33_party_remove',6);assert hp(0)==120
init([0,5,6],[]);call('r33_party_remove',6);assert hp(0)==hp(5)==0
init([0,5,6],[0,6]);call('r33_party_remove',6);assert hp(0)==40
init([5,6],[6]);call('r33_party_remove',6);assert hp(5)==125
init([0],[]);call('r33_party_remove',6);assert hp(0)==0
report={'rom_crc32':f'{zlib.crc32(rom.read_bytes()):08x}','linked_production_cases':10,'Demi_only_survivor_sequential_Gryz_Demi_removal':True,'other_departing_ids':[4,8,9,10],'healthy_survivor_and_already_wiped_party_unchanged':True,'no_Chaz_fallback':True,'TP_preserved':True,'scope':'Actual linked native party-removal calls on controlled records; Zio scene host test exercises real continuation with display/orchestration stubs. Ordinary-save cutscene/device playthrough still needed.'}
(R/'build_artifacts/RELEASE48_DEPARTURE.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS 10 linked departure cases')
