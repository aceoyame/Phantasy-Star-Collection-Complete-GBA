#!/usr/bin/env python3
"""Persistent SMS/GG SRAM: mapped Z80 writes, protected areas and power-cycle reload."""
from pathlib import Path
import sys,json,struct,subprocess,ctypes
R=Path(__file__).resolve().parents[2];V=R.parent/'validation_cp30'
sys.path.insert(0,str(R/'source/tools_host/cp08_mgba'));from mgba2 import GBA,L
L.mg_break.argtypes=[ctypes.c_uint32];syms={}
for line in subprocess.run(['llvm-nm',str(R/'source/psc8/build/psc8.elf')],capture_output=True,text=True,check=True).stdout.splitlines():
 row=line.split()
 if len(row)==3:syms[row[2]]=int(row[0],16)
rom=R/'binaries/RUN_THIS_R48_CP30_NATIVE_CALLS.gba'
def boot(config,saved):
 g=GBA(rom,entry=0x086ca500);g.put(0xe000000,saved);g.put(0x0203ff00,struct.pack('<IIII',*config,0))
 L.mg_break(syms['z80a_run']);g.run(100);assert L.mg_hit();L.mg_break(0xffffffff);return g
rows=[]
for name,config,base,size in [('PS1',(0x082d3b80,0x80000,10),0x4000,16384),('Gaiden',(166669648,262144,17),0x6000,4096)]:
 initial=bytearray((i*37+19)&255 for i in range(32768));initial[0x7f10:0x7f15]=b'GGSV\1'
 g=boot(config,bytes(initial));assert g.mem(syms['sram'],size)==initial[base:base+size]
 # Execute from guest work RAM so the test does not patch cartridge bytes.
 last=0x8000+size-1
 prog=bytes([0xf3,0x3e,8,0x32,0xfc,0xff,0x3e,0x5a,0x32,0,0x80,0x3e,0xa5,0x32,last&255,last>>8,0x3e,0,0x32,0xfc,0xff,0x76])
 g.put(syms['ram'],prog);g.wr32(syms['z80a_ctx']+0x178,0xc000);g.run(20)
 saved=g.mem(0xe000000,32768);expected=bytearray(initial);expected[base]=0x5a;expected[base+size-1]=0xa5
 assert saved==expected,(name,'mapped write/protected bytes')
 g=boot(config,saved);assert g.mem(syms['sram'],size)==expected[base:base+size]
 assert g.mem(0xe000000,32768)==saved
 rows.append({'game':name,'persistent_bytes':size,'mapped_writes_exact':True,'power_cycle_reload_exact':True,'other_save_bytes_untouched':True});print(name,'mapped SRAM writes and power-cycle reload exact',flush=True)
(V/'PSC8_SAVES_EXACT.json').write_text(json.dumps({'all_pass':True,'cases':rows},indent=2)+'\n')
