#!/usr/bin/env python3
"""ARM/Thumb differential state at identical guest frames in busy scenes."""
from pathlib import Path
import os,sys,subprocess,json,tempfile
R=Path(__file__).resolve().parents[2];V=R.parent/'validation_cp29'
if len(sys.argv)>1:
 tag,index,out=sys.argv[1:];index=int(index)
 sys.path.insert(0,str(R/'source/tools_host/cp09_prof'));from gtest import boot,sym,L
 g=boot(R/f'binaries/RUN_THIS_R48_{tag}.gba',index);entry=sym('md_begin_frame_input');m=sym('M');z=sym('md_zram');cpu=sym('C')
 memories=[(sym(n),sz) for n,sz in [('md_ram',65536),('md_vram',65536),('md_zram',8192),('md_cram',128),('md_vsram',128),('aot_rgen',512)]]
 states=bytearray()
 for frame in range(1051):
  L.mg_break(entry);g.run(100);assert L.mg_hit()
  ret=g.reg(14)&~1;L.mg_break(ret);g.run(100);assert L.mg_hit()
  keys=8 if 180<=frame<184 else 2 if 400<=frame<404 else 8 if 500<=frame<504 else 1 if frame>=600 and frame%80<4 else 0
  g.wr16(m+44,((keys&2)<<3)|((keys&1)<<5)|((keys&8)<<4))
  if frame==150:g.wr8(z+0x1c0a,0x81)
  if frame in [150,450,750,1050]:
   states+=g.mem(cpu,136)+g.mem(cpu+156,4) # no optimization history
   states+=g.mem(m,40)+g.mem(m+44,144) # no renderer bookkeeping or event history
   for a,n in memories:states+=g.mem(a,n)
 Path(out).write_bytes(states);sys.exit(0)
rows=[]
with tempfile.TemporaryDirectory(prefix='cp24-arm-busy-') as tmp:
 for index in [2,9]:
  data=[]
  for tag in ['CP28_NATIVE_GLYPH','CP29_NATIVE_ROWS']:
   p=Path(tmp)/(tag+'.bin');elf=R/'../cp28_base/md.elf' if tag=='CP28_NATIVE_GLYPH' else R/'source/additional/build/md.elf'
   subprocess.run([sys.executable,__file__,tag,str(index),str(p)],env=dict(os.environ,MD_ELF=str(elf)),check=True,capture_output=True)
   data.append(p.read_bytes())
  assert data[0]==data[1],('ARM busy state mismatch',index,next(i for i,(a,b) in enumerate(zip(*data)) if a!=b))
  rows.append({'index':index,'guest_checkpoints':[150,450,750,1050],'bytes_compared':len(data[0]),'identical':True})
  print('ARM busy CPU, bus and memories index',index,'exact',flush=True)
(V/'GBA_BUSY_STATE_EXACT.json').write_text(json.dumps({'all_pass':True,'cases':rows,'excluded':'CPU spin/branch history, M.rfull renderer bookkeeping and M.ev/evctx diagnostic history'},indent=2)+'\n')
