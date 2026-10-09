import sys, pickle, json, time, os
from mdrec import *
from clipx import extract
rom=ROMB
spec=json.load(open('/home/claude/w/R45/source/tools/source_enemy_effect_specs.json'))['pairs']
pairs=sorted(set((x['enemy'],x['ability']) for x in spec))
part=int(sys.argv[1]); nparts=int(sys.argv[2])
eids=sorted(set(e for e,a in pairs))
mine=[e for i,e in enumerate(eids) if i%nparts==part]
os.makedirs('/home/claude/w/clips',exist_ok=True)
for e in mine:
    t0=time.time()
    try:
        base=setup_enemy_battle(e)
        st='/tmp/claude_base_%02x.bin'%e; base.save(st)
    except AssertionError as ex:
        print('setup fail',hex(e),flush=True); continue
    for (ee,a) in pairs:
        if ee!=e: continue
        out='/home/claude/w/clips/e%02x_%02x.pkl'%(e,a)
        if os.path.exists(out): continue
        m=MD(); m.load(st)
        r,m=record_enemy_ability(e,a,frames=420,m=m)
        if r is None: print('no turn',hex(e),hex(a),flush=True); continue
        blocks={}
        clip=extract(r,blocks)
        # keep compact: drop images/vram/ram; keep first frame image + every 4th for previews
        prev=[fr['img'] for fr in r['rec'][:clip['ticks'] if clip else 60:16]]
        pickle.dump(dict(eid=e,ability=a,clip=clip,blocks={v[0]:v for v in blocks.values()},fighters=r['fighters'],enemy_slot=r['enemy_slot'],preview=prev,cram0=r['rec'][0]['cram']),open(out,'wb'))
        print(hex(e),hex(a),'ticks',clip['ticks'] if clip else None,'tracks',len(clip['tracks']) if clip else 0,'%.1fs'%(time.time()-t0),flush=True)
