#!/usr/bin/env python3
"""Require fixed addresses of every FM sample before enabling CP20 EWRAM bank shadow."""
import re,sys
from pathlib import Path
r=Path(sys.argv[1]) if len(sys.argv)>1 else Path('.')
s=(r/'source/additional/gsnd/gsnd_bank.c').read_text()
vals=[int(v,16) for v in re.findall(r'0x(08[0-9a-fA-F]{6})u',s)]
lo=0x0804D120;hi=0x0805F660
assert len(vals)==843,('bank table count',len(vals))
assert min(v&~3 for v in vals)==lo,'wave base moved; regenerate safe shadow constants'
assert max((v&~3)+(32<<(v&3)) for v in vals)==hi,'wave end moved; regenerate safe shadow constants'
for i,v in enumerate(vals):
 a=v&~3;end=a+(32<<(v&3))
 assert lo<=a<end<=hi,('table',i,hex(a),hex(end))
print('CP20 FM bank safety PASS: 843 entries, 75,072 immutable bytes at 0x0804D120–0x0805F660')
