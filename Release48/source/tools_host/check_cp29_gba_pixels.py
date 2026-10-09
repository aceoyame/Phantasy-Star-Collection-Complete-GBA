#!/usr/bin/env python3
"""Compare actual Mode 4 output/CRAM, including hardware DMA3 transfers."""
from pathlib import Path
import os,sys,subprocess,json,tempfile
R=Path(__file__).resolve().parents[2];V=R.parent/'validation_cp29'
if len(sys.argv)>1:
 tag,index,out=sys.argv[1:];index=int(index)
 sys.path.insert(0,str(R/'source/tools_host/cp09_prof'));from gtest import boot,sym,L
 name='RUN_THIS_R48_CP28_NATIVE_GLYPH.gba' if tag=='before' else 'RUN_THIS_R48_CP29_NATIVE_ROWS.gba'
 g=boot(R/'binaries'/name,index);entry=sym('md_begin_frame_input');m=sym('M');z=sym('md_zram');tick=sym('last_render_tick');page=sym('md_page');pixels=bytearray()
 for f in range(1051):
  L.mg_break(entry);g.run(100);assert L.mg_hit();ret=g.reg(14)&~1;L.mg_break(ret);g.run(100);assert L.mg_hit()
  keys=8 if 180<=f<184 else 2 if 400<=f<404 else 8 if 500<=f<504 else 1 if f>=600 and f%80<4 else 0
  g.wr16(m+44,((keys&2)<<3)|((keys&1)<<5)|((keys&8)<<4))
  if f==150:g.wr8(z+0x1c0a,0x81)
  if f in [149,449,749,1049]:
   # Force the next normal presentation in both builds. This isolates exact
   # renderer output from the unchanged minimum native presentation cadence.
   g.wr32(tick,0);g.wr32(m+40,1)
  if f in [150,450,750,1050]:
   visible=g.rd32(page)&1;pixels+=g.mem(0x06000000+visible*0xa000,38400)+g.mem(0x05000000,128)
 Path(out).write_bytes(pixels);sys.exit(0)
rows=[]
with tempfile.TemporaryDirectory(prefix='cp29-gba-pixels-') as td:
 for index in [2,9]:
  data=[]
  for tag,elf in [('before',R.parent/'cp28_base/md.elf'),('after',R/'source/additional/build/md.elf')]:
   p=Path(td)/(tag+'.pixels');subprocess.run([sys.executable,__file__,tag,str(index),str(p)],env=dict(os.environ,MD_ELF=str(elf)),capture_output=True,text=True,check=True);data.append(p.read_bytes())
  assert data[0]==data[1],('Actual GBA output mismatch',index,next(i for i,(a,b) in enumerate(zip(*data)) if a!=b))
  rows.append({'index':index,'guest_checkpoints':[150,450,750,1050],'bytes_compared':len(data[0]),'identical':True});print('Actual Mode 4 pixels/palette index',index,'exact',flush=True)
(V/'GBA_PIXELS_EXACT.json').write_text(json.dumps({'all_pass':True,'cases':rows,'method':'Same guest frame/input state, forced full normal presentation immediately before checkpoints to avoid differences from unchanged native-time render cadence; actual visible Mode 4 framebuffer and 64-color hardware palette compared'},indent=2)+'\n')
