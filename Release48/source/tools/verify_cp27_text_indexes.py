#!/usr/bin/env python3
"""Verify immutable native text indexes against every applicable guest ROM."""
from pathlib import Path
import re,sys,struct
R=Path(sys.argv[1]) if len(sys.argv)>1 else Path(__file__).resolve().parents[2]
A=R/'source/additional'; names=['Anne','Rudger','Kinds','Shilka','Amia','Huey','Eusis']
roms=[(R/f'additional_roms/{n}_Adventure_-_English.bin').read_bytes() for n in names];r=roms[0]
pairs=[struct.unpack_from('>HH',r,0x2b856+4*i) for i in range(93)]
assert all(v for k,v in pairs[:-1]) and pairs[-1]==(0,0)
records=sorted((key<<16)|i for i,(key,_) in enumerate(pairs[:-1]))
actual=[int(v,16) for v in re.findall(r'0x([0-9a-f]+)u',(A/'native_kern_index.h').read_text())];assert actual==records
bases=[0x2b7b9,0x2b7c5,0x2b7da,0x2b7dd];lists=[r[a:r.index(b'\0',a)] for a in bases];assert list(map(len,lists))==[10,19,1,3]
assert [(i,j,x) for i,b in enumerate(lists) for j,x in enumerate(b) if x in b[:j]]==[(1,12,76)]
s=(A/'native_class_index.h').read_text()
assert [int(v,16) for v in re.findall(r'0x([0-9a-f]+)u',s)]==bases
lengths=[int(v) for v in re.search(r'class_len\[4\]=\{([^}]+)',s)[1].split(',')];assert lengths==list(map(len,lists))
def rows(name):
 body=s.split(name,1)[1].split('};',1)[0];return [[int(v) for v in row.split(',') if v.strip()] for row in re.findall(r'\{([^{}]*)\}',body)]
assert rows('class_bytes[4][20]')==[list(b+b'\0') for b in lists]
assert rows('class_first[4][128]')==[[b.index(x) if x in b else len(b) for x in range(128)] for b in lists]
for name,rom in zip(names,roms):
 assert rom[0x2b856:0x2b856+93*4]==r[0x2b856:0x2b856+93*4],name
 assert all(rom[a:a+len(b)+1]==b+b'\0' for a,b in zip(bases,lists)),name
print('CP27 text indexes PASS: 92 word pairs, four class lists, all seven guest ROMs')
