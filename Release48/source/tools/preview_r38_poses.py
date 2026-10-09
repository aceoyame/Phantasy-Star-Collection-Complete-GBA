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
groups=[('Collapsed Raja $374',[(0x374,0,0,party)]),
 ('Clinic Esper: down/up',[(0x8194,face,n,party) for face in (16,20) for n in range(4)]),
 ('Clinic Esper: right/left',[(0x8194,face,n,party) for face in (24,28) for n in range(4)])]
sheet=Image.new('RGB',(1030,440),(22,29,38));d=ImageDraw.Draw(sheet)
try:font=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',15)
except OSError:font=ImageFont.load_default()
d.text((16,10),'R38 original collapsed Raja and clinic Esper art; native OBJ bytes',(235,238,245),font=font)
for row,(label,items) in enumerate(groups):
 top=40+row*132;d.text((16,top),label,(225,231,240),font=font)
 for col,(kind,face,frame,pal) in enumerate(items):
  im=pose(kind,face,frame,pal);im=im.resize((im.width*2,im.height*2),Image.Resampling.NEAREST)
  x=16+col*126;y=top+25;sheet.paste(im,(x,y),im)
  d.text((x,y+68),f'frame {frame}',(153,173,197),font=font)
path=O/'R38_field_pose_preview.png';sheet.save(path);print(path)
