#!/usr/bin/env python3
from pathlib import Path
import argparse,hashlib,re
ap=argparse.ArgumentParser(description='Audit PSIV MapDataManager usage against the native shared dispatcher')
ap.add_argument('--rom',required=True,type=Path)
ap.add_argument('--runtime',default=Path('gba/title_runtime_v06.c'),type=Path)
a=ap.parse_args(); rom=a.rom.read_bytes(); src=a.runtime.read_text()
sha=hashlib.sha256(rom).hexdigest()
if sha!='511f35cc11f88316f8b8940e28ab298bd75a4da193672a80172884d6eb913b6a':raise SystemExit('wrong PSIV ROM: '+sha)
sec=src[src.index('static void field_apply_map_manager_load'):src.index('static void field_apply_map_managers_load')]
supported={int(m.group(1),0) for m in re.finditer(r'case\s+(0x[0-9A-Fa-f]+|\d+)u:',sec)}
def u16(p):return int.from_bytes(rom[p:p+2],'big')
def u32(p):return int.from_bytes(rom[p:p+4],'big')
def ff(p):
 while rom[p]!=255:p+=1
 return p+1
def rec(p,n):
 while u16(p)!=65535:p+=n
 return p+2
use={};err=u32(8);valid=0
for mid in range(0x1a1):
 base=u32(0x100000+mid*4)
 if base==err or base>=len(rom):continue
 valid+=1;p=base+2
 while u16(p)<0x8000:p+=6
 p+=2
 while True:
  w=u16(p)
  if w==0xfffe:p+=8;continue
  if w&0x8000:p+=2;break
  p+=6
 p=rec(p,6);p+=6
 for _ in range(2):
  mode=u16(p);p+=2
  if not mode:p+=8
 p=rec(p,4);p=ff(p)
 for n in (10,10,10,6,2):p=rec(p,n)
 if mid>=2:p+=8
 p+=4
 while u16(p)!=0xffff:p+=2
 p+=2;p=ff(p);p+=8
 while u16(p)!=0xffff:
  v=u16(p);use.setdefault(v,[]).append(mid);p+=2
print(f'valid_maps={valid} used_manager_ids={len(use)} supported_used_ids={len(set(use)&supported)} total_occurrences={sum(map(len,use.values()))} supported_occurrences={sum(len(use[i]) for i in set(use)&supported)}')
for i in sorted(use):
 maps=','.join(f'{m:03X}' for m in use[i][:10]); more='...' if len(use[i])>10 else ''
 print(f'{i:02X} {"SUPPORTED" if i in supported else "TODO":9s} count={len(use[i]):2d} maps={maps}{more}')
