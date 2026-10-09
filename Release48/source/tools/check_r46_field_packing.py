#!/usr/bin/env python3
"""Compare compact streams with Release 08, decoding both banks independently.

liblz4 is the format oracle. ASan/UBSan also exercise the exact production C
expander at every destination alignment. --native adds actual ARM lookup/copy
checks; those direct calls disable IRQ and are separate from live loader tests.
"""
from pathlib import Path
import argparse, ctypes, json, os, re, struct, subprocess, tempfile, zipfile, zlib

R = Path(__file__).resolve().parents[2]
S = R / 'source'
ap = argparse.ArgumentParser()
ap.add_argument('--baseline-zip', type=Path, required=True)
ap.add_argument('--label', default='RELEASE08')
ap.add_argument('--native', action='store_true')
a = ap.parse_args()

def entries(text):
    result = {}
    for codec, name in enumerate(('kos', 'nem')):
        block = text.split(f'{name}_entries[]={{')[1].split('};')[0]
        for src, off, n in re.findall(r'\{(0x[0-9A-Fa-f]+)u,(0x[0-9A-Fa-f]+)u,(\d+)u\}', block):
            result[codec, int(src, 16)] = (int(off, 16), int(n))
    return result

with zipfile.ZipFile(a.baseline_zip) as z:
    old = z.read('R46/source/gba/psc_field_predecode.bin')
    before = entries(z.read('R46/source/gba/psc_field_predecode.c').decode())
    old_visual = z.read('R46/source/gba/precomp_assets.bin')
    old_header = z.read('R46/source/gba/precomp_assets.h').decode()
bank = (S / 'gba/psc_field_predecode.bin').read_bytes()
after = entries((S / 'gba/psc_field_predecode.c').read_text())
assert before.keys() == after.keys() and len(after) == 969
offsets = sorted({off & 0x7fffffff for off, _ in after.values()} | {len(bank)})
extent = dict(zip(offsets, offsets[1:]))
lz = ctypes.CDLL('liblz4.so.1')
decode = lz.LZ4_decompress_safe_partial
decode.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_int]
decode.restype = ctypes.c_int
fixtures = []
for key, (off, n) in after.items():
    oldoff, oldn = before[key]
    assert oldn == n
    if oldoff >> 31:
        oldbase = oldoff & 0x7fffffff
        oldoffsets = sorted({o & 0x7fffffff for o, _ in before.values()} | {len(old)})
        oldend = next(x for x in oldoffsets if x > oldbase)
        olddest = ctypes.create_string_buffer(n)
        assert decode(old[oldbase:oldend], olddest, oldend-oldbase, n, n) == n
        expected = olddest.raw
    else:
        expected = old[oldoff:oldoff+n]
    base = off & 0x7fffffff
    payload = bank[base:extent[base]]
    if off >> 31:
        dest = ctypes.create_string_buffer(n)
        assert decode(payload, dest, len(payload), n, n) == n, key
        assert dest.raw == expected, key
    else:
        assert payload[:n] == expected
    fixtures.append((key, payload, expected, bool(off >> 31)))

visual = (S / 'gba/precomp_assets.bin').read_bytes()
assert visual == old_visual
assert (S / 'gba/precomp_assets.h').read_text() == old_header

