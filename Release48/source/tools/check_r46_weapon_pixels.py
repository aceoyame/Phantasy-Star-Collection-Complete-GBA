#!/usr/bin/env python3
"""Original-ROM weapon pixels, using the independent combat inverse oracle."""
from pathlib import Path
import json,runpy
D=Path(__file__).resolve().parent;v=runpy.run_path(str(D/'check_r46_combat_pixels.py'));R=v['R'];raw=v['raw'];word=v['word'];long=v['long'];signed=v['signed'];mapping=v['mapping'];plc=v['plc'];standard=v['standard'];check=v['check'];rows=v['rows'];banks=v['banks']
s=(R/'gba/r46_weapon_effects.c').read_text();parts=rows(s,'r46_weapon_parts');anims=rows(s,'r46_weapon_anims');own=(R/'gba/r46_weapon_effects.bin').read_bytes();manifest=json.loads((R.parent/'build_artifacts/R46_WEAPON_ASSET_MANIFEST.json').read_text())
before=v['counts'].copy()
for entry,a in zip(manifest['entries'],anims):
 table=a[4];source=[];arts=[];base=0
 if entry['kind'] in ('melee','slash_body'):
  for i in range(word(table)):
   p=table+4+signed(word(table+4+i*4));sprites=mapping(p+4);high=max((pr&0x7ff)+(sw//8)*(sh//8) for x,y,sw,sh,pr in sprites);arts.append(raw[long(p):long(p)+high*32])
   dual=entry.get('variant')==2 and entry.get('actor') in (0,2,4,5,10);threshold=5 if entry.get('actor') in (4,5) else 3;turned=entry.get('left',False) or dual and i>=threshold
   if turned:
    sprites=[]
    for j in range(raw[p+4]+1):
     b=raw[p+5+j*6:p+11+j*6];sprites.append((b[5]-256 if b[5]&128 else b[5],b[0]-256 if b[0]&128 else b[0],(((b[1]>>2)&3)+1)*8,((b[1]&3)+1)*8,((b[2]<<8)|b[3])^0x800))
   source.append((max(1,raw[table+3+i*4]),sprites))
 else:
  source=standard(table);art=plc(raw[0x8B0A+entry['item']]);start=0x9C40 if raw[0x8B0A+entry['item']]==0x77 else 0x9B80
  art=bytes(start-0x9B80)+art;art=art[entry['destination']-0x9B80:];arts=[art]*len(source)
  if entry.get('left'):
   source=[]
   for i in range(word(table)):
    p=table+2+signed(word(table+2+i*2));sprites=[]
    if not raw[p+1]&128:
     for j in range(raw[p+1]+1):
      b=raw[p+2+j*6:p+8+j*6];sprites.append((b[5]-256 if b[5]&128 else b[5],b[0]-256 if b[0]&128 else b[0],(((b[1]>>2)&3)+1)*8,((b[1]&3)+1)*8,((b[2]<<8)|b[3])^0x800))
    source.append((max(1,raw[p]),sprites))
 check(a,parts,own,source,arts,0,('weapon',entry['index'],entry['kind']))
counts=v['counts'];report={'result':'PASS','animations':len(anims),'part_frames':counts['part_frames']-before['part_frames'],'pixels':counts['pixels']-before['pixels'],'bank_bytes':len(own),'scope':'Original physical table/art pointers, explicit left-hand coordinates and source PLC/destination-derived trails and gun frames. No generated renderer/packer reused. Runtime hit/miss/critical/reaction integration requires separate review.'}
(R.parent/'build_artifacts/R46_WEAPON_PIXEL_EVIDENCE.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS original weapon pixels',report)
