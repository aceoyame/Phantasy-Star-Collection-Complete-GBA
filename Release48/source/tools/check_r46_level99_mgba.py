#!/usr/bin/env python3
"""Linked production function: capped records and subsequent characters isolated."""
from pathlib import Path
import json,sys,zlib
R=Path(__file__).resolve().parents[2];S=R/'source'
sys.path.insert(0,str(S/'tools_host/r46_mgba'));from mgba import GBA,L,run_until_break
symbols={}
for line in (S/'build-psc-module/ps4_psc_module.map').read_text().splitlines():
 p=line.split()
 if len(p)>=5:
  try:symbols[p[-1]]=int(p[0],16)
  except ValueError:pass
rom=R/'binaries/RUN_THIS_RELEASE48_PSC.gba';g=GBA(rom)
g.run(120)
ident={'rom_crc32':f'{zlib.crc32(rom.read_bytes()):08x}'}
original=(S/'references/roms/ps4built.bin').read_bytes()
tables=[0x2856b0,0x285f1c,0x286704,0x286f70,0x28767c,0x287e7a,0x2886e6,0x288e60,0x28952a,0x289b86,0x28a1b6];start=[1,7,1,17,6,1,12,20,25,27,35]
def call(ci):
 L.mg_break(0xffffffff);L.mg_cpsr(0x1f)
 for i,v in [(0,ci),(13,0x03007d80),(14,0x08800000),(15,symbols['battle_apply_levelups_one'])]:L.mg_set_reg(i,v)
 assert run_until_break(g,0x08800000,20)
 return g.reg(0)
def record(ci):return 0x0200f500+ci*128
rows=[]
for ci in range(11):
 data=bytearray((i*17+ci*13)&255 for i in range(128));data[8:10]=(99).to_bytes(2,'big');data[10:14]=(9999999).to_bytes(4,'big');g.put(record(ci),data)
 assert call(ci)==0 and g.mem(record(ci),128)==data
 for other in range(10):
  if other==ci:continue
  p=tables[other]+(98-start[other])*22;rec=original[p:p+22]
  d=bytearray(128);d[8:10]=(98).to_bytes(2,'big');d[10:14]=rec[:4];g.put(record(other),d)
  assert call(other)==1
  assert g.mem(record(ci),128)==data,'capped character was corrupted by another level-up'
  assert g.mem(record(other)+8,2)==bytes([0,99])
  assert g.mem(record(other)+16,2)==rec[4:6] and g.mem(record(other)+20,2)==rec[6:8]
  for offset,j in [(24,8),(27,9),(30,10),(33,11)]:assert g.rd8(record(other)+offset)==rec[j]
  post=g.mem(record(other),128);assert call(other)==0 and g.mem(record(other),128)==post
  rows.append({'capped_id':ci,'other_id':other,'level_98_to_99_correct':True,'whole_capped_record_preserved':True,'subsequent_level_99_call_noop':True})
report={'rom_crc32':ident['rom_crc32'],'capped_character_records_checked':11,'cross_character_cases':len(rows),'cases':rows,'scope':'Linked native per-character level-up; original OutsidePartyExp shared-table-pointer fault cannot occur in this implementation. Not a normal-save level-99 playthrough; outside-party EXP progression is separate.'}
(R/'build_artifacts/RELEASE48_LEVEL99.json').write_text(json.dumps(report,indent=2)+'\n')
print('PASS 11 capped records,',len(rows),'cross-character transitions 98 -> 99, repeated cap no-op')
