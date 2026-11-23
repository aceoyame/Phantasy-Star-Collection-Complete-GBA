#!/usr/bin/env python3
"""Build the separate R235 Stage7 source-native static enemy-art experiment.

User supplies the SHA-verified Stage6 test ROM, original US Genesis PSIV ROM,
and R203 complete source archive. Never upload proprietary input/output ROMs.
"""
import argparse,hashlib,struct,zlib,json,tempfile,zipfile,sys
from pathlib import Path
GBA_SHA='e8d5418560cdd43fac46a6e5214b7adc691c17b8f4e3bc1eddb58741a77f57b0'
GEN_SHA='511f35cc11f88316f8b8940e28ab298bd75a4da193672a80172884d6eb913b6a'
OUT_SHA='edf7b67c1c651fed51a12f248aa560c191b3ba0cf70f003fb71d7136b0f645be'
ASSET=0xB8B95C;NATIVE=ASSET+1346048;BODY=ASSET+1646592
MAP=ASSET+1781984;PALETTE=ASSET+1808608;ENEMIES=(0x06,0x10)
def sha(b):return hashlib.sha256(b).hexdigest()
def gba_color(w):
    return sum((((w>>src)&7)*255//7>>3)<<dst for src,dst in ((1,0),(5,5),(9,10)))
def animated_tiles(gen,eid):
    table=0x10FCE
    at=table+struct.unpack_from('>h',gen,table+eid*2)[0]
    n=struct.unpack_from('>H',gen,at)[0]+1
    result=set()
    for i in range(n):
        start,count=struct.unpack_from('>BB',gen,at+2+i*6)
        result.update(range(start,start+count))
    return result
def patch(parent,gen,nemesis,enigma):
    assert len(parent)==33554432 and sha(parent)==GBA_SHA,'wrong Stage6 parent'
    assert len(gen)==3145728 and sha(gen)==GEN_SHA,'wrong original Genesis ROM'
    image=bytearray(parent);allowed=[];records=[]
    for eid in ENEMIES:
        _,_,bp,mp,half_w,h=struct.unpack_from('>IIII2B',gen,0x27F3AE+eid*20+2)
        source=nemesis(gen[bp:])
        _,_,body,nbytes,mapoff,count,w,hh=struct.unpack_from('<8I',parent,NATIVE+32*eid)
        assert len(source)==nbytes and w==2*half_w and hh==h and count==w*h
        assert tuple(enigma(gen[mp:])[:count])==struct.unpack_from(f'<{count}H',parent,MAP+mapoff),'source map mismatch'
        source_colors=(0,0xEEE,0,*struct.unpack_from('>11H',gen,0x27FFA2+eid*22),0,0)
        assert tuple(map(gba_color,source_colors))==struct.unpack_from('<16H',parent,PALETTE+32*eid),'palette mismatch'
        protect=animated_tiles(gen,eid)
        tiles=[];edited=0
        for tile in range(nbytes//32):
            at=BODY+body+32*tile
            if tile in protect:continue
            target=source[32*tile:32*(tile+1)]
            n=sum(a!=b for a,b in zip(parent[at:at+32],target))
            if n:image[at:at+32]=target;tiles.append(tile);edited+=n
            allowed.append((at,at+32))
        records.append(dict(id=f'{eid:02X}',native_size_px=[w*8,h*8],edited_bytes=edited,edited_static_tile_count=len(tiles),protected_animated_tiles=len(protect)))
    changed=[i for i,(a,b) in enumerate(zip(parent,image)) if a!=b]
    assert all(any(lo<=i<hi for lo,hi in allowed) for i in changed)
    assert parent[:0xC0]==image[:0xC0] and sha(image)==OUT_SHA
    return bytes(image),dict(candidate_sha256=OUT_SHA,parent_sha256=GBA_SHA,
       crc32=f'{zlib.crc32(image):08x}',rom_bytes=len(image),
       changed_bytes=len(changed),enemies=records,
       warning='isolated experiment: not a FIN playthrough or all-animation acceptance')
def main():
    ap=argparse.ArgumentParser(description=__doc__)
    for name in ('stage6','genesis','r203_archive','output'):
        ap.add_argument('--'+name.replace('_','-'),type=Path,required=True)
    a=ap.parse_args()
    with tempfile.TemporaryDirectory() as folder:
        path=Path(folder)
        with zipfile.ZipFile(a.r203_archive) as z:
            (path/'md_compression.py').write_bytes(z.read('r198/reference_tools/md_compression.py'))
        sys.path.insert(0,str(path))
        from md_compression import nemesis_decompress,enigma_decompress
        image,meta=patch(a.stage6.read_bytes(),a.genesis.read_bytes(),nemesis_decompress,enigma_decompress)
    a.output.write_bytes(image)
    a.output.with_suffix('.manifest.json').write_text(json.dumps(meta,indent=2)+'\n')
    print(json.dumps(meta,indent=2))
if __name__=='__main__':main()
