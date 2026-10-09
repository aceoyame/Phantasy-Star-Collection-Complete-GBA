import sys, pickle, time, os
from mdrec import *
from clipx import extract
part=int(sys.argv[1]); nparts=int(sys.argv[2])
jobs=[]
for t in range(1,39): jobs.append(('tech',t))
for s_ in range(1,55): jobs.append(('skill',s_))
def target_for(kind,i):
    base=0x2A9BE8 if kind=='tech' else 0x2A9D28
    r=ROMB[base+(i-1)*8+2]&15
    if r in (1,2): return 6
    if r in (4,5): return 3
    return 1
os.makedirs('/home/claude/w/clips',exist_ok=True)
base_st='/tmp/claude_party_base.bin'
if True:
    m=setup_party_battle(); m.save(base_st+'.%d'%part); st=base_st+'.%d'%part
else: st=base_st
for j,(kind,i) in enumerate(jobs):
    if j%nparts!=part: continue
    out='/home/claude/w/clips/%s_%02x.pkl'%(kind[0],i)
    if os.path.exists(out): continue
    t0=time.time()
    m=MD(); m.load(st)
    L.md_patch_rom(0x9B94,0x4E); L.md_patch_rom(0x9B95,0x71); L.md_patch_rom(0x9BA6,0x60)  # recorder-only: let the stand-in caster use any skill
    # high HP so targets survive the effect; party TP max
    for e in range(4): m.w16(0x4200+e*0x80+0x10,9999) if False else None
    r=record_party_ability(kind,i,0,target_for(kind,i),m,frames=700)
    if r is None: print('no exec',kind,i,flush=True); continue
    blocks={}
    clip=extract(r,blocks)
    prev=[fr['img'] for fr in r['rec'][:clip['ticks'] if clip else 60:16]]
    pickle.dump(dict(kind=kind,a=i,b=0,clip=clip,blocks={v[0]:v for v in blocks.values()},fighters=r['fighters'],preview=prev,nframes=len(r['rec'])),open(out,'wb'))
    print(kind,i,'ticks',clip['ticks'] if clip else None,'tracks',len(clip['tracks']) if clip else 0,'rec',len(r['rec']),'%.1fs'%(time.time()-t0),flush=True)
