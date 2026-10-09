#!/usr/bin/env python3
"""Actual-linked PSG envelope controls and host/native music CRC comparison."""
import argparse,json,re,subprocess,sys,zlib
from pathlib import Path
R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts'
ap=argparse.ArgumentParser();ap.add_argument('--label',required=True);ap.add_argument('--clang',default='clang');a=ap.parse_args();prefix='R46_'+a.label
sys.path.insert(0,str(R/'tools_host/r46_mgba'));from mgba import GBA,L,run_until_break
symbols={}
for line in (R/'build-psc-module/ps4_psc_module.map').read_text().splitlines():
 p=line.split(None,4)
 if len(p)==5:
  try:symbols[p[4]]=int(p[0],16)
  except ValueError:pass
def symbol(part):
 if part in symbols:return symbols[part]
 matches=[v for k,v in symbols.items() if k==part or (part in k and not k.startswith('/'))]
 assert len(matches)==1,(part,matches)
 return matches[0]
# Derive fixture offsets from the current ARM/native-only compiler layout.
layout=subprocess.run([a.clang,'--target=arm-none-eabi','-mcpu=arm7tdmi','-std=c++17','-ffreestanding','-DMD2GBA_PS4_M4A_NATIVE_ONLY=1','-DMD2GBA_PS4_MAXMOD_MODE_A=1','-DMD2GBA_PS4_MAXMOD_CHANNEL_MAJOR=1','-DMD2GBA_PS4_V19_FULL_MIX=1','-I'+str(R/'gba/audio_p134r/include'),'-Xclang','-fdump-record-layouts','-fsyntax-only',str(R/'gba/audio_p134r/src/chip_audio.cpp')],capture_output=True,text=True,check=True).stdout
block=layout.split('struct ps4gba::chip_audio::(anonymous namespace)::lane_state\n')[1].split('*** Dumping AST Record Layout')[0]
offsets={name:int(re.search(r'\n\s*(\d+) \|[^\n]*\b'+name+r'\n',block).group(1)) for name in ['keyed','psg_envelope','psg_envelope_pos']}
size=int(re.search(r'\[sizeof=(\d+)',block).group(1));assert size==148
g=GBA(R.parent/'binaries/RUN_THIS_RELEASE48_PSC.gba');g.load(O/(prefix+'_MGBA_DEBUG.bin'));g.wr16(0x04000208,0)
md=g.mem(0x02000000,65536)
def call(part,args=()):
 address=symbol(part);L.mg_break(0xffffffff);L.mg_cpsr(0x3f if address&1 else 0x1f)
 for i,value in enumerate(args):L.mg_set_reg(i,value)
 for i,value in [(13,0x03007d80),(14,0x08800000),(15,address&~1)]:L.mg_set_reg(i,value)
 assert run_until_break(g,0x08800000,100),part
rom=(R/'references/roms/ps4built.bin').read_bytes()
be32=lambda p:int.from_bytes(rom[p:p+4],'big')
ptrs=be32(0xd1a74);fixture=0x0203ee08;checks=0
for env in range(1,11):
 start=be32(ptrs+(env-1)*4);cursor=0;attenuation=0;keyed=True
 g.put(fixture-8,b'\x5a'*8+bytes(size)+b'\x5a'*8)
 g.wr8(fixture+offsets['keyed'],1);g.wr8(fixture+offsets['psg_envelope'],env)
 for frame in range(128):
  if keyed:
   for safety in range(8):
    value=rom[start+cursor];cursor+=1
    if value<0x80:attenuation=value;break
    if value==0x81:cursor-=2;break
    if value==0x83:cursor-=2;keyed=False;break
    if value==0x80:cursor=0;continue
    if value==0x82:cursor=rom[start+cursor];continue
    raise AssertionError(value)
   else:raise AssertionError('oracle control loop')
  if frame:call('::advance_psg_envelope(',[fixture])
  assert bool(g.rd8(fixture+offsets['keyed']))==keyed,(env,frame,'key')
  if keyed:
   call('::psg_envelope_delta(',[fixture]);assert g.reg(0)==attenuation,(env,frame,g.reg(0),attenuation)
  checks+=1
 assert g.mem(fixture-8,8)==b'\x5a'*8 and g.mem(fixture+size,8)==b'\x5a'*8
 print('PASS actual-linked envelope',env,flush=True)
host={x['song']:x for x in json.loads((O/(prefix+'_PSG_HOLD.json')).read_text())['songs']}
music=[];output=0x0203f008
for id in [0x84,0x8a,0x93,0x94,0x95,0x9b,0xac]:
 call('ps4_audio_fallback_set_mute_mask',[0]);call('ps4_audio_fallback_play',[id]);assert g.reg(0)==1
 crc=0;g.put(output-8,b'\x5a'*240)
 for frame in range(1800):
  call('ps4_audio_fallback_render_224',[output]);crc=zlib.crc32(g.mem(output,224),crc)
 assert f'{crc:08x}'==host[id]['pcm_crc32'],(id,f'{crc:08x}',host[id])
 assert g.mem(output-8,8)==b'\x5a'*8 and g.mem(output+224,8)==b'\x5a'*8
 row={'song':id,'frames':1800,'samples':403200,'pcm_crc32':f'{crc:08x}','host_native_match':True};music.append(row);print(json.dumps(row),flush=True)
assert g.mem(0x02000000,65536)==md,'PSG/music calls mutated gameplay RAM'
report={'core':'upstream mGBA 0.10.5','envelope_checks':checks,'compiler_derived_lane_bytes':size,'compiler_derived_offsets':offsets,'songs':music,'music_samples':sum(x['samples'] for x in music),'entire_MD_RAM_guarded_bytes':65536,'scope':'actual linked envelope functions and full native music renderer; original-ROM envelope oracle and host/native CRC comparison; fresh current-ROM module state, IRQ disabled for direct-call checks; no listening, PSC selector handoff or hardware certification'}
(O/(prefix+'_MGBA_PSG_HOLD.json')).write_text(json.dumps(report,indent=2)+'\n')
