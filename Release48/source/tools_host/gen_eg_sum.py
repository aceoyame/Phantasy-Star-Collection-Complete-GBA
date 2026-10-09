#!/usr/bin/env python3
"""Generate exact prefix sums from the existing YM2612 increment table."""
from pathlib import Path
import re
p=Path(__file__).resolve().parents[1]/'additional/gsnd'
txt=(p/'gfm_tabs.h').read_text()
block=re.search(r'gfm_inc\[64\] = \{(.*?)\};',txt,re.S)[1]
words=[int(x,16) for x in re.findall(r'0x[0-9a-fA-F]+',block)]
assert len(words)==64
rows=[]
for w in words:
    q=[0]
    for i in range(8): q.append(q[-1]+((w>>(4*i))&15))
    rows.append('  {'+','.join(map(str,q))+'},')
(p/'gsnd_eg_sum.h').write_text('/* CP12: generated from gfm_inc by tools_host/gen_eg_sum.py. */\n#pragma once\n#ifdef MD_HOST\n#define EG_SUM_DATA\n#else\n#define EG_SUM_DATA __attribute__((section(".hotdata.egsum")))\n#endif\nstatic const uint8_t gfm_inc_sum[64][9] EG_SUM_DATA = {\n'+'\n'.join(rows)+'\n};\n')