runtime = (S / 'gba/title_runtime_v06.c').read_text()
func = runtime.split('static void psc_predecode_expand(')[1].split('\nstatic int psc_predecode_copy(')[0]
func = 'static void psc_predecode_expand(' + func
with tempfile.TemporaryDirectory(prefix='r46-pack-check-') as tmp:
    tmp = Path(tmp)
    data = bytearray()
    for _, payload, expected, packed in fixtures:
        if packed:
            data.extend(struct.pack('<II', len(payload), len(expected)))
            data.extend(payload); data.extend(expected)
    (tmp/'fixtures.bin').write_bytes(data)
    main = r'''
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
FUNCTION
int main(int argc,char**argv){
    FILE*f=fopen(argv[1],"rb");unsigned dims[2],cases=0;
    while(fread(dims,4,2,f)==2){
        unsigned p=dims[0],n=dims[1];uint8_t*in=malloc(p),*expected=malloc(n),*raw=malloc(n+40);
        if(fread(in,1,p,f)!=p||fread(expected,1,n,f)!=n)return 2;
        for(unsigned align=0;align<4;align++){
            memset(raw,0xA5,n+40);uint8_t*out=raw+16+align;
            psc_predecode_expand(in,out,n);
            if(memcmp(out,expected,n))return 3;
            for(unsigned j=0;j<n+40;j++)if((j<16+align||j>=16+align+n)&&raw[j]!=0xA5)return 4;
            ++cases;
        }
        free(in);free(expected);free(raw);
    }
    fclose(f);printf("PASS %u sanitized C expansions\n",cases);return 0;
}
'''.replace('FUNCTION', func)
    (tmp/'check.c').write_text(main)
    subprocess.run(['gcc','-O2','-fno-strict-aliasing','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(tmp/'check.c'),'-o',str(tmp/'check')],check=True)
    # The managed execution host cannot expose /proc thread details to LSan.
    # Address/undefined-behavior instrumentation remains fully enabled.
    subprocess.run([str(tmp/'check'),str(tmp/'fixtures.bin')],check=True,
                   env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})

report = {'baseline':'Release 08 independently decoded packed bank','source_records':len(after),
          'unique_stored_payloads':len(offsets)-1,'expanded_bytes':sum(len(x[2]) for x in fixtures),
          'stored_bytes_before':len(old),'stored_bytes_after':len(bank),
          'independent_liblz4_all_streams_identical':True,'ASan_UBSan_C_expansions':4*sum(x[3] for x in fixtures),
          'removed_unused_visual_bytes':0,'retained_visual_bytes_and_offsets_verified':True}
if a.native:
    import sys
    sys.path.insert(0,str(S/'tools_host/r46_mgba'))
    from mgba import GBA,L,run_until_break
    rom=R/'binaries/RUN_THIS_RELEASE48_PSC.gba';g=GBA(rom)
    symbols={}
    for line in (S/'build-psc-module/ps4_psc_module.map').read_text().splitlines():
        cols=line.split()
        if len(cols)>=5:
            try:symbols[cols[-1]]=int(cols[0],16)
            except ValueError:pass
    dest=0x02010010;copied=0x02033d70
    def call(codec,src,out,cap):
        L.mg_break(0xffffffff);L.mg_cpsr(0xdf)
        for i,v in enumerate((codec,src,out,cap)):L.mg_set_reg(i,v)
        g.wr32(0x03007d80,copied)
        for i,v in ((13,0x03007d80),(14,0x08800000),(15,symbols['psc_predecode_copy'])):L.mg_set_reg(i,v)
        assert run_until_break(g,0x08800000,30),(codec,hex(src),'ARM return')
        return g.reg(0)
    cases=0
    for (codec,src),_,expected,_ in fixtures:
        n=len(expected)
        for align in range(4):
            g.put(dest-16,bytes([0xA5])*(n+40));g.wr32(copied,0xDEADBEEF)
            assert call(codec,src,dest+align,n)==1
            assert g.rd32(copied)==n and g.mem(dest+align,n)==expected,(codec,hex(src),align)
            guard=g.mem(dest-16,n+40)
            assert guard[:16+align]==bytes([0xA5])*(16+align) and guard[16+align+n:]==bytes([0xA5])*(24-align)
            cases+=1
        for cap in (0,n-1):
            g.put(dest,bytes([0x5A])*(n+4));g.wr32(copied,0xDEADBEEF)
            assert call(codec,src,dest,cap)==1 and g.rd32(copied)==0
            assert g.mem(dest,n+4)==bytes([0x5A])*(n+4)
            cases+=1
    for codec,src in ((0,0xFFFFFF),(1,0xFFFFFF),(2,0x100802)):
        g.wr32(copied,0xDEADBEEF)
        assert call(codec,src,dest,16384)==0 and g.rd32(copied)==0xDEADBEEF
        cases+=1
    report.update(native_ARM_copy_and_capacity_cases=cases,native_IRQ_disabled_direct_call_fixture=True,
                  ROM_crc32=f'{zlib.crc32(rom.read_bytes()):08x}')
(R/'build_artifacts'/('R46_'+a.label+'_FIELD_PACKING.json')).write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2),flush=True)
