#!/usr/bin/env python3
"""Independent re-measurement of PSC PSII native program level vs the original YM2612 patch (ymfm render)."""
import struct, subprocess, json, sys, math
import numpy as np
SRC = open('/home/claude/work/ref/ps2/ps2disassm/ps2original.bin','rb').read()
ROM = open(sys.argv[1],'rb').read()
TABLE=0x481510; LOOKUP_PTR=0x3953EC; INS=0xB8568; RATE=21024; NAT=7670453/144
off=struct.unpack_from('<I',ROM,LOOKUP_PTR)[0]-0x08000000
raw=[struct.unpack_from('<i',ROM,off+i*4)[0] for i in range(256)]
DEC=np.array([raw[(q+128)&255] for q in range(256)],dtype=np.float64)
def psc_wave(p):
    cnt,ptr=struct.unpack_from('<II',ROM,TABLE+8*p)
    if not(0<cnt<0x80000000): return None
    o=ptr-0x08000000; return DEC[np.frombuffer(ROM[o:o+cnt],dtype=np.uint8)]
def patch(p):
    o=0xB8000+struct.unpack_from('<H',SRC,INS+2*(p-0x40))[0]; return SRC[o:o+26]
def render(pt,fnum,blk,secs):
    n=int(NAT*secs); out=subprocess.run(['./render',pt.hex(),str(fnum),str(blk),str(n),str(n+1)],capture_output=True).stdout
    x=np.frombuffer(out,dtype=np.int16).astype(np.float64)
    # resample to 21024 by linear interpolation
    t=np.arange(int(len(x)*RATE/NAT))*NAT/RATE
    return np.interp(t,np.arange(len(x)),x)
def pitch(x):
    seg=x[200:200+4096]-np.mean(x[200:200+4096])
    if len(seg)<1024 or not np.any(seg): return 0
    ac=np.correlate(seg,seg,'full')[len(seg)-1:]
    lo=int(RATE/2000); hi=int(RATE/40)
    k=lo+int(np.argmax(ac[lo:hi])); return RATE/k
def rms(v): return float(np.sqrt(np.mean(v*v))) if len(v) else 0.0
rows=[]
progs=[int(a,0) for a in sys.argv[2:]] or list(range(0x40,0x69))
for p in progs:
    w=psc_wave(p)
    if w is None: continue
    secs=min(len(w)/RATE,2.4)
    fp=pitch(w)
    best=None
    for blk in range(2,7):
        s=render(patch(p),1584,blk,secs+0.05)
        fs=pitch(s)
        if fs and fp and abs(math.log2(fs/fp))<0.08: best=(blk,s,fs); break
    if best is None:
        blk=4; s=render(patch(p),1584,blk,secs+0.05); best=(blk,s,pitch(s))
    blk,s,fs=best
    n=min(len(w),len(s))
    # align onset by cross-correlation of envelopes over first 4096 samples
    a=w[:n]; b=s[:n]
    m=min(n,8192); xc=np.correlate(np.abs(a[:m]),np.abs(b[:m]),'full'); lag=int(np.argmax(xc))-(m-1)
    lag=max(-400,min(400,lag))
    if lag>0: a2,b2=a[lag:],b[:n-lag]
    else: a2,b2=a[:n+lag],b[-lag:n]
    L=min(len(a2),len(b2)); a2=a2[:L]; b2=b2[:L]
    head=int(0.12*RATE)
    g_head=20*math.log10(max(rms(a2[:head]),1e-9)/max(rms(b2[:head]),1e-9))
    g_full=20*math.log10(max(rms(a2),1e-9)/max(rms(b2),1e-9))
    # least squares scale on full aligned window
    k=float(np.dot(a2,b2)/max(np.dot(b2,b2),1e-9)); corr=float(np.dot(a2,b2)/max(math.sqrt(np.dot(a2,a2)*np.dot(b2,b2)),1e-9))
    rows.append(dict(program=f'0x{p:02X}',psc_hz=round(fp,1),src_hz=round(fs,1),block=blk,lag=lag,
        psc_rms=round(rms(a2),1),src_rms=round(rms(b2),1),src_head_rms=round(rms(b2[:head]),1),
        gain_head_db=round(g_head,2),gain_full_db=round(g_full,2),
        lsq_gain_db=round(20*math.log10(abs(k)) if k else -99,2),corr=round(corr,3)))
    print(rows[-1],flush=True)
json.dump(rows,open('measure_out.json','w'),indent=1)
