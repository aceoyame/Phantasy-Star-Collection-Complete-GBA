#!/usr/bin/env python3
"""Fresh-ROM native SFX ownership/PCM checks; no PD controller is exercised."""
from pathlib import Path
import argparse,json,sys,zlib
R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts'
sys.path.insert(0,str(R/'tools_host/r46_mgba'));import mgba
sys.modules['gba']=mgba
sys.path.insert(0,str(R/'tools_host/r46_gba_emu'));from boot_debug import boot_to_debug
from mgba import GBA,L,run_until_break
sys.path.insert(0,str(R/'tools'));from generate_source_sfx_chip_headers import parse_one,be32
ap=argparse.ArgumentParser();ap.add_argument('--label',required=True);a=ap.parse_args();prefix='R46_'+a.label
s={}
for line in (R/'build-psc-module/ps4_psc_module.map').read_text().splitlines():
 p=line.split()
 if len(p)>=5:
  try:s[p[-1]]=int(p[0],16)
  except ValueError:pass
mask_address=next(v for k,v in s.items() if k.endswith('::render_mute_mask'))
rom_path=R.parent/'binaries/RUN_THIS_RELEASE48_PSC.gba';state=O/(prefix+'_MGBA_DEBUG.bin')
g=boot_to_debug(str(rom_path),str(state))
state.with_suffix('.rom_crc.json').write_text(json.dumps({'rom_bytes':rom_path.stat().st_size,'rom_crc32':f'{zlib.crc32(rom_path.read_bytes()):08x}'},indent=2)+'\n')
print('Fresh release native-module boot reached debug selector',flush=True)
g.wr16(0x04000208,0) # IRQ disabled only for controlled direct-call PCM fixtures.
md=g.mem(0x02000000,65536)
rom=(R/'references/roms/ps4built.bin').read_bytes();pcm=(R/'gba/pcm_sfx/ps4_sfx_bank.pcm').read_bytes()
desc=json.loads((R/'gba/pcm_sfx/ps4_sfx_runtime_manifest.json').read_text())['records'];descs={x['id']:x for x in desc}
channel_bits={0:2,1:4,2:8,4:16,5:32,0x80:64,0xa0:128,0xc0:256,0xe0:256}
masks={}
for sid in range(0xb5,0xfb):
 t=0xde4b6 if sid<0xf8 else 0xde5c2;first=0xb5 if sid<0xf8 else 0xf8
 h=parse_one(rom,sid,be32(rom,t+4*(sid-first)))
 masks[sid]=sum(set(channel_bits[x['channel']] for x in h['tracks']))
 if sid>=0xf8:assert masks[sid]==16
def call(name,args=()):
 address=s[name];L.mg_break(0xffffffff);L.mg_cpsr(0x3f if address&1 else 0x1f)
 for i,v in enumerate(args):L.mg_set_reg(i,v)
 for i,v in [(13,0x03007d80),(14,0x08800000),(15,address&~1)]:L.mg_set_reg(i,v)
 assert run_until_break(g,0x08800000,100),name
def command(id):call('ps4_audio_sound_index',(id,));assert g.reg(0)==1,id
def expect(mask):assert g.rd16(mask_address)==mask,(mask,g.rd16(mask_address))
output=0x0203f000;cases=0;samples=0
for sid in range(0xf8,0xfb):
 for nid in range(0xb5,0xf8):
  for order in range(2):
   command(0x95);expect(0)
   for id in ([nid,sid] if order else [sid,nid]):command(id)
   owner=masks[nid] if g.rd8(s['normal_active']) else 0
   expect(owner|16)
   g.wr8(s['music_active'],0);start=descs[sid]['length']-211;g.wr32(s['special_pos'],start)
   g.put(output,bytes([0x5a])*544);call('ps4_audio_mix_one_frame',(output+8,))
   expected=bytearray()
   for j in range(528):
    q=pcm[descs[sid]['offset']+(start+j)%descs[sid]['length']];q=q-256 if q>=128 else q
    p=pcm[descs[nid]['offset']+j] if owner and j<descs[nid]['length'] else 0;p=p-256 if p>=128 else p
    expected.append((p if owner&16 else (p>>1)+(q>>1) if owner else q)&255)
   assert g.mem(output+8,528)==expected,('linked PCM priority',sid,nid,order)
   assert g.mem(output,8)==bytes([0x5a])*8 and g.mem(output+536,8)==bytes([0x5a])*8
   assert g.rd32(s['special_pos'])==317
   command(0xfc);expect(16);assert g.rd8(s['special_active'])
   command(nid);command(0xfd);expect(owner);assert not g.rd8(s['special_active'])
   command(0xfc);expect(0)
   command(sid);command(nid);command(0x84);expect(0)
   assert not g.rd8(s['special_active']) and not g.rd8(s['normal_active'])
   assert g.rd8(s['current_music_id'])==0x84
   if owner:
    command(sid);command(nid);g.wr8(s['music_active'],0)
    g.wr32(s['normal_pos'],g.rd32(s['normal_len'])-7);g.wr32(s['special_pos'],descs[sid]['length']-3)
    call('ps4_audio_mix_one_frame',(output+8,));expect(16)
    assert not g.rd8(s['normal_active']) and g.rd32(s['special_pos'])==525
    call('ps4_audio_mix_one_frame',(output+8,))
    assert g.mem(output+8,528)==pcm[descs[sid]['offset']+525:descs[sid]['offset']+1053]
    assert g.rd32(s['special_pos'])==1053;samples+=1056
   cases+=1;samples+=528
 print('PASS linked special owner',hex(sid),flush=True)
assert g.mem(0x02000000,65536)==md,'controlled mixer changed MD/gameplay RAM'
report={'core':'upstream mGBA 0.10.5','fresh_current_ROM_native_module_boot':True,'cases':cases,'exact_PCM_samples':samples,'entire_MD_RAM_guarded_bytes':65536,'all_match':True,'scope':'actual linked C mixer, Thumb ownership helpers, actual ROM PCM and mask; direct-call fixtures temporarily disable IRQ; IRQ field/recovery separately tested; no PD, full playthrough, audible quality or hardware certification'}
(O/(prefix+'_MGBA_SFX_OWNERSHIP.json')).write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report),flush=True)
