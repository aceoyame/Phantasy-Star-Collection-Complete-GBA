#!/usr/bin/env python3
"""Build an ISOLATED, NOT-ACCEPTED Stage21 OAM camera-coordination prototype.

This test replaces actor-hook camera addition by a matching +16,+8 addition at
native IWRAM sprite-draw entry. The 2-instruction IWRAM branch uses ROM-backed
source (unique verified copy). The stub is in already-approved retired sound
sample 42 space, not sound data currently addressed by the sample table.

A deterministic reproduction still requires running from game startup (IWRAM
code in a loaded savestate may predate the ROM patch!). No commercial assets
are embedded in this builder or uploaded to GitHub.
"""
from pathlib import Path
import hashlib, tempfile, subprocess, struct, json, zlib
BASE=0x08000000
R235_SHA='4caf23ae8b030c16a32658fdc95085330de6c6dd4a51498f0e2b9c17c7d89308'
ST14_SHA='c1ed9db09505fe2d17322efa9f7c4915e7a4cd57bdf2a3dc6f24d0f8f1d15609'
IWRAM_SITE=0x0300286c
ROM_SOURCE_SITE=0x09eee91c
ROM_STUB=0x08f97f60
ACTOR_HOOK=0x08cd3854

def assemble_stub():
  assembly=''' .syntax unified
.cpu arm7tdmi
.arm
.section .text,"ax",%progbits
.global oam_entry_shift
 oam_entry_shift:
 add r0,r0,#16
 add r1,r1,#8
 str r0,[sp,#0x24]
 str r1,[sp,#0x20]
 ldr pc,[pc,#-4]
 .word 0x03002874
'''
  with tempfile.TemporaryDirectory() as d:
   d=Path(d);(d/'stub.S').write_text(assembly)
   (d/'link.ld').write_text(f'SECTIONS {{ .text {ROM_STUB:#x} : {{ *(.text) }} }}\n')
   for cmd in [['clang','--target=arm-none-eabi','-mcpu=arm7tdmi','-marm','-c',str(d/'stub.S'),'-o',str(d/'stub.o')],
                ['ld.lld','-T',str(d/'link.ld'),'-e','oam_entry_shift',str(d/'stub.o'),'-o',str(d/'stub.elf')],
                ['llvm-objcopy','-O','binary','--only-section=.text',str(d/'stub.elf'),str(d/'stub.bin')]]:
    subprocess.run(cmd,check=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
   return (d/'stub.bin').read_bytes()

def main():
 import argparse
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('accepted');p.add_argument('stage14');p.add_argument('output');p.add_argument('--report',required=True);a=p.parse_args()
 accepted=Path(a.accepted).read_bytes();parent=Path(a.stage14).read_bytes()
 assert len(accepted)==len(parent)==33554432
 assert hashlib.sha256(accepted).hexdigest()==R235_SHA
 assert hashlib.sha256(parent).hexdigest()==ST14_SHA
 def check(b,addr,expected):
  off=addr-BASE
  assert b[off:off+len(expected)]==expected,(hex(addr), b[off:off+len(expected)].hex())
 check(parent,ACTOR_HOOK,bytes.fromhex('42110bea'))
 check(accepted,ACTOR_HOOK,bytes.fromhex('a999ffea'))
 check(parent,ROM_SOURCE_SITE,bytes.fromhex('24008de520108de5'))
 check(accepted,ROM_SOURCE_SITE,bytes.fromhex('24008de520108de5'))
 stub=assemble_stub();assert len(stub)==24
 assert ROM_STUB>=0x08f97f40 and ROM_STUB+len(stub)<=0x08f98100
 assert parent[ROM_STUB-BASE:ROM_STUB-BASE+24]==accepted[ROM_STUB-BASE:ROM_STUB-BASE+24]
 body=bytes.fromhex('04f01fe5')+struct.pack('<I',ROM_STUB)
 patches=[(ACTOR_HOOK,accepted[ACTOR_HOOK-BASE:ACTOR_HOOK-BASE+4]),(ROM_SOURCE_SITE,body),(ROM_STUB,stub)]
 result=bytearray(parent)
 for addr,data in patches:
  off=addr-BASE;result[off:off+len(data)]=data
 assert result[:0xc0]==parent[:0xc0]
 assert len(result)==33554432
 diff=sum(x!=y for x,y in zip(parent,result))
 report={'purpose':'Unaccepted isolated Stage21 test; no emulator proof from source alone', 'parent_sha256':ST14_SHA,'accepted_sha256':R235_SHA,'candidate_sha256':hashlib.sha256(result).hexdigest(),'candidate_crc32':f'{zlib.crc32(result):08x}','candidate_bytes':len(result),'changed_bytes':diff,'patches':[{'rom_bus_address':hex(addr),'byte_count':len(data),'old_sha256':hashlib.sha256(parent[addr-BASE:addr-BASE+len(data)]).hexdigest(),'new_sha256':hashlib.sha256(data).hexdigest()} for addr,data in patches], 'note':'State reload may replace patched IWRAM code; verify real runtime source origin per scene before accepting.'}
 Path(a.output).write_bytes(result);Path(a.report).write_text(json.dumps(report,indent=2)+'\n')
 print(json.dumps(report,indent=2))
if __name__=='__main__':main()
