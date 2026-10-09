#!/usr/bin/env python3
"""Generate GBA-ready immutable field graphics and party poses for PSCSTYLE.

The canonical PSIV data stays authoritative. This bank contains only a nibble-order
conversion of immutable graphics streams plus prearranged 16x32 party pose carriers.
"""
from __future__ import annotations
import argparse, hashlib, re, struct, sys
from pathlib import Path
from md_compression import kosinski_decompress, nemesis_decompress
EXPECTED='511f35cc11f88316f8b8940e28ab298bd75a4da193672a80172884d6eb913b6a'

def nib(b:int)->int:return ((b<<4)&0xF0)|(b>>4)

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--rom',required=True,type=Path)
    ap.add_argument('--runtime',default=Path('gba/title_runtime_v06.c'),type=Path)
    ap.add_argument('--out-bin',default=Path('gba/psc_field_fastassets.bin'),type=Path)
    ap.add_argument('--out-c',default=Path('gba/psc_field_fastassets.c'),type=Path)
    ap.add_argument('--out-h',default=Path('gba/psc_field_fastassets.h'),type=Path)
    ap.add_argument('--out-s',default=Path('gba/psc_field_fastassets_data.s'),type=Path)
    a=ap.parse_args();rom=a.rom.read_bytes();sha=hashlib.sha256(rom).hexdigest()
    if len(rom)!=0x300000 or sha!=EXPECTED:raise SystemExit(f'wrong PSIV ROM {len(rom):#x} {sha}')
    u16=lambda p:int.from_bytes(rom[p:p+2],'big');u32=lambda p:int.from_bytes(rom[p:p+4],'big')
    err=u32(8);kg=set();ng=set()
    for mid in range(0x1A1):
        base=u32(0x100000+mid*4)
        if base==err or base>=len(rom):continue
        p=base+2
        while u16(p)<0x8000:kg.add(u32(p+2));p+=6
        p+=2
        while True:
            w=u16(p)
            if w==0xFFFE:p+=8;continue
            if w&0x8000:p+=2;break
            ng.add(u32(p+2));p+=6
    src=a.runtime.read_text()
    for pat,target in [(r'load_kos_vram_auto\(\s*(0x[0-9A-Fa-f]+)u?',kg),(r'load_nem_vram_auto\(\s*(0x[0-9A-Fa-f]+)u?',ng)]:
        for m in re.finditer(pat,src):target.add(int(m.group(1),16))
    ng.update((0x2975AE,0x2976FA,0x297674))
    # Garuberk eye animation frames are immutable Kosinski streams copied into
    # virtual VRAM at runtime.  PSC-style write-time publication can use their
    # GBA-ready forms instead of rebuilding every resident BG tile each frame.
    kg.update((0x1C7460,0x1C79F0,0x1C7FE0))
    blob=bytearray();entries={0:[],1:[]}
    for codec,addrs,fn in ((0,kg,kosinski_decompress),(1,ng,nemesis_decompress)):
        for source in sorted(addrs):
            data=fn(rom[source:])
            if len(data)%32:raise SystemExit(f'graphics stream not tile aligned {source:06X} {len(data)}')
            while len(blob)&31:blob.append(0)
            off=len(blob);blob.extend(nib(b) for b in data);entries[codec].append((source,off,len(data)))
    while len(blob)&31:blob.append(0)
    pose_base=len(blob)
    # Canonical sprite_frame_offset: down=0, up=$30, side=$18; anim seq 0,8,0,16.
    seq=(0,8,0,16);dirs=(0,0x30,0x18)
    for cid in range(11):
        art=0x290000+cid*0x900
        for di,d in enumerate(dirs):
            for anim in range(4):
                frame=d+seq[anim]
                # GBA 1D 16x32 carrier is row-major 2x4; source sheet is column-major.
                for row in range(4):
                    for col in range(2):
                        st=frame+col*4+row;tile=rom[art+st*32:art+(st+1)*32]
                        blob.extend(nib(b) for b in tile)
    while len(blob)&3:blob.append(0)
    a.out_bin.write_bytes(blob)
    a.out_h.write_text('''#ifndef PS4_PSC_FIELD_FASTASSETS_H\n#define PS4_PSC_FIELD_FASTASSETS_H\n#include <stdint.h>\n#define PS4_FASTGFX_KOS 0u\n#define PS4_FASTGFX_NEM 1u\nextern const uint8_t ps4_psc_field_fastassets[];\nextern const uint32_t ps4_psc_pose_bank_offset;\nint ps4_psc_fastgfx_lookup(unsigned codec,uint32_t source,const uint8_t** data,unsigned* length);\nconst uint8_t* ps4_psc_party_pose(uint32_t art,uint8_t facing,unsigned anim);\n#endif\n''')
    cl=['''/* AUTO-GENERATED GBA-ready immutable PSIV field graphics. */\n#include <stdint.h>\n#include "psc_field_fastassets.h"\ntypedef struct { uint32_t source,offset,length; } Ps4FastGfxEntry;''']
    for codec,name in ((0,'kos'),(1,'nem')):
        cl.append(f'static const Ps4FastGfxEntry {name}_gfx[]={{')
        for source,off,n in entries[codec]:cl.append(f'{{0x{source:06X}u,0x{off:08X}u,{n}u}},')
        cl.append('};')
    cl.append(f'const uint32_t ps4_psc_pose_bank_offset={pose_base}u;')
    cl.append('''static int find_gfx(const Ps4FastGfxEntry*e,unsigned n,uint32_t source,const uint8_t**data,unsigned*length){
 unsigned lo=0u,hi=n;while(lo<hi){unsigned m=lo+((hi-lo)>>1);uint32_t s=e[m].source;if(s<source)lo=m+1u;else hi=m;}
 if(lo>=n||e[lo].source!=source)return 0;*data=ps4_psc_field_fastassets+e[lo].offset;*length=e[lo].length;return 1;
}
int ps4_psc_fastgfx_lookup(unsigned codec,uint32_t source,const uint8_t**data,unsigned*length){
 if(codec==PS4_FASTGFX_KOS)return find_gfx(kos_gfx,(unsigned)(sizeof(kos_gfx)/sizeof(kos_gfx[0])),source,data,length);
 if(codec==PS4_FASTGFX_NEM)return find_gfx(nem_gfx,(unsigned)(sizeof(nem_gfx)/sizeof(nem_gfx[0])),source,data,length);return 0;
}
const uint8_t* ps4_psc_party_pose(uint32_t art,uint8_t facing,unsigned anim){
 if(art<0x290000u||art>=0x296300u||((art-0x290000u)%0x900u)!=0u)return 0;
 unsigned id=(unsigned)((art-0x290000u)/0x900u),dir=facing==1u?1u:((facing==2u||facing==3u)?2u:0u);
 return ps4_psc_field_fastassets+ps4_psc_pose_bank_offset+(((id*3u+dir)*4u+(anim&3u))*256u);
}
''')
    a.out_c.write_text('\n'.join(cl))
    a.out_s.write_text('''    .section .rodata.psc_field_fastassets,"a",%progbits\n    .balign 32\n    .global ps4_psc_field_fastassets\n    .type ps4_psc_field_fastassets, %object\nps4_psc_field_fastassets:\n    .incbin "gba/psc_field_fastassets.bin"\n    .size ps4_psc_field_fastassets, .-ps4_psc_field_fastassets\n''')
    print(f'PASS PSC field fastassets: kos_gfx={len(entries[0])} nem_gfx={len(entries[1])} gfx_bytes={pose_base} poses=132 pose_bytes={132*256} total={len(blob)}')
if __name__=='__main__':main()
