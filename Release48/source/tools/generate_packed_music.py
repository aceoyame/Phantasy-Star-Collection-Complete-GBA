#!/usr/bin/env python3
from __future__ import annotations
from pathlib import Path
import argparse, json, re
import numpy as np
ROOT=Path(__file__).resolve().parents[1]
PCM=ROOT/'gba'/'pcm_v18'
OUT=ROOT/'gba'/'pcm_packed'
BLOCK=528
IDS=(0x84,0x87,0x8A,0x8B,0x8C,0x8F,0x95,0x9F,0xAC)
ONE_SHOT={0x8B}
def parse_loops(path):
    t=path.read_text();out={}
    for m in re.finditer(r'#define\s+PS4_PCM_TRACK_([0-9A-F]{2})_LOOP_OFFSET\s+(\d+)u',t):
        out[int(m.group(1),16)]=int(m.group(2))
    return out
def residuals(data):
    s=np.frombuffer(data,dtype=np.int8).astype(np.int16)
    return (((s[1:]-s[:-1]+128)&255)-128).astype(np.int16)
def choose_width(r):
    best=None;n=int(r.size)
    for w in range(3,8):
        lim=(1<<(w-1))-1;esc=(r<-lim)|(r>lim)
        bits=n*w+int(np.count_nonzero(esc))*8
        if best is None or bits<best[0]:best=(bits,w,int(np.count_nonzero(esc)))
    return best
def pack_track(data,w,loop_sample):
    r=residuals(data);n=int(r.size);lim=(1<<(w-1))-1;esc_code=(1<<w)-1
    esc=(r<-lim)|(r>lim);codes=np.where(esc,esc_code,r+lim).astype(np.uint16)
    lens=np.full(n,w,dtype=np.uint32);lens[esc]+=8
    starts=np.empty(n,dtype=np.uint64)
    if n:
        starts[0]=0
        if n>1: starts[1:]=np.cumsum(lens[:-1],dtype=np.uint64)
        total_bits=int(starts[-1]+lens[-1])
    else: total_bits=0
    bits=np.zeros(total_bits,dtype=np.uint8)
    for b in range(w): bits[starts+b]=((codes>>b)&1).astype(np.uint8)
    ei=np.flatnonzero(esc)
    if ei.size:
        raw=(r[ei].astype(np.int16)&255).astype(np.uint16);es=starts[ei]+w
        for b in range(8):bits[es+b]=((raw>>b)&1).astype(np.uint8)
    packed=np.packbits(bits,bitorder='little').tobytes()+b'\0\0\0\0'
    loop_bit=int(starts[loop_sample]) if loop_sample<n else total_bits
    return packed,total_bits,loop_bit,int(np.count_nonzero(esc))
def decode_track(blob,length,w,seed,loop_sample,loop_bit,loop_seed):
    bit=0;cur=seed&255;out=bytearray(length);esc=(1<<w)-1;lim=(1<<(w-1))-1
    def get(n):
        nonlocal bit
        p=bit>>3; sh=bit&7
        v=blob[p]|(blob[p+1]<<8)|(blob[p+2]<<16)
        bit+=n
        return (v>>sh)&((1<<n)-1)
    for i in range(length):
        out[i]=cur
        if i+1<length:
            c=get(w)
            if c==esc:
                q=get(8);d=q-256 if q&128 else q
            else:d=c-lim
            cur=(cur+d)&255
    return bytes(out)
def main():
    ap=argparse.ArgumentParser();ap.add_argument('--verify-only',action='store_true');a=ap.parse_args()
    loops=parse_loops(PCM/'ps4_pcm_loop_offsets.h');OUT.mkdir(parents=True,exist_ok=True);rows=[]
    for mid in IDS:
        p=PCM/f'track_{mid:02X}.pcm'
        if not p.exists():raise SystemExit(f'missing {p}')
        data=p.read_bytes()
        if len(data)%BLOCK:raise SystemExit(f'{p.name}: length not {BLOCK}-sample aligned')
        loop=0 if mid in ONE_SHOT else loops.get(mid)
        if loop is None:raise SystemExit(f'{p.name}: missing loop offset')
        if loop%BLOCK:raise SystemExit(f'{p.name}: loop not frame aligned')
        r=residuals(data);bits,w,esc=choose_width(r)
        blob,total_bits,loop_bit,esc2=pack_track(data,w,loop);assert esc==esc2
        seed=data[0];loop_seed=data[loop] if loop<len(data) else seed
        dec=decode_track(blob,len(data),w,seed,loop,loop_bit,loop_seed)
        if dec!=data:raise SystemExit(f'{p.name}: roundtrip mismatch')
        if not a.verify_only:(OUT/f'track_{mid:02X}.pdm').write_bytes(blob)
        rows.append(dict(id=mid,length=len(data),loop_sample=loop,width=w,seed=seed,loop_seed=loop_seed,loop_bit=loop_bit,bits=total_bits,stored=len(blob),escapes=esc,one_shot=mid in ONE_SHOT))
    if a.verify_only:print('PASS packed music roundtrip',len(rows),'tracks');return
    h=['#ifndef PS4_PCM_PACKED_H','#define PS4_PCM_PACKED_H','']
    asm=['    .section .rodata.ps4_pcm,"a",%progbits','    .balign 4']
    for x in rows:
        tag=f'{x["id"]:02X}'
        h += [f'#define PS4_PCM_TRACK_{tag}_LENGTH {x["length"]}u',f'#define PS4_PCM_TRACK_{tag}_LOOP_SAMPLE {x["loop_sample"]}u',f'#define PS4_PCM_TRACK_{tag}_LOOP_BIT {x["loop_bit"]}u',f'#define PS4_PCM_TRACK_{tag}_WIDTH {x["width"]}u',f'#define PS4_PCM_TRACK_{tag}_SEED {x["seed"]}u',f'#define PS4_PCM_TRACK_{tag}_LOOP_SEED {x["loop_seed"]}u',f'#define PS4_PCM_TRACK_{tag}_ONE_SHOT {1 if x["one_shot"] else 0}u','']
        asm += [f'    .global ps4_pcm_track_{tag}_start, ps4_pcm_track_{tag}_end',f'ps4_pcm_track_{tag}_start:',f'    .incbin "gba/pcm_packed/track_{tag}.pdm"',f'ps4_pcm_track_{tag}_end:','    .balign 4']
    h += ['#endif','']
    (OUT/'ps4_pcm_packed.h').write_text('\n'.join(h))
    (ROOT/'gba'/'pcm_tracks.s').write_text('\n'.join(asm)+'\n')
    (OUT/'manifest.json').write_text(json.dumps({'format':'delta-fixed-width-escape-v1','bit_order':'lsb-first','tracks':rows},indent=2)+'\n')
    raw=sum(x['length'] for x in rows);stored=sum(x['stored'] for x in rows)
    print(f'PASS packed music: tracks={len(rows)} raw={raw} stored={stored} saved={raw-stored}')
    for x in rows:print(f'  ${x["id"]:02X}: raw={x["length"]} stored={x["stored"]} w={x["width"]} loop={x["loop_sample"]} loop_bit={x["loop_bit"]}')
if __name__=='__main__':main()
