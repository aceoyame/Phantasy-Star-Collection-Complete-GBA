#!/usr/bin/env python3
"""Actual ARM7 execution: matched guest inputs, PCM, state, pixels and cycle costs."""
from pathlib import Path
import sys,os,json,subprocess,concurrent.futures
W=Path(__file__).resolve().parents[1];R=W/'Release48';V=W/'native58/gba_results';V.mkdir(exist_ok=True)
if len(sys.argv)>1:
 mode,index=sys.argv[1:];index=int(index)
 sys.path.insert(0,str(R/'source/tools_host/cp09_prof'))
 from gtest import boot,sym,L
 rom=W/'RUN_THIS_R48_CP57_NATIVE_WINDOW_LOOPS.gba' if mode=='ref' else W/'native58/internal_candidate.gba'
 g=boot(rom,index);entry=sym('md_begin_frame_input');machine=sym('M');zram=sym('md_zram');cpu=sym('C')
 sound=sym('mdsnd_frame');tick=sym('last_render_tick');page=sym('md_page')
 memories=[(sym(n),sz) for n,sz in [('md_ram',65536),('md_vram',65536),('md_zram',8192),('md_cram',128),('md_vsram',128),('aot_rgen',512),('z',40)]]
 states=bytearray();pixels=bytearray();pcm=bytearray();times={}
 for frame in range(1051):
  L.mg_break(entry);g.run(100);assert L.mg_hit(),('input',frame)
  ret=g.reg(14)&~1;L.mg_break(ret);g.run(100);assert L.mg_hit(),('return',frame)
  keys=8 if 180<=frame<184 else 2 if 400<=frame<404 else 8 if 500<=frame<504 else 1 if frame>=600 and frame%80<4 else 0
  g.wr16(machine+44,((keys&2)<<3)|((keys&1)<<5)|((keys&8)<<4))
  if frame==150:g.wr8(zram+0x1c0a,0x81)
  if frame in (149,449,749,1049):g.wr32(tick,0);g.wr32(machine+40,1)
  if frame in (150,450,750,1050):
   times[frame]=g.cycles();states+=g.mem(cpu,136)+g.mem(cpu+156,4)+g.mem(machine,40)+g.mem(machine+44,144)
   for a,n in memories:states+=g.mem(a,n)
   visible=g.rd32(page)&1;pixels+=g.mem(0x06000000+visible*0xa000,38400)+g.mem(0x05000000,128)
  if frame==1050:break
  L.mg_break(sound);g.run(100);assert L.mg_hit(),('sound',frame)
  left,right=g.reg(0),g.reg(1);ret=g.reg(14)&~1
  L.mg_break(ret);g.run(100);assert L.mg_hit(),('sound return',frame)
  pcm+=g.mem(left,224)+g.mem(right,224)
 for ext,data in (('state',states),('pixels',pixels),('pcm',pcm)):(V/f'GBA_{index}_{mode}.{ext}').write_bytes(data)
 rows=[{'guest_start':f,'cycles':times[f+300]-times[f],'guest_per_display_frame':300*280896/(times[f+300]-times[f])} for f in (150,450,750)]
 print(json.dumps(rows));sys.exit(0)
def job(item):
 mode,index=item;elf=W/'cp57_base/md.elf' if mode=='ref' else W/'native58/armcheck/md.elf'
 if os.environ.get('CP46_REUSE') and all((V/f'GBA_{index}_{mode}.{ext}').exists() for ext in ('json','state','pixels','pcm')):
  return item,json.loads((V/f'GBA_{index}_{mode}.json').read_text())
 p=subprocess.run([sys.executable,__file__,mode,str(index)],env=dict(os.environ,MD_ELF=str(elf)),capture_output=True,text=True)
 if p.returncode:raise RuntimeError(f'{mode} {index}: {p.stderr}')
 (V/f'GBA_{index}_{mode}.json').write_text(p.stdout)
 return item,json.loads(p.stdout)
indices=[int(x) for x in os.environ.get('CP46_GAMES','2,3').split(',')]
with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
 results=dict(pool.map(job,[(m,i) for i in indices for m in ('ref','native')]))
rows=[]
for i in indices:
 row={'index':i,'baseline_CP57':results['ref',i],'candidate':results['native',i]}
 row['cycle_reduction_percent']=[100*(a['cycles']-b['cycles'])/a['cycles'] for a,b in zip(row['baseline_CP57'],row['candidate'])]
 for ext in ('state','pixels','pcm'):
  a=(V/f'GBA_{i}_ref.{ext}').read_bytes();b=(V/f'GBA_{i}_native.{ext}').read_bytes();row[ext+'_exact']=a==b
  if a!=b:row[ext+'_first_difference']=next((n for n,(x,y) in enumerate(zip(a,b)) if x!=y),min(len(a),len(b)))
 rows.append(row);print(row,flush=True)
(V/'GBA_COMPARISON.json').write_text(json.dumps({'guest_frames':1050,'script':'START 180..183; B 400..403; START 500..503; A >=600, four frames every80; song81 at150','rows':rows},indent=2)+'\n')
assert all(all(r[e+'_exact'] for e in ('state','pixels','pcm')) for r in rows)
