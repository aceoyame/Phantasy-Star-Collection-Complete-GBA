#!/usr/bin/env python3
"""Append only original Torch thieves, light circles, ash and Bishop OBJ poses. Run once against R40; immutable R40 prefix is retained."""
from pathlib import Path
import json
from md_compression import nemesis_decompress

R = Path(__file__).resolve().parents[1]
rom = (R / 'references/roms/ps4built.bin').read_bytes()
u32 = lambda p: int.from_bytes(rom[p:p+4], 'big')
shapes = [(8,8), (16,16), (32,32), (64,64), (16,8), (32,8),
          (32,16), (64,32), (8,16), (8,32), (16,32), (32,64), (32,128),(64,128),(128,64),(128,128)]
families = [
 (0x280,0x502F6,[0],0x1B1D06),
 (0x284,0x502F6,[8],0x1B1D06),
 (0x288,0x502F6,[12],0x1B1D06),
 (0x28C,0x502F6,[4],0x1B1D06),
 (0x290,0x502F6,[16],0x1B1D06),
 (0x338,0x502C2,[0,4,8],0x1D712C),
 (0x8194,0x4A504,[0,4,8,12],0x1A5F38,32),
]
assert not (R/"generated/r41_field_poses_report.json").exists(), "Already appended R41 poses; restore the immutable R40 input first"
old_blob=(R/"gba/r31_field_poses.bin").read_bytes()
old_header=(R/"gba/r31_field_poses.h").read_text()
blob = bytearray(old_blob)
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
rows=out[3:-2]
assert rows and old_header.count("static const struct R31FieldPose")==1
(R/"gba/r31_field_poses.h").write_text(old_header.replace("\n};", "\n"+"\n".join(rows)+"\n};"))
(R/'gba/r31_field_poses.bin').write_bytes(blob)
(R/'gba/r31_field_poses_data.s').write_text('''    .section .rodata.r31_field_poses,"a",%progbits
    .balign 32
    .global r31_field_pose_data
r31_field_pose_data:
    .incbin "gba/r31_field_poses.bin"
''')
(R/'generated/r41_field_poses_report.json').write_text(json.dumps({'bytes':len(blob),'retained_r40_bytes':len(old_blob),'appended_bytes':len(blob)-len(old_blob),'poses':[{'type':p[0],'facing':p[1],'frame':p[2],'offset':p[3],'w':p[4],'h':p[5],'x':p[6],'y':p[7],'source_mapping':p[8]} for p in poses]},indent=2)+'\n')
cumulative=R/'generated/r31_field_poses_report.json'
report=json.loads(cumulative.read_text());assert report['bytes']==len(old_blob)
report['bytes']=len(blob);report['poses'].extend(json.loads((R/'generated/r41_field_poses_report.json').read_text())['poses'])
cumulative.write_text(json.dumps(report,indent=2)+'\n')
print('PASS R41 appended native field poses:',len(poses),'bytes:',len(blob))
