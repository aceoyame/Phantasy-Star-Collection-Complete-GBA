import json, collections
A = json.load(open('ps3_audit3.json')); B = json.load(open('ps3_audit2.json'))
nz = {r['source_song']: (r['noise_active'], r['dac_writes']) for r in B}
summ = []; cnt = collections.Counter(); rows = []
for r in A:
    s = r['source_song']; fm = {}; psg = {}
    for l in r['lanes']:
        bd = l['best_delay']
        if l['lane'].startswith('FM'):
            c = 'PRESENT' if l['timing_best'] >= 0.95 and l['oct'] >= 0.9 else ('PRESENT_TIMING_ONLY' if l['timing_best'] >= 0.95 else 'ABSENT_OR_PARTIAL')
            if l['lane'] == 'FM3' and c != 'PRESENT': c = 'PRESENT_TIMING_ONLY' if l['timing_best'] >= 0.95 else c
            fm[l['lane']] = c
        else:
            d0 = l['oct']  # best-voice octave-folded delay-0
            if bd['match'] >= 0.9 and abs(bd['delay']) <= 6: c = 'DOUBLES_PSC_VOICE'
            elif bd['match'] >= 0.9: c = 'ECHO_COPY_OF_PSC_VOICE(d=%d)' % bd['delay']
            elif l['timing_any'] < 0.3 and bd['match'] < 0.5: c = 'ABSENT_UNIQUE'
            else: c = 'PARTIAL_OR_TEXTURE'
            psg[l['lane']] = dict(cls=c, onsets=l['onsets'], voice=bd['voice'], delay=bd['delay'], match=bd['match'], timing_any=l['timing_any'])
        cnt[(l['lane'][:2] if l['lane'].startswith('FM') else 'PSG', fm.get(l['lane']) if l['lane'].startswith('FM') else psg[l['lane']]['cls'].split('(')[0])] += 1
    summ.append(dict(source_song=s, psc_song=s+1, fm=fm, psg=psg, noise_events=nz[s][0], dac_write_frames=nz[s][1]))
json.dump(summ, open('ps3_audit_summary.json', 'w'), indent=1)
for k, v in sorted(cnt.items()): print(k, v)
print('noise/dac nonzero songs:', [s for s, (n, d) in nz.items() if n or d])
for r in summ:
    bad = {k: v for k, v in r['psg'].items() if not v['cls'].startswith('DOUBLES')}
    fmb = {k: v for k, v in r['fm'].items() if v != 'PRESENT'}
    if bad or fmb: print(r['source_song'], 'FM:', fmb, 'PSG:', {k: (v['cls'], v['onsets']) for k, v in bad.items()})
