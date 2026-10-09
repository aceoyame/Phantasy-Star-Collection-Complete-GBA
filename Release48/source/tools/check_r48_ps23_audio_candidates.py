#!/usr/bin/env python3
"""mGBA smoke/PCM metrics for Release 48 PSII/PSIII audio candidates."""
from pathlib import Path
import argparse,sys,struct,json,math,tempfile
R=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(R/'source/tools_host/r46_mgba'))
from mgba import GBA,L
ap=argparse.ArgumentParser(); ap.add_argument('roms',nargs='+',help='tag=path'); ap.add_argument('--frames',type=int,default=600); ap.add_argument('--out',type=Path); a=ap.parse_args()
ROMS={}
for spec in a.roms:
    tag,path=spec.split('=',1); ROMS[tag]=Path(path)

def launch(rom,slot):
    g=GBA(rom,entry=0x08000000);g.run(120)
    L.mg_break(0x080005f0);g.run(10,'START');g.run(900);assert L.mg_hit();L.mg_break(0xffffffff);g.run(40)
    for _ in range(slot):g.press('DOWN',1,40)
    entry=[0x08738000,0x08390000,0x08080000,0x08041800][slot]
    L.mg_break(entry);g.run(10,'A');g.run(900);assert L.mg_hit();L.mg_break(0xffffffff);g.run(300)
    return g

def stats(path):
    raw=Path(path).read_bytes();v=struct.unpack('<'+str(len(raw)//2)+'h',raw);m=[(v[i]+v[i+1])//2 for i in range(0,len(v),2)]
    return dict(stereo_frames=len(m),rms=math.sqrt(sum(x*x for x in m)/len(m)) if m else 0,
                delta_rms=math.sqrt(sum((m[i]-m[i-1])**2 for i in range(1,len(m)))/max(1,len(m)-1)) if m else 0,
                zero_crossings=sum((m[i-1]<0)!=(m[i]<0) for i in range(1,len(m))),peak=max(map(abs,m),default=0),nonzero=sum(x!=0 for x in m),
                near_clip=sum(abs(x)>=12000 for x in m))
report={}
with tempfile.TemporaryDirectory() as td:
    td=Path(td)
    for tag,rom in ROMS.items():
        report[tag]={}
        for slot,name in [(1,'II'),(2,'III')]:
            g=launch(rom,slot);p=td/f'{tag}_{name}.pcm';g.pcm(p);before=g.frame();g.run(a.frames);after=g.frame();g.pcm('')
            s=stats(p);s.update(frame_before=before,frame_after=after,advanced=after-before,pc=hex(g.reg(15)));assert after-before==a.frames and s['nonzero']>0
            report[tag][name]=s
if 'base' in report:
    for tag in list(report):
        if tag=='base': continue
        report['delta_'+tag]={}
        for name in ('II','III'):
            report['delta_'+tag][name]={k:report[tag][name][k]-report['base'][name][k] for k in ('rms','delta_rms','zero_crossings','peak','nonzero','near_clip')}
if a.out:
    a.out.parent.mkdir(parents=True,exist_ok=True);a.out.write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
