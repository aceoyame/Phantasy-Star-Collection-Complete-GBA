#!/usr/bin/env python3
"""Independent arithmetic from original 68000 counters, ROM sine and palettes.
Run check_r45_pd.py first. UI, sprite-only PLC overlays and hardware excluded.
"""
from pathlib import Path
import struct,json
R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts';rom=(R/'references/roms/ps4built.bin').read_bytes()
w=lambda a:int.from_bytes(rom[a:a+2],'big')
cos=lambda a:(-rom[0x423CC+((~((a+64)&255))&127)] if (a+64)&128 else rom[0x423CC+((a+64)&127)])
fg=[0]*192;bg=[0]*192;pal=[w(0x306F4+i*2) for i in range(32)];saved=pal[:]
for i in range(14):pal[i]=pal[i+16]=0xEEE
cycles=[None]*12;frames=[]
def signed(v):return ((v+32768)%65536)-32768
def palette_change(start,delta,target=None):
 for i in range(start+1,start+14):
  # Model the original three ROR passes, keeping full-word carry/borrow.
  v=pal[i]
  for sh in (4,8,0):
   rot=((v>>sh)|(v<<(16-sh)))&65535 if sh else v
   need=(rot&15)!=0 if target is None else (rot&15)!=((target[i]>>sh)&15)
   if need:rot=(rot+delta)&65535
   v=((rot<<sh)|(rot>>(16-sh)))&65535 if sh else rot
  pal[i]=v

def cycle_tick():
 for i,c in enumerate(cycles):
  if c is None:continue
  a,f,t=c
  if t>=rom[a+1+f]:f+=1;t=0
  t+=1
  if f>=rom[a]:f=0
  colors=a+1+rom[a]+(1 if rom[a]%2==0 else 0)
  pal[3+i if i<9 else 26+i-9]=w(colors+f*2);cycles[i]=(a,f,t)

def record():frames.append(pal[:]+fg[:]+bg[:])
angle=0
for frame in range(192):
 cycle_tick()
 if frame<64:
  if frame%8==7:
   if frame==63:
    for i in range(9):cycles[i]=(0x30600 if i<6 else [0x3066E,0x30694,0x306BA][i-6],i if i<6 else 0,0)
   else:
    palette_change(0,-2,saved);palette_change(16,-2,saved)
    if pal[0]:pal[0]=(pal[0]-0x222)&65535
 if frame==191:
  fg=[0]*192;bg=[0]*192
 else:
  angle=(angle+2)%256
  limit=0 if frame<64 else frame-63
  for i in range(64):
   raw=cos(angle+i*4);fg[i]=max(0,raw-limit) if raw>=0 else min(0,raw+limit)
  fg[64:120]=fg[:56];bg[120:192]=fg[:72]
 bg[:120]=[cos(frame+i*4)//8 for i in range(120)]
 record()
bgangle=191;delta=[signed(w(0x2FA1E+i*2)) for i in range(120)]
for enemy,maximum,table in [(0x85,168,0x2F70A),(0x86,192,0x2EB66)]:
 fg=[0]*192;bg=[0]*192
 for i in (0,2,4,6):cycles[i]=None
 stage=0;counter=0;timer=0;index=0;entered=False
 while True:
  cycle_tick();end=False
  if stage==0:
   timer=(timer+1)%8
   if not timer:
    counter+=1;palette_change(16,-2)
    if counter==2:stage=1;counter=0
  elif stage==1:
   counter+=1
   for i in range(min(counter,120)):fg[i]=signed(fg[i]+delta[i])
   for i in range(max(0,counter-120)):bg[120+i]=signed(bg[120+i]+delta[i])
   if counter==maximum:stage=2;counter=0
  elif stage==2:
   for i in range(120):fg[i]=signed(fg[i]+delta[i])
   for i in range(maximum-120):bg[120+i]=signed(bg[120+i]+delta[i])
   counter+=1
   if not rom[table+index]:stage=3;counter=maximum-1;entered=True
   elif counter==rom[table+index]:index+=1
  elif stage==3:
   if entered:
    entered=False
    live=0x2F8B8 if enemy==0x85 else 0x2EDB8;save=0x2F898 if enemy==0x85 else 0x2ED98
    pal[16:32]=[w(live+i*2) for i in range(16)];saved[16:32]=[w(save+i*2) for i in range(16)]
   counter=(counter-1)%256
   if counter==255:stage=4;counter=timer=0
   else:
    for i in range(min(counter,119)+1):fg[i]=signed(fg[i]+delta[i])
    if counter>119:
     for i in range(counter-119):bg[120+i]=signed(bg[120+i]+delta[i])
     bg[counter+1]=0
    elif counter==119:bg[120]=0
    else:fg[counter+1]=0
  else:
   timer=(timer+1)%8
   if not timer:
    counter+=1;palette_change(16,2,saved)
    if counter==2:end=True
  bgangle=(bgangle+1)%256;bg[:120]=[cos(bgangle+i*4)//8 for i in range(120)]
  record()
  if end:break
raw=(O/'R45_PD_VISUAL_STATES.bin').read_bytes();actual=list(struct.iter_unpack('<32H384h',raw));assert len(actual)==len(frames)==1306
for i,(a,b) in enumerate(zip(actual,frames)):
 for j,(v,expected) in enumerate(zip(a,b)):
  assert v==expected,(i,j,v,expected)
report={'production_controller_frames':len(frames),'palette_words_compared':len(frames)*32,'scroll_words_compared':len(frames)*384,'source_quirks_retained':['opening child executes after root clears its scrolls','PD sprite callback precedes stat reload','four alternating palette objects cleared','16-bit scroll overflow and uncleared first FG row'], 'scope':'Original ROM arithmetic for every opening/transform frame. Sprite-only PLC overlays, GBA timing and full playthrough are not certified.'}
(O/'R45_PD_VISUAL_EVIDENCE.json').write_text(json.dumps(report,indent=2)+'\n')
print('PASS',len(frames),'original-source palette and scroll states')
