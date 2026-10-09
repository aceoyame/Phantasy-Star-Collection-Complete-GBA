#!/usr/bin/env python3
"""Compile PSIV's variable-length MapData records into a native fixed table."""
from __future__ import annotations
import argparse,hashlib
from pathlib import Path
EXPECTED='511f35cc11f88316f8b8940e28ab298bd75a4da193672a80172884d6eb913b6a'
FIELDS=('kos_tiles','nem_tiles','kos_ram','chunks','map_updates','trans1','trans2','objects','treasure','layout_fg','layout_bg','data_manager','dialogue_tree','interactions','events','palette')
META=('valid','general','music','is_world','row_fg','col_fg','row_bg','col_bg','collision_plane')
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--rom',required=True,type=Path);ap.add_argument('--out-c',default=Path('gba/psc_map_desc.c'),type=Path);ap.add_argument('--out-h',default=Path('gba/psc_map_desc.h'),type=Path);a=ap.parse_args()
 r=a.rom.read_bytes();sha=hashlib.sha256(r).hexdigest()
 if len(r)!=0x300000 or sha!=EXPECTED:raise SystemExit('wrong PSIV ROM '+sha)
 def u8(p):return r[p]
 def u16(p):return int.from_bytes(r[p:p+2],'big')
 def u32(p):return int.from_bytes(r[p:p+4],'big')
 def rec(p,n):
  while u16(p)!=0xffff:p+=n
  return p+2
 def ff(p):
  while u8(p)!=0xff:p+=1
  return p+1
 err=u32(8);rows=[];valid=0
 for mid in range(0x1a1):
  base=u32(0x100000+mid*4)
  d={k:0 for k in FIELDS};m={k:0 for k in META}
  if base!=err and base<len(r):
   p=base;m.update(valid=1,general=u8(p),music=u8(p+1),is_world=int(mid<2));p+=2
   d['kos_tiles']=p
   while u16(p)<0x8000:p+=6
   p+=2;d['nem_tiles']=p
   while True:
    w=u16(p)
    if w==0xfffe:p+=8;continue
    if w&0x8000:p+=2;break
    p+=6
   d['kos_ram']=p
   while u16(p)!=0xffff:p+=6
   p+=2
   m['row_fg']=u8(p);m['col_fg']=u8(p+1);m['row_bg']=u8(p+2);m['col_bg']=u8(p+3);m['collision_plane']=u8(p+4);p+=6
   if m['collision_plane']>1: raise SystemExit(f'bad collision plane map {mid:03X}')
   for _ in range(2):
    mode=u16(p);p+=2
    if not mode:p+=8
   d['chunks']=p
   while u16(p)!=0xffff:p+=4
   p+=2;d['map_updates']=p;p=ff(p)
   for name,n in [('trans1',10),('trans2',10),('objects',10),('treasure',6)]:d[name]=p;p=rec(p,n)
   p=rec(p,2)
   if mid>=2:d['layout_fg']=u32(p);p+=4;d['layout_bg']=u32(p);p+=4
   d['dialogue_tree']=u32(p);p+=4;d['interactions']=p
   while u16(p)!=0xffff:p+=2
   p+=2;d['events']=p;p=ff(p);d['palette']=u32(p);p+=4;p+=4;d['data_manager']=p
   if d['palette']>=0x300000:raise SystemExit(f'bad palette map {mid:03X}')
   valid+=1
  rows.append((m,d))
 a.out_h.write_text('''#ifndef PS4_PSC_MAP_DESC_H\n#define PS4_PSC_MAP_DESC_H\n#include <stdint.h>\ntypedef struct Ps4PscMapDesc {\n uint32_t kos_tiles,nem_tiles,kos_ram,chunks,map_updates,trans1,trans2,objects,treasure,layout_fg,layout_bg,data_manager,dialogue_tree,interactions,events,palette;\n uint8_t valid,general,music,is_world,row_fg,col_fg,row_bg,col_bg,collision_plane;\n} Ps4PscMapDesc;\nconst Ps4PscMapDesc* ps4_psc_map_desc_get(uint16_t map_id);\n#endif\n''')
 lines=['''/* AUTO-GENERATED fixed-form PSIV MapData table. */\n#include "psc_map_desc.h"\nstatic const Ps4PscMapDesc ps4_maps[0x1A1]={''']
 for mid,(m,d) in enumerate(rows):
  vals=','.join(f'0x{d[k]:06X}u' for k in FIELDS)
  metas=','.join(f'{m[k]}u' for k in META)
  lines.append(f'{{{vals},{metas}}}, /* {mid:03X} */')
 lines.append('};\nconst Ps4PscMapDesc* ps4_psc_map_desc_get(uint16_t map_id){if(map_id>=0x1A1u||!ps4_maps[map_id].valid)return 0;return &ps4_maps[map_id];}\n')
 a.out_c.write_text('\n'.join(lines));print(f'PASS PSC map descriptors: valid={valid}/417 bytes~={len(rows)*(16*4+9)}')
if __name__=='__main__':main()
