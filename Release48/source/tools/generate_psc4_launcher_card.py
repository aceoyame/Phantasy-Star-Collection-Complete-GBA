#!/usr/bin/env python3
"""Generate P17 PSIV native BG3 launcher card on a control-safe P15 base.

P16's shared-card pointer rewrite caused runtime control problems. P17 therefore
keeps P15's launcher hook and stock PSIII gfx/map pointers unchanged. The retail
PSIII card never uses palette indices 14 or 15, so P17 repurposes only those two
otherwise-unused slots for two extra PSIV gold shades. Entries 0..13 remain
byte-identical to the retail PSIII palette.

The approved pre-blue P12 logo pixels are remapped semantically:
  * gold/title pixels -> retail gold/brown + the two extra gold shades;
  * subtitle teal pixels -> the retail PSIII blue ramp;
  * dark outline pixels -> black.
This keeps PSIII visually unchanged while recovering substantially more PSIV
texture and a source-like blue subtitle without any runtime palette ownership
fight.
"""
from pathlib import Path
from collections import Counter
import struct

ROOT=Path(__file__).resolve().parent.parent
GBA=ROOT/'gba'
W,H=192,96
FULL_W,FULL_H=240,160
BLOCKS=[(0,0,64,64),(64,0,64,64),(128,0,64,64),
        (0,64,64,32),(64,64,64,32),(128,64,64,32)]

# Exact retail PSIII card palette. Indices 14/15 are unused by the stock PSIII
# card; those are the only entries P17 changes.
PSIII_PAL_BASE=bytearray(bytes.fromhex(
    '60017b6ff36e6b6ee75d634de04c603c'
    '002c001cf73e770273010000b556b556'
))
# Earlier approved/pre-blue PSIV shades: pale gold and orange. These restore
# highlights/midtones absent from the stock PSIII gold ramp.
EXTRA_GOLD_14=0x43DF  # (255,246,131)
EXTRA_GOLD_15=0x0EBD  # (238,172,24)
struct.pack_into('<H',PSIII_PAL_BASE,14*2,EXTRA_GOLD_14)
struct.pack_into('<H',PSIII_PAL_BASE,15*2,EXTRA_GOLD_15)
P17_PAL=bytes(PSIII_PAL_BASE)

# P12/pre-blue source-index semantics. These three indices are the teal/green
# subtitle ramp; the others are gold/brown title or dark outline.
SUBTITLE_SOURCE={3,6,10}
DARK_SOURCE={0,11,12,13,14,15}


def decode_pal(pal:bytes):
    out=[]
    for i in range(16):
        v=struct.unpack_from('<H',pal,i*2)[0]
        out.append(((v&31)*255//31,((v>>5)&31)*255//31,((v>>10)&31)*255//31))
    return out


def nearest(c, colors, candidates):
    return min(candidates,key=lambda j:sum((c[k]-colors[j][k])**2 for k in range(3)))


def main():
    blob=(GBA/'psc4_logo_obj_4bpp.bin').read_bytes()
    src_pal=(GBA/'psc4_logo_obj_pal.bin').read_bytes()
    if len(blob)!=9216 or len(src_pal)!=32:
        raise SystemExit('unexpected approved logo asset sizes')
    src_colors=decode_pal(src_pal)
    dst_colors=decode_pal(P17_PAL)

    # Reconstruct exact approved 192x96 logo from six object-local blocks.
    pix=[[0]*W for _ in range(H)]
    off=0
    for bx,by,w,h in BLOCKS:
        data=blob[off:off+w*h//2];off+=len(data);p=0
        for ty in range(0,h,8):
            for tx in range(0,w,8):
                tile=data[p:p+32];p+=32
                for y in range(8):
                    for xp in range(4):
                        q=tile[y*4+xp]
                        pix[by+ty+y][bx+tx+xp*2]=q&15
                        pix[by+ty+y][bx+tx+xp*2+1]=q>>4

    # Preserve P12's semantic separation. Title never maps into blue; subtitle
    # never maps into gold. This avoids P15's blue STAR/gold-subtitle crossover.
    title_candidates=(1,10,11,12,13,14,15)
    subtitle_candidates=(1,2,3,4,5,6,7,8,9,13)
    full=[[0]*FULL_W for _ in range(FULL_H)]
    counts=Counter()
    for y in range(H):
        for x in range(W):
            v=pix[y][x]
            if v==0:
                q=0
            elif v in DARK_SOURCE:
                q=13
            elif v in SUBTITLE_SOURCE:
                q=nearest(src_colors[v],dst_colors,subtitle_candidates)
            else:
                q=nearest(src_colors[v],dst_colors,title_candidates)
            full[32+y][24+x]=q
            if q: counts[q]+=1

    # Native PSC 30x20 card with exact-tile deduplication.
    tiles=[bytes(32)];lookup={tiles[0]:0};mapping=[]
    for ty in range(20):
        for tx in range(30):
            t=bytearray()
            for yy in range(8):
                row=full[ty*8+yy][tx*8:tx*8+8]
                for xx in range(0,8,2):
                    t.append((row[xx]&15)|((row[xx+1]&15)<<4))
            tb=bytes(t)
            if tb not in lookup:
                lookup[tb]=len(tiles);tiles.append(tb)
            mapping.append(lookup[tb])

    # PSC launcher card codec: dictionary count 0 + 32 literal bytes/tile.
    enc=bytearray()
    for tile in tiles:
        enc.append(0);enc+=tile
    enc.append(0x80)
    if len(enc)>0x1600: raise SystemExit(f'card gfx too large: {len(enc)}')
    if max(mapping)>0x3ff: raise SystemExit('tile index overflow')

    (GBA/'psc4_card_gfx.bin').write_bytes(enc)
    (GBA/'psc4_card_map.bin').write_bytes(b''.join(struct.pack('<H',v) for v in mapping))
    (GBA/'psc4_card_pal.bin').write_bytes(P17_PAL)
    print(f'P17 control-safe native BG3 card: {len(tiles)} unique tiles, {len(enc)} codec bytes')
    print('P17 palette: retail PSIII entries 0..13 unchanged; 14=pale gold, 15=orange')
    print('P17 used indices:',dict(sorted(counts.items())))

    try:
        from PIL import Image
        im=Image.new('RGBA',(FULL_W,FULL_H),(0,0,0,0))
        for y in range(FULL_H):
            for x in range(FULL_W):
                q=full[y][x]
                if q: im.putpixel((x,y),(*dst_colors[q],255))
        im.save(GBA/'psc4_card_preview_p17.png')
    except Exception:
        pass

if __name__=='__main__': main()
