#!/usr/bin/env python3
from __future__ import annotations
import argparse, hashlib
from pathlib import Path
from md_compression import nemesis_decompress
EXPECTED='511f35cc11f88316f8b8940e28ab298bd75a4da193672a80172884d6eb913b6a'
ART_OFF=0x2A2B7E
FONT_OFF=0x2A303A
PAL_OFF=0x296300
TILES=(0x00,0x69,0x6A,0x73)
FONT_TILES=87

def c_rows(raw: bytes, width: int = 16) -> str:
    return '\n'.join('    '+','.join(f'0x{x:02X}' for x in raw[i:i+width])+',' for i in range(0,len(raw),width))

def main():
    ap=argparse.ArgumentParser(); ap.add_argument('--rom',required=True,type=Path); ap.add_argument('-o',default='gba/ps4_window_assets.h',type=Path); a=ap.parse_args()
    data=a.rom.read_bytes(); h=hashlib.sha256(data).hexdigest()
    if len(data)!=0x300000 or h!=EXPECTED: raise SystemExit('unexpected PSIV ROM')
    dec=nemesis_decompress(data[ART_OFF:])
    if len(dec)<0x74*32: raise SystemExit('window art too short')
    raw=b''.join(dec[t*32:(t+1)*32] for t in TILES)
    font=nemesis_decompress(data[FONT_OFF:])
    if len(font)<FONT_TILES*32: raise SystemExit('window font art too short')
    font=font[:FONT_TILES*32]
    # Font Nemesis is the exact 8x8 window/menu font. Most text glyphs use
    # palette entries $E/$F; retain all source nibble values for punctuation and
    # special window symbols rather than normalizing them.
    pal=[int.from_bytes(data[PAL_OFF+i:PAL_OFF+i+2],'big') for i in range(0,32,2)]
    txt=f'''#ifndef PS4_WINDOW_ASSETS_H
#define PS4_WINDOW_ASSETS_H
#include <stdint.h>
/* Generated from PSIV ArtNem_WindowTiles, ArtNem_Font and loc_296300.
 * Window tiles are source indices $680(interior), $6E9(corner), $6EA(horizontal), $6F3(vertical).
 * Font tiles are source window-font indices 0..86 (general/tables/wincharset.asm). */
static const uint8_t ps4_window_tiles_4bpp[128] = {{
{c_rows(raw)}
}};
static const uint8_t ps4_window_font_4bpp[{len(font)}] = {{
{c_rows(font)}
}};
static const uint16_t ps4_window_md_palette[16] = {{
    {','.join(f'0x{x:04X}' for x in pal)}
}};
#endif
'''
    a.o.write_text(txt)
    print(f'generated {a.o} from PSIV window art + {FONT_TILES}-tile menu font; palette indices used by border: 2,3,14')
if __name__=='__main__': main()
