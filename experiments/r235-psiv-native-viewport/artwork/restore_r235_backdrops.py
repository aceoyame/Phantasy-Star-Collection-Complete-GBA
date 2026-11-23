#!/usr/bin/env python3
"""Preserve accepted R235 battle scene composition while keeping Stage7 sprite work.
No game data is embedded. Requires user-provided SHA-verified original and Stage7 ROMs.
"""
from pathlib import Path
import argparse,hashlib,json,struct,zlib

PARENT_SHA="edf7b67c1c651fed51a12f248aa560c191b3ba0cf70f003fb71d7136b0f645be"
ACCEPTED_SHA="4caf23ae8b030c16a32658fdc95085330de6c6dd4a51498f0e2b9c17c7d89308"
OUTPUT_SHA="7d5afd0b0472542a700aa6af8e8c46dc7a86115cf18ad3567d4bb5f753b08fff"
IDS=(0,2,4,5,16,17,20,21,22,23,24,25,26)
TABLE=0xBA815C
START=0x0924CE1A-0x08000000
END=0x0926F40C-0x08000000

def sha(raw):return hashlib.sha256(raw).hexdigest()

def build(accepted_path,stage7_path,output_path):
    original=accepted_path.read_bytes()
    stage7=stage7_path.read_bytes()
    assert len(original)==len(stage7)==33554432
    assert sha(original)==ACCEPTED_SHA and sha(stage7)==PARENT_SHA
    assert not any(original[START:END])
    assert any(stage7[START:END])
    assert all(not START<=TABLE+struct.unpack_from('<I',stage7,TABLE+i*4)[0]<END
               for i in range(32) if i not in IDS)
    out=bytearray(stage7)
    for i in IDS:
        at=TABLE+i*4
        old=struct.unpack_from('<I',original,at)[0]
        new=struct.unpack_from('<I',stage7,at)[0]
        assert old!=new and START<=TABLE+new<END
        out[at:at+4]=original[at:at+4]
    out[START:END]=original[START:END]
    assert out[:0xC0]==stage7[:0xC0]
    assert all(out[TABLE+i*4:TABLE+i*4+4]==original[TABLE+i*4:TABLE+i*4+4] for i in IDS)
    assert all(out[TABLE+i*4:TABLE+i*4+4]==stage7[TABLE+i*4:TABLE+i*4+4]
               for i in range(32) if i not in IDS)
    diffs=[i for i,(a,b) in enumerate(zip(stage7,out)) if a!=b]
    assert all(START<=i<END or any(TABLE+j*4<=i<TABLE+j*4+4 for j in IDS) for i in diffs)
    assert len(diffs)==137417 and sha(out)==OUTPUT_SHA
    output_path.write_bytes(out)
    result={'parent_sha256':PARENT_SHA,'output_sha256':OUTPUT_SHA,
            'crc32':'%08x'%zlib.crc32(out),'rom_bytes':len(out),
            'restored_background_indices':IDS,
            'bytes_changed':len(diffs),'freed_rom_bytes':END-START,
            'preserved':'manga40, muted portrait39, stage4 field crop, native enemy 06/10',
            'status':'experimental; real Rykros battle and hardware not yet replayed'}
    output_path.with_suffix('.manifest.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('accepted_r235',type=Path)
    p.add_argument('stage7',type=Path)
    p.add_argument('output',type=Path)
    a=p.parse_args()
    build(a.accepted_r235,a.stage7,a.output)
