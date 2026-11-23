#!/usr/bin/env python3
"""Stage12: Genesis static body-detail patch for enemy IDs 07 and 08.

Inputs: exact Stage11 experimental ROM and verified US Genesis PSIV ROM.
Requires md_compression.py and enemy_tiles.py from the private R203 source tools.
No copyrighted assets are stored in this repository.
"""
import argparse,hashlib,struct,zlib,json,sys
from pathlib import Path
try:
    from md_compression import nemesis_decompress,enigma_decompress
    from enemy_tiles import animated_tiles,ASSET,NATIVE,BODY,MAP,PALETTE
except ImportError:
    raise SystemExit("Put R203 r198/reference_tools and r199 on PYTHONPATH")
PARENT_SHA='820892b5fa2c145a249876baba90144152d7ff1520fb865e43d2103bc2bb80b2'
GEN_SHA='511f35cc11f88316f8b8940e28ab298bd75a4da193672a80172884d6eb913b6a'
TEST_SHA='5ff6d48606576e19ee46d14d7d8892b9835d4161afebde74f781caf76d38ff02'
ENEMIES=(0x07,0x08)
def sha(b):return hashlib.sha256(b).hexdigest()
def gba_color(w):return sum((((w>>src)&7)*255//7>>3)<<dst for src,dst in ((1,0),(5,5),(9,10)))
def build(gba,gen):
    assert len(gba)==33554432 and sha(gba)==PARENT_SHA, 'Stage11 SHA mismatch'
    assert len(gen)==3145728 and sha(gen)==GEN_SHA, 'Genesis SHA mismatch'
    candidate=bytearray(gba);allowed=[];records=[]
    for eid in ENEMIES:
        _,ap,bp,mp,hw,h=struct.unpack_from('>IIII2B',gen,0x27F3AE+eid*20+2)
        src=nemesis_decompress(gen[bp:])
        anim,alen,body,size,mmap,n,w,hh=struct.unpack_from('<8I',gba,NATIVE+eid*32)
        assert len(src)==size and w==hw*2 and hh==h and n==w*h
        assert tuple(enigma_decompress(gen[mp:])[:n])==struct.unpack_from('<'+str(n)+'H',gba,MAP+mmap)
        colors=(0,0xEEE,0,*struct.unpack_from('>11H',gen,0x27FFA2+eid*22),0,0)
        assert tuple(map(gba_color,colors))==struct.unpack_from('<16H',gba,PALETTE+eid*32)
        protected=animated_tiles(gen,eid)
        altered=0;tile_count=0
        for tile in range(size//32):
            if tile in protected:continue
            off=BODY+body+tile*32
            target=src[tile*32:(tile+1)*32]
            changes=sum(a!=b for a,b in zip(gba[off:off+32],target))
            if changes:
                candidate[off:off+32]=target
                altered+=changes;tile_count+=1
            allowed.append((off,off+32))
        for tile in protected:
            if tile<size//32:
                off=BODY+body+tile*32
                assert candidate[off:off+32]==gba[off:off+32], 'animation tile mutated'
        records.append(dict(enemy=f'{eid:02X}',size_px=[w*8,hh*8],changed_tiles=tile_count,changed_bytes=altered))
    changed=[i for i,(a,b) in enumerate(zip(gba,candidate)) if a!=b]
    assert all(any(lo<=i<hi for lo,hi in allowed) for i in changed)
    assert candidate[:0xC0]==gba[:0xC0], 'header changed'
    assert sha(candidate)==TEST_SHA, 'candidate not identical to emulator-tested build'
    return bytes(candidate),dict(stage11_sha=PARENT_SHA,stage12_sha=sha(candidate),
       genesis_sha=GEN_SHA,crc32=f'{zlib.crc32(candidate):08x}',size=len(candidate),
       changed_rom_bytes=len(changed),enemy_records=records,
       untested='full attack/death animations, full story to FIN, GBA Micro')
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for arg in ('stage11','genesis','output'):p.add_argument(arg,type=Path)
    a=p.parse_args()
    output,record=build(a.stage11.read_bytes(),a.genesis.read_bytes())
    a.output.write_bytes(output)
    a.output.with_suffix('.manifest.json').write_text(json.dumps(record,indent=2)+'\n')
    print(json.dumps(record,indent=2))
