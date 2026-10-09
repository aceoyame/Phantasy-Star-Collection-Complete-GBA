#!/usr/bin/env python3
"""R25: densify the existing exact-YM FM multisample anchors to per-semitone zones.

The R23 bank contains source-rendered YM2612 anchors, usually spaced by 12 semitones.
This tool preserves every anchor byte-for-byte and fills notes between anchors offline.
For intermediate notes it linearly interpolates each snapshot's harmonic magnitudes in
phase-normalized Fourier space, retaining the phase of the nearer exact anchor.
Runtime cost stays the same; only ROM data grows.
"""
from __future__ import annotations
import argparse, re, math
from pathlib import Path
from collections import defaultdict
import numpy as np

ZONE_RE = re.compile(r'\{\s*0x([0-9A-Fa-f]+)u,\s*\{\s*([^}]*)\},\s*(\d+),\s*(\d+),\s*\{\s*([^}]*)\}\s*\}')


def parse(path: Path):
    txt = path.read_text()
    sr = int(re.search(r'generated_fm_wave_sample_rate\s*=\s*(\d+)u', txt).group(1))
    pts = int(re.search(r'generated_fm_wave_points\s*=\s*(\d+)u', txt).group(1))
    m = re.search(r'const signed char generated_fm_wave_blob\[\]\s*=\s*\{(.*?)\};\s*\n\s*constexpr unsigned int generated_fm_wave_zone_count', txt, re.S)
    blob = [int(x) for x in re.findall(r'-?\d+', m.group(1))]
    zones=[]
    for z in ZONE_RE.finditer(txt):
        bank=int(z.group(1),16)
        offs=[int(x.strip().rstrip('u')) for x in z.group(2).split(',')]
        inst=int(z.group(3)); note=int(z.group(4))
        gains=[int(x.strip()) for x in z.group(5).split(',')]
        zones.append(dict(bank=bank,inst=inst,note=note,offs=offs,gains=gains))
    return sr,pts,blob,zones


def harmonic_interp(a: np.ndarray, b: np.ndarray, t: float):
    A=np.fft.rfft(a.astype(np.float64))
    B=np.fft.rfft(b.astype(np.float64))
    mag=(1.0-t)*np.abs(A)+t*np.abs(B)
    phase=np.angle(A if t < 0.5 else B)
    # DC sign/phase is not musically meaningful here; interpolating the actual DC
    # prevents a midpoint phase selection from flipping the offset.
    C=mag*np.exp(1j*phase)
    C[0]=(1.0-t)*A[0]+t*B[0]
    w=np.fft.irfft(C,n=len(a)).real
    return w


def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('input',type=Path)
    ap.add_argument('output',type=Path)
    args=ap.parse_args()
    sr,pts,blob,zones=parse(args.input)
    groups=defaultdict(list)
    for z in zones: groups[(z['bank'],z['inst'])].append(z)
    for v in groups.values(): v.sort(key=lambda z:z['note'])

    out_blob=[]
    wave_cache={}
    def add_wave(vals):
        q=tuple(max(-128,min(127,int(round(v)))) for v in vals)
        if q in wave_cache: return wave_cache[q]
        off=len(out_blob); out_blob.extend(q); wave_cache[q]=off; return off

    # Seed exact anchors first, preserving bytes exactly and allowing dedupe.
    anchor_wave={}
    anchor_offset={}
    for z in zones:
        for st,off in enumerate(z['offs']):
            arr=tuple(blob[off:off+pts])
            k=(z['bank'],z['inst'],z['note'],st)
            anchor_wave[k]=arr
            anchor_offset[k]=add_wave(arr)

    dense=[]
    interpolated=0
    exact=0
    for key in sorted(groups):
        gs=groups[key]
        if len(gs)==1:
            z=gs[0]
            q=z.copy(); q['offs']=[anchor_offset[(z['bank'],z['inst'],z['note'],st)] for st in range(6)]
            dense.append(q); exact+=1
            continue
        bynote={z['note']:z for z in gs}
        lo,hi=gs[0]['note'],gs[-1]['note']
        for note in range(lo,hi+1):
            if note in bynote:
                z=bynote[note]
                q=z.copy(); q['offs']=[anchor_offset[(z['bank'],z['inst'],z['note'],st)] for st in range(6)]
                dense.append(q); exact+=1
                continue
            lower=max((z for z in gs if z['note']<note), key=lambda z:z['note'])
            upper=min((z for z in gs if z['note']>note), key=lambda z:z['note'])
            span=upper['note']-lower['note']; t=(note-lower['note'])/span
            offs=[]; gains=[]
            for st in range(6):
                wa=np.array(anchor_wave[(key[0],key[1],lower['note'],st)],dtype=float)
                wb=np.array(anchor_wave[(key[0],key[1],upper['note'],st)],dtype=float)
                w=harmonic_interp(wa,wb,t)
                g=(1.0-t)*lower['gains'][st]+t*upper['gains'][st]
                peak=float(np.max(np.abs(w))) if len(w) else 0.0
                # Keep the signed-char waveform comfortably below clipping and
                # compensate in the per-stage gain to preserve level.
                if peak>118.0:
                    scale=118.0/peak
                    w*=scale
                    g/=scale
                offs.append(add_wave(w))
                gains.append(max(1,min(255,int(round(g)))))
            dense.append(dict(bank=key[0],inst=key[1],note=note,offs=offs,gains=gains)); interpolated+=1

    dense.sort(key=lambda z:(z['bank'],z['inst'],z['note']))
    out=[]
    out.append('// R25 dense exact-YM-anchor FM multisample bank.\n')
    out.append('// Exact R23 anchor snapshots are preserved byte-for-byte; intermediate semitones\n')
    out.append('// are synthesized offline by harmonic-magnitude interpolation between anchors.\n')
    out.append(f'constexpr unsigned int generated_fm_wave_sample_rate = {sr}u;\n')
    out.append(f'constexpr unsigned int generated_fm_wave_points = {pts}u;\n')
    out.append(f'constexpr unsigned int generated_fm_wave_blob_size = {len(out_blob)}u;\n')
    out.append('const signed char generated_fm_wave_blob[] = {\n')
    for i in range(0,len(out_blob),20):
        out.append('    '+', '.join(str(x) for x in out_blob[i:i+20])+',\n')
    out.append('};\n\n')
    out.append(f'constexpr unsigned int generated_fm_wave_zone_count = {len(dense)}u;\n')
    out.append('const fm_wave_zone generated_fm_wave_zones[] = {\n')
    for z in dense:
        offs=', '.join(f'{o}u' for o in z['offs'])
        gains=', '.join(str(g) for g in z['gains'])
        out.append(f"    {{ 0x{z['bank']:06X}u, {{ {offs} }}, {z['inst']}, {z['note']}, {{ {gains} }} }},\n")
    out.append('};\n')
    args.output.write_text(''.join(out))
    print(f'anchors={len(zones)} groups={len(groups)} dense_zones={len(dense)} interpolated={interpolated}')
    print(f'blob {len(blob)} -> {len(out_blob)} bytes (+{len(out_blob)-len(blob)})')

if __name__=='__main__': main()
