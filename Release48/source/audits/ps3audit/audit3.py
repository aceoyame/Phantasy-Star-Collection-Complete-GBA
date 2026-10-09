#!/usr/bin/env python3
"""Classify every source lane (6 FM + 3 PSG) per song against PSC's six native voices.
Metrics (all +-1 frame, onset based):
  timing  : fraction of lane onsets with ANY PSC-voice onset (any pitch) at same time  -> is the rhythm there?
  pitch0  : same but pitch within 0.6 st (exact)
  pitchO  : pitch within 0.6 st modulo octave
For PSG lanes also: best PSC voice at delay 0..40 frames (octave-folded) = echo/doubling already present in PSC."""
import json, sys
exec(open('audit_ps3.py').read().split('res = []')[0])
def two(A): return [(t, m) for t, m, e in A]
def timing(B, A, tt=1, delay=0):
    if not B: return 0.0
    ts = set(t for t, m in A); hit = 0
    for t, m in B:
        if any((t - delay + dt) in ts for dt in range(-tt, tt+1)): hit += 1
    return hit/len(B)
out = []
names = [f'FM{i+1}' for i in range(6)] + [f'PSG{i+1}' for i in range(3)]
for ssong in range(42):
    r = src3.parse(f'vgm/s{ssong}.vgm')
    P = [two(A) for A in psc_onsets(ssong+1)]
    allP = [x for A in P for x in A]
    lanes = fm_onsets(r) + psg_onsets(r)
    row = dict(source_song=ssong, psc_song=ssong+1, lanes=[])
    for li, B in enumerate(lanes):
        if not B: continue
        tm_any = timing(B, allP)
        best_tm = max(((timing(B, A), v) for v, A in enumerate(P) if A), default=(0, None))
        p0 = max(((frac_match(B, A, 1, 0.6), v) for v, A in enumerate(P) if A), default=(0, None))
        po = max(((frac_match(B, A, 1, 0.6, True), v) for v, A in enumerate(P) if A), default=(0, None))
        # best (voice, delay, fold) over delays 0..40
        bd = (0, None, None, None)
        for v, A in enumerate(P):
            if not A: continue
            for d in range(-6, 41):
                for fold in (False, True):
                    s = frac_match(B, A, 1, 0.6, fold, d)
                    if s > bd[0] + 1e-9: bd = (s, v, d, fold)
        row['lanes'].append(dict(lane=names[li], onsets=len(B), timing_any=round(tm_any, 2), timing_best=round(best_tm[0], 2), timing_voice=best_tm[1],
                                 exact=round(p0[0], 2), exact_voice=p0[1], oct=round(po[0], 2), oct_voice=po[1],
                                 best_delay=dict(match=round(bd[0], 2), voice=bd[1], delay=bd[2], octave_fold=bd[3])))
    out.append(row)
json.dump(out, open('ps3_audit3.json', 'w'), indent=1)
for row in out:
    print(f"song {row['source_song']:>2}:")
    for l in row['lanes']:
        bd = l['best_delay']
        print(f"   {l['lane']:<5} n={l['onsets']:>3} timingAny={l['timing_any']:.2f} timingBest={l['timing_best']:.2f}(v{l['timing_voice']}) exact={l['exact']:.2f}(v{l['exact_voice']}) oct={l['oct']:.2f}(v{l['oct_voice']})  best-delay: {bd['match']:.2f} v{bd['voice']} d{bd['delay']} fold={bd['octave_fold']}")
