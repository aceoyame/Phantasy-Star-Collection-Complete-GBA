#!/usr/bin/env python3
"""Per-song: how much of each of the 9 source lanes (6 FM + 3 PSG) is carried by some PSC voice (exact pitch, +-1 frame;
and octave-folded), and which PSC voices are unexplained by any source lane."""
import json, sys
sys.argv = [sys.argv[0]] + sys.argv[1:]
exec(open('audit_ps3.py').read().split('res = []')[0])   # reuse helpers only
def two(A): return [(t, m) for t, m, e in A]
res = []
for ssong in range(42):
    r = src3.parse(f'vgm/s{ssong}.vgm')
    P = [two(A) for A in psc_onsets(ssong + 1)]
    lanes = fm_onsets(r) + psg_onsets(r)
    names = [f'FM{i+1}' for i in range(6)] + [f'PSG{i+1}' for i in range(3)]
    row = dict(source_song=ssong, psc_song=ssong+1, frames=r['frames'], lanes=[], voices=[])
    for li, B in enumerate(lanes):
        if not B: row['lanes'].append(dict(lane=names[li], onsets=0)); continue
        ex = max((frac_match(B, A, 1, 1.0) for A in P if A), default=0.0)
        fo = max((frac_match(B, A, 1, 1.0, True) for A in P if A), default=0.0)
        # also allow a constant PSC-vs-source time shift up to +-6 frames
        sh = max((frac_match(B, A, 1, 1.0, False, d) for A in P if A for d in range(-6, 7)), default=0.0)
        row['lanes'].append(dict(lane=names[li], onsets=len(B), exact=round(ex, 3), folded=round(fo, 3), shifted=round(sh, 3)))
    for v, A in enumerate(P):
        if not A: row['voices'].append(dict(voice=v, onsets=0)); continue
        sc = [(frac_match(A, B, 1, 1.0), names[i]) for i, B in enumerate(lanes) if B]
        sf = [(frac_match(A, B, 1, 1.0, True), names[i]) for i, B in enumerate(lanes) if B]
        b = max(sc) if sc else (0, None); bf = max(sf) if sf else (0, None)
        row['voices'].append(dict(voice=v, onsets=len(A), best=b[1], exact=round(b[0], 3), best_folded=bf[1], folded=round(bf[0], 3)))
    row['noise_active'] = sum(1 for e in r['psg'][3] if e['att'] < 15)
    row['dac_writes'] = len(r['dac'])
    res.append(row)
json.dump(res, open('ps3_audit2.json', 'w'), indent=1)
for row in res:
    L = ' '.join(f"{l['lane']}:{l.get('exact',0):.2f}/{l.get('folded',0):.2f}" for l in row['lanes'] if l['onsets'])
    V = ' '.join(f"v{v['voice']}={v.get('best','-')}{v.get('exact',0):.2f}/{v.get('best_folded','-')}{v.get('folded',0):.2f}" for v in row['voices'] if v['onsets'])
    print(f"{row['source_song']:>2} SRC {L}\n   PSC {V}")
