#!/usr/bin/env python3
"""CP22 field-object mapping census: decodes each FieldObjectsJmpTbl routine's initial $C(a4) sprite
mapping from the ROM.  usage: audit_field_object_mappings.py <ps4disasm_dir> <ps4built.bin>
(ps4disasm_dir must contain ps4.asm and ps4.lst).  Writes fieldobj_mappings.json in the CWD."""
import sys, os
DIS = sys.argv[1] if len(sys.argv) > 1 else '.'
ROMP = sys.argv[2] if len(sys.argv) > 2 else 'ps4built.bin'
import re,sys,json
from collections import defaultdict
L=open(os.path.join(DIS,'ps4.asm'),encoding='latin-1').read().split('\n')
lst=open(os.path.join(DIS,'ps4.lst'),encoding='latin-1').read().split('\n')
addr={}
for l in lst:
    m=re.match(r'\s*\d+/\s*([0-9A-F]+) :\s+([A-Za-z_][A-Za-z0-9_]*):',l)
    if m: addr[m.group(2)]=int(m.group(1),16)
rom=open(ROMP,'rb').read()
rows=[]
for i in range(97992,len(L)):
    m=re.match(r'\s*bra\.w\s+(\S+)\s*;\s*\$?([0-9A-F]+)(.*)',L[i])
    if not m: break
    rows.append((int(m.group(2),16),m.group(1),m.group(3).strip(' ;\t')))
labels={l.split(':')[0]:i for i,l in enumerate(L) if re.match(r'^[A-Za-z_][A-Za-z0-9_]*:',l)}
def mapping_of(name):
    i=labels.get(name)
    if i is None: return None
    for l in L[i:i+16]:
        m=re.search(r'move\.l\s+#(loc_[0-9A-F]+), \$C\(a4\)',l)
        if m: return m.group(1)
    return None
def decode(lbl):
    a=addr.get(lbl)
    if a is None: return None
    n=rom[a]+1; out=[]
    for k in range(n):
        y,sz,th,tl,x=rom[a+2+k*6:a+7+k*6]
        y=y-256 if y>127 else y; x=x-256 if x>127 else x
        out.append((((sz>>2)&3)*8+8,(sz&3)*8+8,y,x,(th<<8|tl)&0x7FF))
    return tuple(out)
groups=defaultdict(list); per={}
for t,name,cmt in rows:
    mp=mapping_of(name); d=decode(mp) if mp else None
    groups[d].append((t,name,mp,cmt[:48])); per[t]=(name,mp,d)
print('entries',len(rows),'last',hex(rows[-1][0]))
for key,v in sorted(groups.items(),key=lambda kv:-len(kv[1])):
    print(f'{len(v):3d} types mapping={key}')
    for t,name,mp,c in v[:5]: print(f'      ${t:03X} {name} {mp} {c}')
json.dump({hex(t):[n,m,d] for t,(n,m,d) in per.items()},open('fieldobj_mappings.json','w'))
