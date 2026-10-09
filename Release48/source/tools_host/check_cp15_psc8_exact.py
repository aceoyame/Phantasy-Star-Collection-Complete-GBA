#!/usr/bin/env python3
"""Same guest-frame inputs: exact Z80, bus, guest memory, PCM and periodic GBA graphics/save bytes."""
from pathlib import Path
import json,sys,os,subprocess,struct,tempfile
R=Path(__file__).resolve().parents[2];V=R.parent/'validation_cp15'
CASES={'PS1':(0x082d3b80,0x80000,10),'Adventure':(137706368,131072,1),'Gaiden':(166669648,262144,17),'MicroVDP':(0x02028000,32768,0),'MicroIRQ':(0x02028000,32768,0),'MicroMix':(0x02028000,32768,2),'MicroStereo':(0x02028000,32768,1)}
if len(sys.argv)>3:
 tag,name,out=sys.argv[1:4];elf=R/'source/references/cp14_final/psc8.elf' if tag=='before' else R/'source/psc8/build/psc8.elf'
 syms={}
 for line in subprocess.run(['llvm-nm',str(elf)],capture_output=True,text=True,check=True).stdout.splitlines():
  row=line.split()
  if len(row)==3:syms[row[2]]=int(row[0],16)
 sys.path.insert(0,str(R/'source/tools_host/cp08_mgba'));from mgba2 import GBA,L
 import ctypes;L.mg_break.argtypes=[ctypes.c_uint32]
 g=GBA(R/f'binaries/RUN_THIS_R48_{"CP14" if tag=="before" else "CP15"}_TEST.gba',entry=0x086ca500)
 g.put(0x0203ff00,struct.pack('<IIII',*CASES[name],0));times={};busy=[]
 if name.startswith('Micro'):
  sys.path.insert(0,str(R/'source/psc8/tools'));import blktest
  if name=='MicroVDP':image=blktest.prog_a()
  elif name=='MicroIRQ':image=blktest.prog_b()
  else:
   a=blktest.Asm();blktest.header(a)
   for port,value in [(0x7f,0x85),(0x7f,0x21),(0x7f,0x90),(0x7f,0xc5),(0x7f,0x01),(0x7f,0xe7),(0x7f,0xf0)]:a.raw(0x3e,value,0xd3,port)
   if name=='MicroMix':
    for reg,value in [(0x30,0x10),(0x10,0x80),(0x20,0x16),(0x0e,0x3f)]:a.raw(0x3e,reg,0xd3,0xf0,0x3e,value,0xd3,0xf1)
   top=a.pc()
   for value in ([0,1,3,2,1,0] if name=='MicroMix' else [0x11,0x22,0x44,0x88,0xff,0]):
    a.raw(0x3e,value,0xd3,0xf2 if name=='MicroMix' else 6,0x06,0x40,0x10,0xfe)
   a.raw(0xc3,top&255,top>>8);image=bytes(a.b)
  g.put(0x02028000,image.ljust(32768,b'\0'))
 fields={'ram':8192,'sram':32768,'vram':16384,'cram':64,'vr':16,'banks':4,'zcyc':4,'cycles_next':4,'sample_next':4,'status':1,'stereo':1,'fm_control':1,'fm_addr':1,'fm_active':1,'tones':6,'noise_lfsr':2,'volumes':4,'noise':1,'psg_latch':1,'psg_clock_fraction':1,'phases':16,'fastopll':580}
 # sizeof FastOPLL is read from its symbol in the ELF table rather than guessed.
 for line in subprocess.run(['llvm-nm','-S',str(elf)],capture_output=True,text=True,check=True).stdout.splitlines():
  row=line.split()
  if len(row)==4 and row[3]=='fastopll':fields['fastopll']=int(row[1],16)
 with open(out,'wb') as f:
  nframes=1200 if not name.startswith('Micro') else 120
  for frame in range(nframes):
   L.mg_break(syms['frame']);g.run(100);assert L.mg_hit(),(name,'entry',frame)
   ret=g.reg(14)&~1;b=g.reg(0)&3;t0=g.cycles()
   keys=8 if 60<=frame<64 else 1 if frame>=100 and frame%80<4 else 2 if frame>=800 and frame%120<4 else [16,64,32,128][(frame//80)%4] if frame>=300 and frame%80 in range(20,44) else 0
   if frame in [100,400,700,1000]:times[frame]=t0
   L.mg_break(ret);g.run(100,keys);assert L.mg_hit(),(name,'return',frame)
   busy.append(g.cycles()-t0)
   ctx=syms['z80a_ctx'];state=bytearray()
   for off,size in [(0x120,8),(0x12c,12),(0x150,12),(0x164,28),(0x194,8),(0x1a8,20),(0x1bc,4)]:state+=g.mem(ctx+off,size)
   for symbol,size in fields.items():state+=g.mem(syms[symbol],size)
   state+=g.mem(syms['pcm']+b*672,304)+g.mem(syms['pcm']+b*672+336,304)
   if frame%20==0:state+=g.mem(0x6000000,0x18000)+g.mem(0x7000000,0x400)+g.mem(0x5000000,0x400)+g.mem(0xe000000,32768)
   f.write(struct.pack('<I',len(state)));f.write(state)
   if frame in [400,1000]:g.shot(V/f'FIXED_{name}_{tag}_{frame}.png')
 rows=[]
 for start in ([100,400,700] if not name.startswith('Micro') else []):
  cycles=times[start+300]-times[start];work=sum(busy[start:start+300]);rows.append({'guest_start':start,'guest_end':start+300,'native_cycles':cycles,'rate':300*280896/cycles,'frame_work_cycles':work,'mean_frame_work_cycles':work/300})
 print(json.dumps({'game':name,'tag':tag,'frames':nframes,'windows':rows}));sys.exit(0)
rows=[]
with tempfile.TemporaryDirectory(prefix='cp15-psc8-') as tmp:
 for name in CASES:
  stats={};paths=[]
  for tag in ['before','after']:
   p=Path(tmp)/(name+'_'+tag+'.state');paths.append(p)
   sub=subprocess.run([sys.executable,__file__,tag,name,str(p)],capture_output=True,text=True)
   if sub.returncode:raise RuntimeError(sub.stderr)
   stats[tag]=json.loads(sub.stdout)
  first=None
  with paths[0].open('rb') as a,paths[1].open('rb') as b:
   for frame in range(stats['before']['frames']):
    n=struct.unpack('<I',a.read(4))[0];m=struct.unpack('<I',b.read(4))[0];x=a.read(n);y=b.read(m)
    if x!=y and first is None:
     first={'frame':frame,'byte':next((i for i,(v,w) in enumerate(zip(x,y)) if v!=w),min(n,m)),'lengths':[n,m]}
     (V/(name+'_before_first.state')).write_bytes(x);(V/(name+'_after_first.state')).write_bytes(y)
  row={'game':name,'frames':stats['before']['frames'],'identical':first is None,'first_difference':first,'before':stats['before']['windows'],'after':stats['after']['windows']};rows.append(row)
  print(name,'exact' if first is None else first,flush=True)
  for x,y in zip(row['before'],row['after']):print('  work cycles %.0f -> %.0f; rate %.3f -> %.3f'%(x['mean_frame_work_cycles'],y['mean_frame_work_cycles'],x['rate'],y['rate']),flush=True)
 result={'all_pass':all(row['identical'] for row in rows),'cases':rows};(V/'PSC8_FIXED_EXACT.json').write_text(json.dumps(result,indent=2)+'\n')
 assert result['all_pass']
