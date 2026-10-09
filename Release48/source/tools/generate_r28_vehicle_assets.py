#!/usr/bin/env python3
"""Lower the three original vehicle battle planes and source stat/skill tables."""
from pathlib import Path
from PIL import Image
from md_compression import nemesis_decompress,enigma_decompress
import struct
R=Path(__file__).resolve().parents[1];rom=(R/'references/roms/ps4built.bin').read_bytes()
records=bytes.fromhex('02e440003c46c85032010201010102030000000000000203080803c050003046fa5032010201010102030000000000000250080402a8ff00604696462801020101010203000000000000020c0402')
vs=rom.find(records);assert vs>=0 and rom.find(records,vs+1)<0
skills=bytes.fromhex('0101028006010000010102f0070400000101020007070000010102f0070200000101028007030000010102800601000002010220020e00000101028006010000')
ss=rom.find(skills);assert ss>=0 and rom.find(skills,ss+1)<0
pal=[];pixels=[]
for vi,(art,mp) in enumerate([(0x27AE00,0x27B7BE),(0x27B8C6,0x27C2DE),(0x27C3D8,0x27CF66)],1):
    pool=bytearray(65536)
    for ad,base in [(0x27D04E,0x186),(0x27D152,0x196),(art,0x19F)]:
        raw=nemesis_decompress(rom[ad:]);pool[base*32:base*32+len(raw)]=raw
    cells=enigma_decompress(rom[mp:],0xE19F);assert len(cells)==40*13,(vi,len(cells))
    im=Image.new('P',(320,104),0)
    for ty in range(13):
        for tx in range(40):
            e=cells[ty*40+tx];tile=pool[(e&0x7FF)*32:((e&0x7FF)+1)*32]
            for y in range(8):
                for x in range(8):
                    xx=7-x if e&0x800 else x;yy=7-y if e&0x1000 else y
                    b=tile[yy*4+(xx//2)];ci=(b&15) if xx&1 else b>>4
                    if ci:im.putpixel((tx*8+x,ty*8+y),160+ci)
    out=im.resize((240,74),Image.Resampling.NEAREST);pixels.extend(out.tobytes())
    colors=[0]+[int.from_bytes(rom[0x76A4+vi*30+j*2:0x76A4+vi*30+j*2+2],'big') for j in range(15)]
    cv=(0,4,9,13,18,22,27,31);g=[cv[(w>>1)&7]|cv[(w>>5)&7]<<5|cv[(w>>9)&7]<<10 for w in colors];pal.append(g)
    rgb=[0]*768
    for ci,w in enumerate(colors):rgb[(160+ci)*3:(160+ci)*3+3]=[int(((w>>k)&7)*255/7) for k in (1,5,9)]
    out.putpalette(rgb);out.save(R.parent/'build_artifacts'/f'R28_VEHICLE_{vi}.png')
h=['/* Original vehicle battle art, no live decompression. */\n',f'#define R28_VEHICLE_DATA 0x{vs:06X}u\n#define R28_VEHICLE_SKILLS 0x{ss:06X}u\n', 'static const char* const r28_vehicle_names[]={"LANDMASTER","ICE DIGGER","HYDROFOIL"};\nstatic const char* const r28_vehicle_skill_names[]={"CANNON","CLUSTER","GRAVITN","TH.GRID","X-BURST","NAPALM","NOTHING","N-SPHER","NOTHING"};\n', 'static const uint16_t r28_vehicle_palette[3][16]={\n']
h += ['{'+','.join(hex(w) for w in g)+'},\n' for g in pal];h += ['};\nstatic const uint8_t r28_vehicle_pixels[3][240*74]={\n']
for vi in range(3):h.append('{\n'+''.join(','.join(str(v) for v in pixels[j:j+24])+',\n' for j in range(vi*240*74,(vi+1)*240*74,24))+'},\n')
h.append('};\n');(R/'gba/r28_vehicle_assets.h').write_text(''.join(h));print('vehicle tables',hex(vs),hex(ss),'pixels',len(pixels))
