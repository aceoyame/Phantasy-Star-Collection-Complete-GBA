#!/usr/bin/env python3
"""Lower Rune/Tonoe source mappings into immutable native GBA OBJ poses."""
from pathlib import Path
import json
from md_compression import nemesis_decompress

R = Path(__file__).resolve().parents[1]
rom = (R / 'references/roms/ps4built.bin').read_bytes()
u32 = lambda p: int.from_bytes(rom[p:p+4], 'big')
shapes = [(8,8), (16,16), (32,32), (64,64), (16,8), (32,8),
          (32,16), (64,32), (8,16), (8,32), (16,32), (32,64), (32,128),(64,128),(128,64),(128,128)]
families = [
    (0x214, 0x4DF04, [0], 0x1D628C),
    (0x218, 0x4DF04, [4], 0x1D642E),
    (0x21C, 0x4DF04, [8,12,16], 0x1D6A2E),
    (0x220, 0x4DF04, [20], 0x1D6BC8),
    (0x1A4, 0x4DE4C, [0], 0x1D49B0),
    (0x1A8, 0x4DE4C, [4], 0x1D49B0),
    (0x1AC, 0x4DE4C, [0,4,8,12,16], 0x140ECE),
    (0x150, 0x4DE40, [0,4], 0x114808),
    (0x1F8, 0x4DEA4, [0,4], 0x164768),
    (0x1FC, 0x4DEAC, list(range(0,72,4)), 0x164768),
    (0x20C, 0x4DEF4, [0,4,8,12], 0x1D5DF0),
    (0x1A0, 0x4DE48, [0], 0x1D48FA),
    (0x8194, 0x4A504, [0,4,8,12], 0x139FF8),
    (0x298, 0x5028E, [0], 0x13B51C),
    (0xC, 0x47E64, [0,4,8,12], 0x291200),
    (0x24, 0x47EC4, [0,4,8,12], 0x294800),
    (0x1F4, 0x4DE98, [0,4,8], 0x1D547C),
    (0x200,0x4DE80,[0],0x1D7E08),
    (0x204,0x4DE80,[4,8,12],0x1D7E08),
    (0x208,0x4DE80,[16],0x1D7E08),
    (0x1B8,0x4DE60,[0],0x1D79B4),
    (0x1BC,0x4DE60,[4],0x1D79B4),
    (0x1C0,0x4DE60,[8],0x1D79B4),
    (0x1C4,0x4DE60,[12,16],0x1D79B4),
    (0x1C8,0x4DE60,[20],0x1D79B4),
    (0x1CC,0x4DE60,[24],0x1D79B4),
    (0x1D0,0x4DE60,[28],0x1D79B4),
    *[(0x210,0x514D0,[world*4],int.from_bytes(rom[0x64538+world*4:0x6453C+world*4],'big')) for world in range(6)],
    (0x78,0x4DE24,[0],0x1D4234),
    (0x7C,0x4DE24,[4],0x1D46C6),
    (0x84,0x4DE24,[12],0x1D3C1E),
    (0x374,0x502A2,[0],0x1D78CA),
    (0x8194,0x4A504,[0,4,8,12],0x19E726,16),
]
blob = bytearray()
poses = []
for family in families:
    kind, table, faces, address = family[:4]
    face_bias = family[4] if len(family)>4 else 0
    art = rom[address:address+0x900] if kind in (0xC,0x24) else nemesis_decompress(rom[address:])
    if kind == 0x1AC:
        # The source scene loads Alys immediately after Dorin's 22 tiles.
        assert len(art) == 22*32
        art += nemesis_decompress(rom[0x1D49B0:])
    for face in faces:
        seq = u32(table+face)
        count = rom[seq] & 127
        variable = bool(rom[seq] & 128)
        maps = seq+1+count+((1+count)&1) if variable else seq+2
        for frame in range(count):
            m = u32(maps+frame*4)
            pieces = []
            for j in range(rom[m]+1):
                q = rom[m+2+j*6:m+8+j*6]
                x, y = int.from_bytes(q[4:5],'big',signed=True), int.from_bytes(q[:1],'big',signed=True)
                w, h = (((q[1]>>2)&3)+1)*8, ((q[1]&3)+1)*8
                pieces.append((x,y,w,h,int.from_bytes(q[2:4],'big')))
            ox, oy = min(p[0] for p in pieces), min(p[1] for p in pieces)
            xmax, ymax = max(x+w for x,y,w,h,p in pieces), max(y+h for x,y,w,h,p in pieces)
            w, h = min((q for q in shapes if q[0]>=xmax-ox and q[1]>=ymax-oy), key=lambda q:q[0]*q[1])
            pixels = bytearray(w*h)
            for x,y,pw,ph,pat in pieces:
                for yy in range(ph):
                    for xx in range(pw):
                        sx,sy = (pw-1-xx if pat&0x800 else xx), (ph-1-yy if pat&0x1000 else yy)
                        tile = (pat&0x7ff)+(sx>>3)*(ph>>3)+(sy>>3)
                        a = tile*32+(sy&7)*4+((sx&7)>>1)
                        assert a<len(art), (hex(kind),face,frame,hex(a),len(art))
                        c = art[a]&15 if sx&1 else art[a]>>4
                        if c: pixels[(y-oy+yy)*w+x-ox+xx] = c
            offset = len(blob)
            for ty in range(h//8):
                for tx in range(w//8):
                    for yy in range(8):
                        for xx in range(0,8,2):
                            p=(ty*8+yy)*w+tx*8+xx
                            blob.append(pixels[p]|(pixels[p+1]<<4))
            poses.append((kind,face+face_bias,frame,offset,w,h,ox,oy,m))
out = ['/* Generated from the original PSIV Nemesis art and sprite mappings. */',
       'struct R31FieldPose { uint16_t type; uint8_t facing,frame; uint32_t offset; uint8_t w,h; int8_t x,y; };',
       'static const struct R31FieldPose r31_field_poses[]={']
out += ['{%du,%du,%du,%du,%du,%du,%d,%d},'%p[:8] for p in poses]
out += ['};','extern const uint8_t r31_field_pose_data[];']
(R/'gba/r31_field_poses.h').write_text('\n'.join(out)+'\n')
(R/'gba/r31_field_poses.bin').write_bytes(blob)
(R/'gba/r31_field_poses_data.s').write_text('''    .section .rodata.r31_field_poses,"a",%progbits
    .balign 32
    .global r31_field_pose_data
r31_field_pose_data:
    .incbin "gba/r31_field_poses.bin"
''')
(R/'generated/r31_field_poses_report.json').write_text(json.dumps({'bytes':len(blob),'poses':[{'type':p[0],'facing':p[1],'frame':p[2],'offset':p[3],'w':p[4],'h':p[5],'x':p[6],'y':p[7],'source_mapping':p[8]} for p in poses]},indent=2)+'\n')
print('PASS R31 native field poses:',len(poses),'bytes:',len(blob))
