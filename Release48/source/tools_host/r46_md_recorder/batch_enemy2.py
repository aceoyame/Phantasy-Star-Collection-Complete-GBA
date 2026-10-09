"""R46 pass 2: every (enemy, ability) pair in the ROM's EnemyData tables that the
spec pass did not cover (normal attacks = ability 0), boss opening actions
($FFEE87 intros), Zio's scripted $FFEE98 stage sequence, and the Ripper/Piercer
pair moves that need both enemies in the formation."""
import sys, pickle, json, time, os
from mdrec import *
from clipx import extract
OUT='/home/claude/w/clips'
spec=json.load(open('/home/claude/w/R45/source/tools/source_enemy_effect_specs.json'))['pairs']
sp=set((x['enemy'],x['ability']) for x in spec)
B=0x2816BC
rom=set()
for e in range(0x99):                       # EnemyAttackOffs covers $00-$98
    p=B+e*48
    for a in set(ROMB[p+36:p+44])|set(x for x in ROMB[p+32:p+36] if x): rom.add((e,a))
ZIO={0x8b:3,0x8c:1,0x98:5}                   # stages to record (EnemyAttack_Zio/Zio2/Zio3)
PAIRFORM={}   # Ripper $3A / Piercer $3B fuse into Twin Arms: palette-only, lowered in C (battle_twinarms_fusion)
PAIRMOVE=set()
part=int(sys.argv[1]); nparts=int(sys.argv[2])
only=[int(x,16) for x in sys.argv[3:]]
eids=sorted(set(e for e,a in rom)|set(ZIO))
mine=[e for i,e in enumerate(eids) if i%nparts==part and (not only or e in only)]
def save(fn,kind,e,a,r,prio):
    blocks={}; clip=extract(r,blocks)
    prev=[fr['img'] for fr in r['rec'][:clip['ticks'] if clip else 60:16]]
    pickle.dump(dict(kind=kind,eid=e,ability=a,prio=prio,clip=clip,blocks={v[0]:v for v in blocks.values()},fighters=r['fighters'],enemy_slot=r['enemy_slot'],preview=prev,cram0=r['rec'][0]['cram']),open(os.path.join(OUT,fn),'wb'))
    return clip
def result_ability(r):
    a4=r['a4']&0xFFFF; ram=r['rec'][0]['ram']; return (ram[a4+0x24]<<8)|ram[a4+0x25]
for e in mine:
    t0=time.time()
    form=PAIRFORM.get(e)
    try:
        r,m=record_enemy_intro(e,form=form)
    except AssertionError:
        print('setup fail',hex(e),flush=True); continue
    if r is not None:
        if e in ZIO:
            a=result_ability(r); c=save('z%02x_s0.pkl'%e,'enemy',e,a,r,2)
            print(hex(e),'zio stage0 ability',hex(a),'ticks',c and c['ticks'],flush=True)
            for s in range(1,ZIO[e]):
                r,m=record_enemy_ability(e,None,frames=420,m=m)
                if r is None: print(hex(e),'zio stage',s,'no turn',flush=True); break
                a=result_ability(r); c=save('z%02x_s%d.pkl'%(e,s),'enemy',e,a,r,2)
                print(hex(e),'zio stage',s,'ability',hex(a),'ticks',c and c['ticks'],flush=True)
        else:
            c=save('i%02x.pkl'%e,'enemy_intro',e,0,r,1)
            print(hex(e),'intro ee87',r['ee87'],'ticks',c and c['ticks'],'tracks',len(c['tracks']) if c else 0,flush=True)
    st='/tmp/claude_base2_%02x.bin'%e; m.save(st)
    todo=sorted(a for (ee,a) in rom if ee==e and e not in (0x8b,0x98))   # existing e-files from the spec pass are skipped below
    for a in todo:
        out='%s%02x_%02x.pkl'%('p' if (e,a) in PAIRMOVE else 'e',e,a)
        if os.path.exists(os.path.join(OUT,out)) and out[0]=='e': continue
        m=MD(); m.load(st)
        r,m=record_enemy_ability(e,a,frames=420,m=m)
        if r is None: print('no turn',hex(e),hex(a),flush=True); continue
        c=save(out,'enemy',e,a,r,1 if out[0]=='p' else 0)
        print(hex(e),hex(a),'ticks',c and c['ticks'],'tracks',len(c['tracks']) if c else 0,'%.1fs'%(time.time()-t0),flush=True)
