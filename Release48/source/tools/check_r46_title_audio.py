#!/usr/bin/env python3
"""Cold native-title run: IRQ production, sequencer progress and transport PCM."""
from pathlib import Path
import argparse, array, json, math, sys, wave, zlib
R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts'
ap=argparse.ArgumentParser();ap.add_argument('--label',required=True);ap.add_argument('--frames',type=int,default=3600);a=ap.parse_args()
sys.path.insert(0,str(R/'tools_host/r46_mgba'));from mgba import GBA
s={}
for line in (R/'build-psc-module/ps4_psc_module.map').read_text().splitlines():
 p=line.split()
 if len(p)>=5:
  try:s[p[-1]]=int(p[0],16)
  except ValueError:pass
rom=R.parent/'binaries/RUN_THIS_RELEASE48_PSC.gba';g=GBA(rom)
prefix='R46_'+a.label;pcm=O/(prefix+'_TITLE.pcm');g.pcm(pcm)
rows=[]
for n in range(a.frames):
 g.run(1)
 if n%60==59:
  p=s['ps4_audio_diag'];row={'frame':g.frame(),'PC':hex(g.reg(15)),**dict(zip(['vblanks','productions','underruns','late_commits','waitvb_calls','commands'],[g.rd32(p+4*i) for i in range(6)])),'max_produce_ticks':g.rd16(p+26),'music_sequence':g.rd32(s['music_seq']),'music_id':g.rd8(s['current_music_id']),'max_late_offset_samples':g.rd16(p+24)}
  rows.append(row)
  if n%600==599:print(json.dumps(row),flush=True)
g.pcm();data=pcm.read_bytes();samples=array.array('h');samples.frombytes(data)
with wave.open(str(pcm.with_suffix('.wav')),'wb') as w:w.setnchannels(2);w.setsampwidth(2);w.setframerate(32768);w.writeframes(data)
pcm.unlink();g.shot(O/(prefix+'_TITLE.png'),2)
report={'ROM_crc32':f'{zlib.crc32(rom.read_bytes()):08x}','frames':a.frames,'rows':rows,'PCM_RMS':math.sqrt(sum(x*x for x in samples)/len(samples)),'scope':'cold boot at native module entry through natural fading title intro and idle title; no pressed Start, PSC selector, save/physical listening certification'}
(O/(prefix+'_TITLE_AUDIO.json')).write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({'last':rows[-1],'PCM_RMS':report['PCM_RMS']}),flush=True)
