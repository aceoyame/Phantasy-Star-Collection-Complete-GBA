#!/usr/bin/env python3
"""Generate named maps, authored events and flag labels from pinned source."""
from pathlib import Path
import re
R=Path(__file__).resolve().parents[1];D=R/'references/disassembly/ps4disasm/ps4disasm'
a=(D/'ps4.constants.asm').read_text();b=(D/'ps4.asm').read_text()
def label(s):return re.sub(r'(?<=[a-z])(?=[A-Z])',' ',s).replace('_',' ').upper()[:34]
names=['UNUSED']*417
for m in re.finditer(r'^MapID_(\w+)\s*=.*?;\s*(\$[0-9A-Fa-f]+|\d+)\s*$',a,re.M):
    n=int(m[2][1:],16) if m[2][0]=='$' else int(m[2]);names[n]=label(m[1])
events=['EVENT']*160
p=b.index('EventPtrs:');section=b[p:b.index('\n\n',p)]
for m in re.finditer(r'dc\.l\s+(\w+).*?;\s*(\$[0-9A-Fa-f]+|\d+)',section):
    n=int(m[2][1:],16) if m[2][0]=='$' else int(m[2])
    if n<len(events):events[n]=label(m[1].removeprefix('Event_'))
flags=['FLAG']*256
for m in re.finditer(r'^EventFlag_(\w+)\s*=\s*(\$[0-9A-Fa-f]+|\d+)',a,re.M):
    n=int(m[2][1:],16) if m[2][0]=='$' else int(m[2]);flags[n]=label(m[1])
out=['/* Generated from the pinned PSIV source; all labels stay in ROM. */\n']
for n,v in [('map',names),('event',events),('flag',flags)]:out.append(f'static const char* const r28_{n}_names[{len(v)}]={{\n'+''.join('    "'+x+'",\n' for x in v)+'};\n')
(R/'gba/r28_debug_names.h').write_text(''.join(out));print('maps',len(names),'events',sum(n!='EVENT' for n in events),'flags',sum(n!='FLAG' for n in flags))
