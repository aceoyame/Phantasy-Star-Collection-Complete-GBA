#!/usr/bin/env python3
"""Generate a PSC-style build-time decoded PSIV field-asset bank.

The GBA runtime keeps PSIV source addresses as stable identities, but immutable
Kosinski/Nemesis streams used by field maps are decoded on the host.  Runtime
lookup therefore becomes ROM->EWRAM DMA/copy, with the canonical decoder kept
as an exact fallback for streams not in this bank.
"""
from __future__ import annotations
import argparse, hashlib, re, os
from pathlib import Path
from md_compression import kosinski_decompress, nemesis_decompress
EXPECTED='511f35cc11f88316f8b8940e28ab298bd75a4da193672a80172884d6eb913b6a'

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--rom',required=True,type=Path)
    ap.add_argument('--runtime',default=Path('gba/title_runtime_v06.c'),type=Path)
    ap.add_argument('--out-bin',default=Path('gba/psc_field_predecode.bin'),type=Path)
    ap.add_argument('--out-c',default=Path('gba/psc_field_predecode.c'),type=Path)
    ap.add_argument('--out-h',default=Path('gba/psc_field_predecode.h'),type=Path)
    ap.add_argument('--out-s',default=Path('gba/psc_field_predecode_data.s'),type=Path)
    args=ap.parse_args()
    rom=args.rom.read_bytes()
    if len(rom)!=0x300000:raise SystemExit(f'wrong PSIV ROM size {len(rom):#x}')
    if os.environ.get('PS4_NO_HASH')!='1':
        sha=hashlib.sha256(rom).hexdigest()
        if sha!=EXPECTED:raise SystemExit(f'wrong PSIV ROM {sha}')
    def u16(p): return int.from_bytes(rom[p:p+2],'big')
    def u32(p): return int.from_bytes(rom[p:p+4],'big')
    def rec(p,n):
        while u16(p)!=0xffff:p+=n
        return p+2
    def ff(p):
        while rom[p]!=0xff:p+=1
        return p+1
    err=u32(8); kos=set(); nem=set(); valid=0
    # Exact parser shape shared with the native runtime/audit tools.
    for mid in range(0x1a1):
        base=u32(0x100000+mid*4)
        if base==err or base>=len(rom): continue
        valid+=1; p=base+2
        while u16(p)<0x8000: kos.add(u32(p+2)); p+=6
        p+=2
        while True:
            w=u16(p)
            if w==0xfffe: nem.add(u32(p+4)); p+=8; continue
            if w&0x8000: p+=2; break
            nem.add(u32(p+2)); p+=6
        while u16(p)!=0xffff: kos.add(u32(p+2)); p+=6
        p+=2; p+=6
        for _ in range(2):
            mode=u16(p);p+=2
            if not mode:p+=8
        while u16(p)!=0xffff: kos.add(u32(p));p+=4
        p+=2; p=ff(p)
        for n in (10,10,10,6,2): p=rec(p,n)
        if mid>=2: kos.add(u32(p));p+=4;kos.add(u32(p));p+=4
        kos.add(u32(p)) # dialogue tree
    # Also include source-address literals at canonical decoder call boundaries.
    # This catches title/event streams while refusing to guess from unrelated constants.
    src=args.runtime.read_text()
    for pat,target in [
        (r'kos_decode\(\s*(0x[0-9A-Fa-f]+)u?',kos),
        (r'load_kos_vram_auto\(\s*(0x[0-9A-Fa-f]+)u?',kos),
        (r'nemesis_fast\(\s*(0x[0-9A-Fa-f]+)u?',nem),
        (r'load_nem_vram_auto\(\s*(0x[0-9A-Fa-f]+)u?',nem),
    ]:
        for m in re.finditer(pat,src): target.add(int(m.group(1),16))
    # Fixed treasure graphics can be reached through a conditional expression.
    nem.update((0x2975AE,0x2976FA,0x297674))
    decoded=[]
    for codec,addrs,fn in ((0,kos,kosinski_decompress),(1,nem,nemesis_decompress)):
        for a in sorted(addrs):
            if not (0<=a<len(rom)): raise SystemExit(f'bad source {a:06X}')
            try: data=fn(rom[a:])
            except Exception as e: raise SystemExit(f'decode {"kos" if codec==0 else "nem"} {a:06X}: {e}')
            decoded.append((codec,a,data))
    # The source-address catalog and expanded bytes remain identical. Store
    # lossless blocks and exact duplicate payloads once; bit 31 marks packing.
    from field_stream_pack import pack_bank
    blob,entries,packing=pack_bank(decoded)
    args.out_bin.write_bytes(blob)
    args.out_h.write_text('''#ifndef PS4_PSC_FIELD_PREDECODE_H\n#define PS4_PSC_FIELD_PREDECODE_H\n#include <stdint.h>\n#define PS4_PREDECODE_KOS 0u\n#define PS4_PREDECODE_NEM 1u\n#define PS4_PREDECODE_PACKED 2\nextern const uint8_t ps4_psc_field_predecode[];\n/* 0 = absent, 1 = raw, 2 = LZ4 block; length is always expanded bytes. */\nint ps4_psc_predecode_lookup(unsigned codec,uint32_t source,const uint8_t** data,unsigned* length);\n#endif\n''')
    lines=['''/* AUTO-GENERATED: PSC-style host-decoded immutable PSIV streams. */\n#include <stdint.h>\n#include "psc_field_predecode.h"\ntypedef struct { uint32_t source,offset,length; } Ps4PscPredecodeEntry;''']
    for codec,name in ((0,'kos'),(1,'nem')):
        lines.append(f'static const Ps4PscPredecodeEntry {name}_entries[]={{')
        for a,o,n in entries[codec]: lines.append(f'{{0x{a:06X}u,0x{o:08X}u,{n}u}},')
        lines.append('};')
    lines.append('''static int find_entry(const Ps4PscPredecodeEntry*e,unsigned n,uint32_t source,const uint8_t**data,unsigned*length){
 unsigned lo=0u,hi=n;while(lo<hi){unsigned m=lo+((hi-lo)>>1);uint32_t s=e[m].source;if(s<source)lo=m+1u;else hi=m;}
 if(lo>=n||e[lo].source!=source)return 0;uint32_t off=e[lo].offset;*data=ps4_psc_field_predecode+(off&0x7FFFFFFFu);*length=e[lo].length;return (off&0x80000000u)?PS4_PREDECODE_PACKED:1;
}
int ps4_psc_predecode_lookup(unsigned codec,uint32_t source,const uint8_t**data,unsigned*length){
 if(codec==PS4_PREDECODE_KOS)return find_entry(kos_entries,(unsigned)(sizeof(kos_entries)/sizeof(kos_entries[0])),source,data,length);
 if(codec==PS4_PREDECODE_NEM)return find_entry(nem_entries,(unsigned)(sizeof(nem_entries)/sizeof(nem_entries[0])),source,data,length);
 return 0;
}
''')
    args.out_c.write_text('\n'.join(lines))
    args.out_s.write_text('''    .section .rodata.psc_field_predecode,"a",%progbits\n    .balign 4\n    .global ps4_psc_field_predecode\n    .type ps4_psc_field_predecode, %object\nps4_psc_field_predecode:\n    .incbin "gba/psc_field_predecode.bin"\n    .size ps4_psc_field_predecode, .-ps4_psc_field_predecode\n''')
    print(f'PASS PSC field predecode: maps={valid} kos={len(entries[0])} nem={len(entries[1])} packing={packing}')

if __name__=='__main__': main()
