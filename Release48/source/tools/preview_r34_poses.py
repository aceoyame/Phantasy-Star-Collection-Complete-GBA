#!/usr/bin/env python3
"""Render native immutable sprite bytes with the original scene palettes."""
from pathlib import Path
import json,re
from PIL import Image,ImageDraw,ImageFont
R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts'
rom=(R/'references/roms/ps4built.bin').read_bytes();blob=(R/'gba/r31_field_poses.bin').read_bytes()
poses=json.loads((R/'generated/r31_field_poses_report.json').read_text())['poses']
rows=re.findall(r'^\{([^}]+)\},', (R/'gba/psc_map_desc.c').read_text(),re.M)
map_pal=lambda id:int(rows[id].split(',')[15].rstrip('u'),0)
def palette(addr):
 out=[]
 for n in range(16):
  c=int.from_bytes(rom[addr+n*2:addr+n*2+2],'big');out.append(tuple(((c>>s)&14)*255//14 for s in (0,4,8)))
 return out
def pose(kind,face,frame,pal):
 p=next(p for p in poses if (p['type'],p['facing'],p['frame'])==(kind,face,frame))
 im=Image.new('RGBA',(p['w'],p['h']))
 for y in range(p['h']):
  for x in range(p['w']):
   off=p['offset']+((y//8)*(p['w']//8)+x//8)*32+(y%8)*4+(x%8)//2
   c=(blob[off]>>(4*(x&1)))&15
   if c:im.putpixel((x,y),(*pal[c],255))
 return im
party=palette(0x296300)
groups=[('Hahn idle directions',[(0xC,f,0,party) for f in (0,4,8,12)]),
 ('Hahn walk directions',[(0xC,f,1,party) for f in (0,4,8,12)]),
 ('Hahn second walk frame',[(0xC,f,3,party) for f in (0,4,8,12)])]
sheet=Image.new('RGB',(864,3*200+55),(22,29,38));d=ImageDraw.Draw(sheet)
try:font=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',17)
except OSError:font=ImageFont.load_default()
d.text((20,15),'R34 Hahn native field sprites',(235,238,245),font=font)
for row,(label,items) in enumerate(groups):
 top=55+row*200;d.text((20,top),label,(225,231,240),font=font)
 for col,(kind,face,frame,pal) in enumerate(items):
  im=pose(kind,face,frame,pal);im=im.resize((im.width*3,im.height*3),Image.Resampling.NEAREST)
  x=20+col*210;y=top+32
  sheet.paste(im,(x+(192-im.width)//2,y+(192-im.height)//2),im)
  d.text((x,y+140),f'{kind:04X} / {face:02X} / {frame}',(153,173,197),font=font)
path=O/'R34_field_pose_preview.png';sheet.save(path);print(path)
