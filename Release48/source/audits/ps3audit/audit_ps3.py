#!/usr/bin/env python3
"""Map PSC's six native PSIII voices to the original six FM + three PSG (+noise/DAC) lanes, song by song."""
import json, math, sys
from psc3 import *
import src3
FR = int(sys.argv[1]) if len(sys.argv) > 1 else 3600
def midi(f): return 69 + 12*math.log2(f/440.0) if f > 0 else None
def psc_onsets(song):
    out = []
    for ch, p in enumerate(song_start(song)):
        if p == 0xFFFF: out.append([]); continue
        ev = run_channel(p, FR)
        out.append([(e['t'], midi(freq(e)), e) for e in ev if not e['tie'] and e['t'] < FR])
    return out
def fm_onsets(r):
    return [[(n['t'], midi(n['f'])) for n in src3.notes_fm(r['fm'][ch]) if n['f'] > 0 and n['t'] < FR] for ch in range(6)]
def psg_onsets(r):
    out = []
    for ch in range(3):
        st = r['psg_frames'][ch]; ons = []; prev = None; pa = 15
        for f, (att, per) in enumerate(st):
            if f >= FR: break
            if att >= 15 or per == 0: prev = None; pa = att; continue
            m = midi(src3.PSGCLK/(32.0*per))
            if prev is None or abs(m - prev) >= 0.6 or (pa - att >= 4): ons.append((f, m)); 
            prev = m; pa = att
        out.append(ons)
    return out
def frac_match(A, B, tt=1, tp=0.6, fold=False, delay=0):
    """fraction of A onsets with a B onset at t-delay within tt frames and pitch within tp (octave-folded if fold)"""
    if not A: return 0.0
    hit = 0; Bs = {}
    for t, m in B: Bs.setdefault(t, []).append(m)
    for t, m in A:
        ok = False
        for dt in range(-tt, tt+1):
            for mb in Bs.get(t - delay + dt, []):
                d = abs(m - mb)
                if fold: d = min(d % 12, 12 - d % 12)
                if d <= tp: ok = True; break
            if ok: break
        hit += ok
    return hit / len(A)
def best_delay(A, lanes, maxd=36):
    best = (0.0, None, None, None)
    for li, B in enumerate(lanes):
        if not B: continue
        for d in range(0, maxd+1):
            for fold in (False, True):
                s = frac_match(A, B, 1, 0.6, fold, d)
                if s > best[0] + 1e-9: best = (s, li, d, fold)
    return best
res = []
for ssong in range(42):
    psong = ssong + 1
    r = src3.parse(f'vgm/s{ssong}.vgm')
    P = psc_onsets(psong); F = fm_onsets(r); G = psg_onsets(r)
    row = dict(source_song=ssong, psc_song=psong, frames=r['frames'])
    # PSC voice -> FM lane
    mapping = []
    for v, A3 in enumerate(P):
        if not A3: mapping.append(dict(voice=v, onsets=0)); continue
        A = [(t, mm) for t, mm, e in A3]
        sc = [frac_match(A, B, 1, 1.0) for B in F]; bi = max(range(6), key=lambda i: sc[i])
        mapping.append(dict(voice=v, onsets=len(A), best_fm=bi+1, match=round(sc[bi], 3), second=sorted(round(x, 2) for x in sc)[-2]))
    row['voices'] = mapping
    # FM lane coverage by PSC
    cov = []
    for m, B in enumerate(F):
        if not B: cov.append(dict(fm=m+1, onsets=0)); continue
        c = max(frac_match(B, [(t, mm) for t, mm, e in A], 1, 1.0) for A in P if A) if any(P) else 0.0
        cov.append(dict(fm=m+1, onsets=len(B), covered_by_psc=round(c, 3)))
    row['fm_coverage'] = cov
    # PSG lanes: how much is a duplicate of an FM lane (any delay up to 36 frames, octave-folded allowed)?
    pg = []
    for ch, A in enumerate(G):
        if not A: pg.append(dict(psg=ch+1, onsets=0)); continue
        s, li, d, fold = best_delay(A, F)
        pg.append(dict(psg=ch+1, onsets=len(A), dup_of_fm=(li+1 if li is not None else None), delay_frames=d, octave_folded=fold, explained=round(s, 3)))
    row['psg'] = pg
    noise = r['psg'][3]; row['noise_events'] = sum(1 for e in noise if e['att'] < 15)
    row['dac_write_frames'] = len(r['dac'])
    res.append(row)
json.dump(res, open('ps3_audit.json', 'w'), indent=1)
print('song  voices(best FM:match)                          unmatched-FM(onsets)      PSG onsets/explained(FMlane,delay)       noise dacF')
for row in res:
    vs = ' '.join(f"{v['voice']}:{v.get('best_fm','-')}/{v.get('match',0):.2f}" if v['onsets'] else f"{v['voice']}:-" for v in row['voices'])
    um = ' '.join(f"F{c['fm']}({c['onsets']})" for c in row['fm_coverage'] if c['onsets'] and c['covered_by_psc'] < 0.9)
    pg = ' '.join((f"G{p['psg']}:{p['onsets']}/{p['explained']:.2f}(F{p['dup_of_fm']},d{p['delay_frames']})" if p['onsets'] else f"G{p['psg']}:-") for p in row['psg'])
    print(f"{row['source_song']:>3} {vs:<44} {um:<26} {pg:<60} {row['noise_events']:>4} {row['dac_write_frames']:>4}")
