import sys, pickle, time, os
from mdrec import *
from clipx import extract
ks=[int(x) for x in sys.argv[1:]] or list(range(1,15))
st='/tmp/claude_party_base.bin.0'
# Purifying Light instantly destroys the zombie test formation (actor changes mid-effect):
# record it against a non-dark formation instead.
FORM={12:bytes([4,0x0f,0,0x28,0x08,0x28,0x10,0x28,0x18,0x28,0x20,0xff])}
SKIP={1,13}   # Paradinblw, Lethal Img: the stand-in caster setup corrupts VRAM / Battle_Palette_Objects (recorder limitation); the port keeps its R45 path
for k in ks:
    if k in SKIP: print('combo',k,'skipped (recorder limitation)',flush=True); continue
    t0=time.time()
    if k in FORM:
        m=setup_party_battle(form=FORM[k]); m.save('/tmp/claude_party_base_c%d.bin'%k)
    else:
        m=MD(); m.load(st)
    L.md_patch_rom(0x9B94,0x4E); L.md_patch_rom(0x9B95,0x71); L.md_patch_rom(0x9BA6,0x60)  # recorder-only skill-use patch (see batch_party)
    r=record_party_combo(k,m)
    if r is None: print('combo',k,'not triggered',flush=True); continue
    blocks={}; clip=extract(r,blocks)
    prev=[fr['img'] for fr in r['rec'][:clip['ticks'] if clip else 60:16]]
    pickle.dump(dict(kind='combo',a=k,b=0,clip=clip,blocks={v[0]:v for v in blocks.values()},fighters=r['fighters'],preview=prev,nframes=len(r['rec'])),open('/home/claude/w/clips/c_%02x.pkl'%k,'wb'))
    print('combo',k,[hex(x) for x in r['part']],'ticks',clip['ticks'] if clip else None,'tracks',len(clip['tracks']) if clip else 0,'rec',len(r['rec']),'%.1fs'%(time.time()-t0),flush=True)
