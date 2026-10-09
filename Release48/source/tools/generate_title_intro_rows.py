#!/usr/bin/env python3
from __future__ import annotations
import argparse, hashlib
from pathlib import Path
from md_compression import nemesis_decompress, enigma_decompress
EXPECTED='511f35cc11f88316f8b8940e28ab298bd75a4da193672a80172884d6eb913b6a'
ROW_W=240; LOGICAL_FIRST=32; LOGICAL_LAST=477

def main()->None:
    ap=argparse.ArgumentParser();ap.add_argument('--rom',required=True,type=Path)
    ap.add_argument('--out',default=Path('gba/title_intro_rows.bin'),type=Path)
    ap.add_argument('--header',default=Path('gba/title_intro_rows.h'),type=Path)
    a=ap.parse_args();rom=a.rom.read_bytes();sha=hashlib.sha256(rom).hexdigest()
    if len(rom)!=0x300000 or sha!=EXPECTED: raise SystemExit('wrong PSIV ROM '+sha)
    vram=bytearray(65536)
    for src,tile,length in ((0x1CD370,0x00CA,0x01A0),(0x1CD2A6,0x00D8,0x0160),(0x1CD462,0x00E3,0x33C0),(0x2A2B7E,0x0680,0x1000),(0x2A303A,0x07C0,0x0AE0),(0x2A303A,0x0681,0x0AE0)):
        d=nemesis_decompress(rom[src:])
        if len(d)!=length: raise SystemExit(f'Nemesis length {src:06X}: {len(d)} != {length}')
        off=tile*32;vram[off:off+length]=d
    for t in range(16):vram[t*32:(t+1)*32]=bytes([t*17])*32
    for src,off,length in ((0x1CC47A,0x0200,0x1040),(0x1CCEAA,0x3040,0x0700)):
        d=nemesis_decompress(rom[src:])
        if len(d)!=length:raise SystemExit(f'Nemesis title length {src:06X}')
        vram[off:off+length]=d
    plane=[0]*2048
    def plane_map(words,addr,w,h):
        k=0;a1=addr
        for _y in range(h):
            a2=a1
            for _x in range(w):
                if 0x9000<=a2<0xA000:plane[((a2-0x9000)&0x0FFF)>>1]=words[k]
                k+=1;a2=(a2+2)&0xFFFF
                if (a2&0x7F)==0:a2=(a2-0x80)&0xFFFF
            a1=(a1+0x80)&0xFFFF
            if (a1&0x0F80)==0:a1=(a1-0x1000)&0xFFFF
    intro=enigma_decompress(rom[0x1D2172:],0x60E3);bottom=enigma_decompress(rom[0x1D223E:],0x60E3)
    if len(intro)!=224 or len(bottom)!=224:raise SystemExit('bad intro Enigma sizes')
    plane_map(intro,0x920C,8,28);plane_map(bottom[:8],0x900C,8,1)
    def tile_px(e,x,y):
        if e&0x0800:x=7-x
        if e&0x1000:y=7-y
        o=(((e&0x07FF)*32)+(y*4)+(x>>1))&0xFFFF;v=vram[o]
        return (v&15) if (x&1) else (v>>4)
    def sample_b(sx,sy):
        e=plane[((sy>>3)&31)*64+((sx>>3)&63)];i=tile_px(e,sx&7,sy&7)
        return ((((e>>13)&3)<<4)|i) if i else 0
    def render_row(counter,y):
        sy=((7*y)//5+32+counter)&255
        return bytes(sample_b((4*x)//3,sy) for x in range(ROW_W))
    rows={}
    for counter in range(0xE0):
        if counter and (counter&7)==0:
            r=counter>>3;plane_map(bottom[r*8:r*8+8],0x900C+(r<<7),8,1)
        for y in range(160):
            logical=(7*y)//5+32+counter;row=render_row(counter,y);old=rows.get(logical)
            if old is not None and old!=row:raise SystemExit(f'row invariant failed counter={counter} y={y} logical={logical}')
            rows[logical]=row
    expected=set(range(LOGICAL_FIRST,LOGICAL_LAST+1))
    if set(rows)!=expected:raise SystemExit(f'row range mismatch {min(rows)}..{max(rows)} count={len(rows)}')
    payload=b''.join(rows[y] for y in range(LOGICAL_FIRST,LOGICAL_LAST+1))
    a.out.parent.mkdir(parents=True,exist_ok=True);a.out.write_bytes(payload)
    a.header.write_text(f'''#ifndef PS4_TITLE_INTRO_ROWS_H\n#define PS4_TITLE_INTRO_ROWS_H\n#include <stdint.h>\n#define PS4_TITLE_INTRO_ROW_WIDTH {ROW_W}u\n#define PS4_TITLE_INTRO_FIRST_Y {LOGICAL_FIRST}u\n#define PS4_TITLE_INTRO_LAST_Y {LOGICAL_LAST}u\n#define PS4_TITLE_INTRO_ROW_COUNT {LOGICAL_LAST-LOGICAL_FIRST+1}u\nextern const uint8_t ps4_title_intro_rows[];\n#endif\n''')
    print(f'PASS title intro rows: logical={LOGICAL_FIRST}..{LOGICAL_LAST} rows={len(rows)} bytes={len(payload)}')
if __name__=='__main__':main()
