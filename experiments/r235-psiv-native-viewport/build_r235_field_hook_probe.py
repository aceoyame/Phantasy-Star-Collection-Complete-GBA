#!/usr/bin/env python3
"""Guarded R235 no-op live field-camera interception test.
Identical relocated ARM shim, not an alternate renderer yet.
Requires clang, ld.lld and llvm-objcopy with ARM7TDMI support.
"""
import argparse, hashlib, struct, subprocess, tempfile, zlib, json
from pathlib import Path
BASE=0x08000000
EXPECTED_SHA='4caf23ae8b030c16a32658fdc95085330de6c6dd4a51498f0e2b9c17c7d89308'
ENTRY=0x08CD3854
SOURCE=0x08CB9F00
RELOCATED=0x08F97D64
EXPECTED_BRANCH=bytes.fromhex('a999ffea')
def build(parent:Path,out:Path,clang='clang',ld='ld.lld',objcopy='llvm-objcopy'):
    original=parent.read_bytes()
    assert len(original)==33554432
    assert hashlib.sha256(original).hexdigest()==EXPECTED_SHA,'expected accepted R235'
    assert original[ENTRY-BASE:ENTRY-BASE+4]==EXPECTED_BRANCH,'hook mismatch'
    with tempfile.TemporaryDirectory() as d:
        p=Path(d)
        linker=p/'probe.ld'
        linker.write_text('SECTIONS { .r235_field_hook_probe 0x%08X : { *(.r235_field_hook_probe) } /DISCARD/ : { *(.ARM.attributes) } }' % RELOCATED)
        obj,elf,raw=p/'probe.o',p/'probe.elf',p/'probe.bin'
        subprocess.run([clang,'--target=arm-none-eabi','-mcpu=arm7tdmi','-marm','-c',str(Path(__file__).with_name('r235_field_hook_probe.S')),'-o',str(obj)],check=True)
        subprocess.run([ld,'-T',str(linker),'-e','r235_field_hook_probe',str(obj),'-o',str(elf)],check=True)
        subprocess.run([objcopy,'-O','binary','--only-section=.r235_field_hook_probe',str(elf),str(raw)],check=True)
        payload=raw.read_bytes()
    assert len(payload)==124
    assert payload==original[SOURCE-BASE:SOURCE-BASE+len(payload)],'not a byte-identical camera shim'
    assert RELOCATED+len(payload)<=0x08F98100,'outside R234 Tier-B sample-42 space'
    displacement=RELOCATED-ENTRY-8
    assert displacement%4==0 and -(1<<25)<=displacement<(1<<25)
    replacement=struct.pack('<I',0xEA000000|((displacement>>2)&0xffffff))
    patched=bytearray(original)
    patched[ENTRY-BASE:ENTRY-BASE+4]=replacement
    patched[RELOCATED-BASE:RELOCATED-BASE+len(payload)]=payload
    changed=[i for i,(a,b) in enumerate(zip(original,patched)) if a!=b]
    assert all(ENTRY-BASE<=i<ENTRY-BASE+4 or RELOCATED-BASE<=i<RELOCATED-BASE+len(payload) for i in changed)
    assert original[:0xC0]==patched[:0xC0]
    out.write_bytes(patched)
    print(json.dumps(dict(parent_sha256=EXPECTED_SHA,candidate_sha256=hashlib.sha256(patched).hexdigest(),candidate_crc32='%08x'%zlib.crc32(patched),rom_bytes=len(patched),changed_bytes=len(changed),field_camera_hook=hex(ENTRY),relocated_shim=hex(RELOCATED),code_bytes=len(payload),no_visual_change=True),indent=2))
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('parent',type=Path);p.add_argument('output',type=Path)
    p.add_argument('--clang',default='clang');p.add_argument('--ld',default='ld.lld');p.add_argument('--objcopy',default='llvm-objcopy')
    a=p.parse_args();build(a.parent,a.output,a.clang,a.ld,a.objcopy)
