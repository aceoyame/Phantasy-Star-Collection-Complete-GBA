#!/usr/bin/env python3
"""Independent static ARM7TDMI verifier for Stage21C; not an emulator test."""
import argparse,hashlib,json,struct,zlib
from pathlib import Path
BASE=0x08000000
EXPECTED='ff5f96aae1a536ee13994040ff98bb74a2afbfe1f1a3065df6e996aa1cab5139'
def word(rom,addr):return struct.unpack_from('<I',rom,addr-BASE)[0]
def branch_dest(addr,op):
    assert (op>>25)&7==5 and (op>>28)==14
    d=(op&0xffffff)<<2
    if d&(1<<25):d-=1<<26
    return addr+8+d
def verify(path):
    b=path.read_bytes();assert len(b)==33554432
    assert hashlib.sha256(b).hexdigest()==EXPECTED
    caller=0x08c7d4f4;stub=0x08f97f90;original=0x08c4d13c
    op=word(b,caller)
    assert op>>24==0xeb and branch_dest(caller,op)==stub
    assert [word(b,stub+i) for i in (0,4,8,12)]==[0xe2800010,0xe2811008,0xe51ff004,original]
    assert word(b,0x08cd3854)==0xeaff99a9
    assert word(b,original)==0xe92d4010
    return {'sha256':EXPECTED,'crc32':f'{zlib.crc32(b):08x}', 'bytes':len(b),'branch_from':hex(caller),'branch_to':hex(branch_dest(caller,op)), 'tail_target':hex(original),'actor_hook_restored':True,'test_class':'STATIC_ONLY_NOT_PLAYABLE_QA','risk':'Original BL to wrapper is replaced by BL to tail-call stub; extra 2 ARM ADD instructions and ROM literal load alter CPU cycles/ROM bus traffic. Audio parity and transition first reveal still unverified.'}
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('rom',type=Path);a=p.parse_args();print(json.dumps(verify(a.rom),indent=2))
