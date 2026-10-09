#!/usr/bin/env python3
"""Reproducible build-time bake of exact Genesis/SN76489 pitch steps (CP17)."""
from pathlib import Path
out=Path(__file__).with_name('psg_step_lut.h')
rows=['/* CP17: precomputed exact Genesis PSG pitch steps.',
' * Generated from ((uint64_t)(0xFFFFFFFFu / p) * 547935u) >> 16',
' * for 10-bit SN76489 tone period p. No runtime division, no audio approximation. */',
'static const u32 psg_step_lut[1024]={']
vals=[0 if p<9 else (((0xffffffff//p)*547935)>>16)&0xffffffff for p in range(1024)]
for i in range(0,1024,8): rows.append('  '+','.join('0x%08xu'%v for v in vals[i:i+8])+',')
rows.append('};')
out.write_text('\n'.join(rows)+'\n')
print(out, 'values',len(vals))
