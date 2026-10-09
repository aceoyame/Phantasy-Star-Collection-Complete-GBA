#!/usr/bin/env python3
"""Band-limit the fixed 128-point YM-derived FM wave bank for PSC's 13,379 Hz mixer.

The reference waves were generated at 31,536 Hz; the GBA replays their harmonic
shape at 13,379 Hz. Remove harmonics that would fold back below its Nyquist.
This is offline asset processing: it adds no ROM data or ARM work. Run once on
the unmodified generated include from R17; an embedded marker prevents repeats.
"""
from pathlib import Path
import json
import re

import numpy as np

root = Path(__file__).resolve().parents[1]
path = root / 'gba/audio_p134r/src/ps4_fm_multisample_data_generated.inc'
source = path.read_text()
marker = '// R18: band-limited for 13,379 Hz PSC mixer.'
if marker in source: raise SystemExit('Already band-limited')
music = (root / 'gba/audio_p134r/src/ps4_chip_audio_data_generated.inc').read_text()
phase = list(map(int, re.findall(r'\d+', music.split('generated_fm_phase_inc[128] = {')[1].split('};')[0])))
start = source.index('const signed char generated_fm_wave_blob[] = {')
data_start = source.index('{', start)+1
data_end = source.index('};', data_start)
waves = np.array([int(x) for x in re.findall(r'-?\d+', source[data_start:data_end])], dtype=np.float64)
assert len(waves) == 335616 and len(waves)%128 == 0
zone_text = source.split('generated_fm_wave_zones[] = {')[1].split('};')[0]
zones = re.findall(r'\{\s*0x[0-9A-Fa-f]+u,\s*\{([^}]+)\},\s*\d+,\s*(\d+),\s*\{', zone_text)
assert len(zones)==511
waves = waves.reshape((-1,128))
# Some zones reuse the same physical wave. Use the lowest cutoff required by
# any zone referencing it, so shared samples cannot reintroduce aliasing.
required = {}
for offsets, note in zones:
    center=int(note)
    if center>83: continue
    for off in map(int,re.findall(r'\d+',offsets)):
        assert off%128==0 and off//128 < len(waves)
        required[off//128]=max(required.get(off//128,0),min(center+5,83))
changed = 0
removed_energy = 0.0
for wi, hi in required.items():
    # Zones above note 83 can put even their fundamental beyond Nyquist;
    # leave those to a future pitch-aware runtime resampler.
    hz = phase[hi] * 13379.0 / (2**32)
    if hz <= 0: continue
    cutoff = (13379/2)/hz
    # The last representable harmonic gets a short cosine taper. No gain
    # renormalization: the retained harmonic levels stay source-derived.
    factors = np.ones(65)
    for k in range(1,65):
        if k >= cutoff: factors[k] = 0.0
        elif k > cutoff-1: factors[k] = (1 + np.cos(np.pi*(k-(cutoff-1))))/2
    if np.all(factors == 1): continue
    before = waves[wi]
    bins = np.fft.rfft(before)
    after = np.fft.irfft(bins*factors, n=128)
    quantized = np.clip(np.rint(after),-128,127)
    if not np.array_equal(quantized,before): changed += 1
    removed_energy += float(np.mean((before-after)**2))
    waves[wi] = quantized

numbers = waves.astype(np.int8).ravel()
lines = ['    '+', '.join(str(int(v)) for v in numbers[i:i+20])+',' for i in range(0,len(numbers),20)]
source = source[:data_start]+'\n'+'\n'.join(lines)+'\n'+source[data_end:]
source = marker+'\n'+source
path.write_text(source)
report = {'changed_128_point_waves':changed,'total_waves':len(waves),
          'mean_squared_harmonic_removal':round(removed_energy/len(waves),2),
          'mixer_rate_hz':13379,'source_rate_hz':31536,
          'method':'per-zone highest-note Nyquist harmonic cutoff, one-bin cosine taper; high fundamentals left unchanged'}
(root/'gba/audio_p134r/R18_WAVE_BANDLIMIT.json').write_text(json.dumps(report,indent=2)+'\n')
print(report)
