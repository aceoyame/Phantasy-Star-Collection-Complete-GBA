#!/usr/bin/env python3
"""Generate source rear-view party battle poses directly from verified PSIV ROM."""
from __future__ import annotations
import argparse,hashlib,json,os
from pathlib import Path
from PIL import Image
from md_compression import enigma_decompress
from generate_enemy_animation_frames import Rom
ROOT=Path(__file__).resolve().parent.parent
EXPECTED='511f35cc11f88316f8b8940e28ab298bd75a4da193672a80172884d6eb913b6a'
TRANS=(248,0,248)
def mdcol(w):return (((w>>1)&7)*255//7,((w>>5)&7)*255//7,((w>>9)&7)*255//7)
def cols(words):return [mdcol(w) for w in words]
def tilepix(tile,x,y):
 q=tile[y*4+(x>>1)];return (q&15) if x&1 else (q>>4)
def mapped(art,cells,palette,columns=6):
 im=Image.new('RGB',(64,64),TRANS);ox=(64-columns*8)//2;oy=8;tc=len(art)//32
 if len(cells)!=columns*6:raise ValueError((len(cells),columns))
 for n,c in enumerate(cells):
  ti=c&0x7ff
  if ti>=tc:raise ValueError(f'tile {ti}>={tc}')
  hf=bool(c&0x0800);vf=bool(c&0x1000);t=art[ti*32:(ti+1)*32]
  tx=n%columns;ty=n//columns
  for y in range(8):
   sy=7-y if vf else y
   for x in range(8):
    sx=7-x if hf else x;ci=tilepix(t,sx,sy)
    if ci:im.putpixel((ox+tx*8+x,oy+ty*8+y),palette[ci])
 return im
def hmap(rom,off):
 p=off+4;n=rom.read(p,1)[0]+1;return rom.read(p,1+n*6)
def hardware(art,m,palette):
 n=m[0]+1;rs=[]
 for i in range(n):
  r=m[1+i*6:7+i*6];y=r[0]-256 if r[0]&128 else r[0];x=r[4]-256 if r[4]&128 else r[4];sz=r[1];w=((sz>>2)&3)+1;h=(sz&3)+1;prop=(r[2]<<8)|r[3];rs.append((x,y,w,h,prop))
 x0=min(x for x,_,_,_,_ in rs);y0=min(y for _,y,_,_,_ in rs);x1=max(x+w*8 for x,_,w,_,_ in rs);y1=max(y+h*8 for _,y,_,h,_ in rs)
 ox=(64-(x1-x0))//2-x0;oy=(64-(y1-y0))//2-y0;im=Image.new('RGB',(64,64),TRANS);tc=len(art)//32
 for x,y,w,h,prop in rs:
  bt=prop&0x7ff;hf=bool(prop&0x0800);vf=bool(prop&0x1000)
  for tx in range(w):
   for ty in range(h):
    stx=w-1-tx if hf else tx;sty=h-1-ty if vf else ty;ti=bt+stx*h+sty
    if ti>=tc:raise ValueError(f'hw tile {ti}>={tc}')
    t=art[ti*32:(ti+1)*32]
    for py in range(8):
     sy=7-py if vf else py
     for px in range(8):
      sx=7-px if hf else px;ci=tilepix(t,sx,sy)
      if ci:im.putpixel((ox+x+tx*8+px,oy+y+ty*8+py),palette[ci])
 return im
def sheet(frames,path):
 out=Image.new('RGB',(64*len(frames),64),TRANS)
 for i,im in enumerate(frames):out.paste(im,(i*64,0))
 out.save(path)
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--rom',required=True,type=Path);a=ap.parse_args();raw=a.rom.read_bytes();sha=None if os.environ.get('PS4_NO_HASH')=='1' else hashlib.sha256(raw).hexdigest()
 if len(raw)!=0x300000 or (sha is not None and sha!=EXPECTED):raise SystemExit('wrong PSIV ROM')
 r=Rom(raw);out=ROOT/'generated_precomp'/'party_battle';out.mkdir(parents=True,exist_ok=True)
 for p in out.glob('*'):p.unlink()
 base_words=tuple(int.from_bytes(r.read(0x296300+i*2,2),'big') for i in range(16));base=cols(base_words);manifest={'source_rom_sha256':sha,'base_palette_rom_offset':'0x296300','base_palette_words':[f'0x{x:04X}' for x in base_words],'characters':{}}
 # (id,name,body start,end, pose map offsets).  These are the complete
 # CharBattleLongRangePLCOffs roster; keeping this table exhaustive prevents
 # the runtime from silently substituting an overworld sprite for a party
 # member whose battle sheet was not lowered.
 chars=(
  (0,'chaz',0x24FC0E,0x24FF6E,{'idle':0x2515AE,'cast':0x2515C2}),
  (1,'alys',0x2515E2,0x251A42,{'idle':0x251A42,'cast':0x251A5A,'slasher':0x251A7A,'throw_slasher':0x251A9C,'two_slashers':0x251ABA,'throw_two_slashers':0x251AE0}),
  (2,'hahn',0x251B02,0x251E02,{'idle':0x252922,'cast':0x252938,'arms_raised':0x252956}),
  (3,'rune',0x25297A,0x252F5A,{'idle':0x253FDA,'cast':0x253FEC,'charge_magic':0x254008}),
  (4,'gryz',0x254024,0x254624,{'idle':0x256364,'cast':0x256376,'arms_raised':0x256394}),
  (5,'rika',0x2563B8,0x256758,{'idle':0x257AD8,'cast':0x257AF2}),
  (6,'demi',0x257B12,0x257E12,{'idle':0x2581D2,'cast':0x2581E6,'charge':0x258200,'load_gun':0x25821C,'fire_gun':0x25822A}),
  (7,'wren',0x25823E,0x2586DE,{'idle':0x258CDE,'cast':0x258CF6,'charge':0x258D18,'load_gun':0x258D3C,'fire_gun':0x258D4C}),
  (8,'raja',0x258D66,0x259246,{'idle':0x259BE6,'cast':0x259C00,'arms_raised':0x259C1E,'arms_raised_2':0x259C3E}),
  (9,'kyra',0x259C60,0x25A320,{'idle':0x25A320,'cast':0x25A336,'charge_magic':0x25A350,'cast_2':0x25A372,'slasher':0x25A392,'throw_slasher':0x25A3B4,'two_slashers':0x25A3D0,'throw_two_slashers':0x25A3F6}),
  (10,'seth',0x25A414,0x25A714,{'idle':0x25AF34,'cast':0x25AF4A}),
 )
 for cid,name,bs,be,poses in chars:
  body=r.read(bs,be-bs);frames=[];names=[]
  gun=(r.read(0x257E12,0x2581D2-0x257E12) if name=='demi' else
       r.read(0x2586DE,0x258CDE-0x2586DE) if name=='wren' else None)
  for pn,mo in poses.items():
   cells=enigma_decompress(r.tail(mo));pa=body
   # BattleChar_LongRangeGunInit replaces the body PLC at the SAME VRAM
   # tile base. Both gun maps are relative to that replacement bank; they
   # must never address body tiles or an appended body+gun concatenation.
   # Charge precedes the replacement and still uses the ordinary body PLC.
   if gun is not None and pn in ('load_gun','fire_gun'):pa=gun
   frames.append(mapped(pa,cells,base,7 if name=='wren' and pn=='load_gun' else 6));names.append(pn)
  sheet(frames,out/f'{name}_poses.png');manifest['characters'][name]={'id':cid,'poses':names,'pose_file':f'{name}_poses.png'}
 # Exact close-range physical mappings.
 sword=list(base);sword[10:14]=[mdcol(x) for x in (0x0888,0x0666,0,0)]
 chaz_art=[r.read(0x24FF6E,0x25028E-0x24FF6E),r.read(0x25028E,0x25056E-0x25028E),r.read(0x25056E,0x2507EE-0x25056E)]
 chaz_map=[0x25F7D6,0x25F7F4,0x25F812];cf=[hardware(chaz_art[i],hmap(r,chaz_map[i]),sword) for i in range(3)];sheet(cf,out/'chaz_attack.png');manifest['characters']['chaz']['attack']={'file':'chaz_attack.png','holds':[10,5,20]}
 hahn_art=[r.read(0x251E02,0x2520A2-0x251E02),r.read(0x2520A2,0x252362-0x2520A2),r.read(0x252362,0x252622-0x252362)];hm=[0x25F8E2,0x25F8FA,0x25F918];hf=[hardware(hahn_art[i],hmap(r,hm[i]),base) for i in range(3)];sheet(hf,out/'hahn_attack.png');manifest['characters']['hahn']['attack']={'file':'hahn_attack.png','holds':[10,5,20]}
 claw=list(base);claw[10:14]=[mdcol(x) for x in (0x02EE,0x028E,0x08EE,0x0C2E)]
 ra=[(0x256758,0x256A18),(0x256A18,0x256D38),(0x256D38,0x2570D8),(0x2570D8,0x257478),(0x257478,0x2577B8)];rm=[0x25FC22,0x25FC46,0x25FC64,0x25FC88,0x25FCAC];rf=[hardware(r.read(a,b-a),hmap(r,m),claw) for (a,b),m in zip(ra,rm)];sheet(rf,out/'rika_attack.png');manifest['characters']['rika']['attack']={'file':'rika_attack.png','holds':[5,10,4,4,12]}
 (out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
 print('wrote party battle assets',out)
if __name__=='__main__':main()
