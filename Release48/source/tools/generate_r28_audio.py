#!/usr/bin/env python3
"""Raise all R26 instrument waves to 512 points with no new synthesis cost."""
from pathlib import Path
from collections import Counter
import json
import numpy as np
from generate_dense_fm_multisamples import parse

ROOT = Path(__file__).resolve().parents[1]
WAVES = ROOT/'gba/audio_p134r/src/ps4_fm_multisample_data_generated.inc'
BATTLE_BANKS = {0x0D570F, 0x0D70DE}

def main():
    sr, points, blob, zones = parse(WAVES)
    assert points == 256 and len(zones) == 3977, 'Run the R26 asset generator first'
    old = np.array(blob, dtype=np.int16)
    bank = bytearray()
    cache = {}
    counts = Counter()
    for z in zones:
        points = 512
        counts[points] += 1
        offsets = []
        for off in z['offs']:
            wave = old[off:off+256]
            if points == 512:
                bins = np.fft.rfft(wave.astype(np.float64))
                bins[-1] *= 0.5
                wave = np.rint(np.fft.irfft(bins, n=512)*2).clip(-127,127)
            raw = wave.astype(np.int8).tobytes()
            if raw not in cache:
                cache[raw] = len(bank)
                bank.extend(raw)
            offsets.append(cache[raw])
        z['offs'] = offsets
    out = ['// R28: R26 timbres; all 3977 zones use 512-point waves.\n',
           f'constexpr unsigned int generated_fm_wave_sample_rate = {sr}u;\n',
           'constexpr unsigned int generated_fm_wave_points = 512u; // maximum\n',
           f'constexpr unsigned int generated_fm_wave_blob_size = {len(bank)}u;\n',
           'const signed char generated_fm_wave_blob[] = {\n']
    samples = np.frombuffer(bank, dtype=np.int8)
    for i in range(0,len(samples),20):
        out.append('    '+', '.join(map(str,samples[i:i+20]))+',\n')
    out += ['};\n', f'constexpr unsigned int generated_fm_wave_zone_count = {len(zones)}u;\n',
            'const fm_wave_zone generated_fm_wave_zones[] = {\n']
    for z in zones:
        offsets = ', '.join(str(v)+'u' for v in z['offs'])
        gains = ', '.join(map(str,z['gains']))
        out.append(f"    {{ 0x{z['bank']:06X}u, {{ {offsets} }}, {z['inst']}, {z['note']}, {{ {gains} }} }},\n")
    out.append('};\n')
    WAVES.write_text(''.join(out))
    report = {'zones':len(zones), 'wave_blob_bytes':len(bank),
              'extra_wave_bytes':len(bank)-len(blob), 'zones_by_points':dict(counts),
              'coverage':'all instrument banks',
              'source':'R26 periodic sinc expansion; no new YM captures or pitch changes'}
    (ROOT/'R28_AUDIO_ASSET_REPORT.json').write_text(json.dumps(report,indent=2)+'\n')
    print(report)

if __name__ == '__main__': main()
