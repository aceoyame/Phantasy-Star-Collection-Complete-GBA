#!/usr/bin/env python3
"""R22: rerender PSIV SFX B5-FA from source sequences using the host four-operator/PSG backend.
This is an offline build step: the GBA still receives one cheap signed-8 PCM voice.
"""
from pathlib import Path
import argparse,json,subprocess,tempfile,math
import numpy as np
from scipy.signal import resample_poly
RATE=31536; SYNTH_RATE=13379; FIRST=0xB5; LAST=0xFA; SILENT={0xF6,0xF7}; SPECIAL=0xF8

def main():
 ap=argparse.ArgumentParser();ap.add_argument('--calibration-bank',type=Path,default=Path('gba/pcm_sfx/ps4_sfx_bank.r19.pcm'));ap.add_argument('--calibration-manifest',type=Path,default=Path('gba/pcm_sfx/ps4_sfx_runtime_manifest.r19.json'));ap.add_argument('--manifest',type=Path,default=Path('gba/pcm_sfx/ps4_sfx_runtime_manifest.json'));ap.add_argument('--renderer',type=Path,default=Path('tools_host/audio_sim/render_sfx_source'));ap.add_argument('--output-dir',type=Path,default=Path('gba/pcm_sfx'));a=ap.parse_args()
 old=json.loads(a.manifest.read_text()); recs=old['records']; bank=bytearray(); rows=[]
 calbank=a.calibration_bank.read_bytes() if a.calibration_bank.is_file() else b''
 cal=json.loads(a.calibration_manifest.read_text()) if a.calibration_manifest.is_file() else None
 with tempfile.TemporaryDirectory() as td:
  td=Path(td)
  for r in recs:
   sid=int(r['id']); frames=int(r.get('source_duration_frames') or r.get('duration_frames') or 1); out=td/f'{sid:02x}.raw'
   if sid in SILENT: pcm=bytes(round(frames*RATE/60))
   else:
    subprocess.run([str(a.renderer),f'{sid:02X}',str(frames),str(SYNTH_RATE),str(out)],check=True)
    native=np.frombuffer(out.read_bytes(),dtype=np.int8).astype(np.float64)
    # The reference synth's phase increments are calibrated for 13,379 Hz.
    # R20 accidentally emitted those samples as 31,536 Hz, pitching every
    # synthesized effect up by 31536/13379 ~= 2.357x. Render at the native
    # rate, then band-limited-resample to the Direct Sound transport rate.
    target_len=round(frames*RATE/60)
    if native.size:
     rs=resample_poly(native,RATE,SYNTH_RATE,window=('kaiser',7.5))
     if rs.size < target_len: rs=np.pad(rs,(0,target_len-rs.size))
     rs=rs[:target_len]
     pcm=np.clip(np.rint(rs),-128,127).astype(np.int8).tobytes()
    else:
     pcm=bytes(target_len)
    # Preserve R19's proven per-effect loudness while replacing the waveform/timbre.
    # This avoids turning the fidelity rerender into a new clipping/mix regression.
    if cal and calbank and sid not in SILENT:
     cr=cal['records'][sid-FIRST]; oldpcm=calbank[cr['offset']:cr['offset']+cr['length']]
     def sv(b): return b-256 if b>127 else b
     oldr=math.sqrt(sum(sv(v)*sv(v) for v in oldpcm)/max(1,len(oldpcm)))
     newr=math.sqrt(sum(sv(v)*sv(v) for v in pcm)/max(1,len(pcm)))
     scale=(oldr/newr) if newr>0.01 else 1.0
     # Keep a little margin for R19's music+SFX combiner.
     vals=[]
     for v in pcm:
      q=int(round(sv(v)*scale)); q=max(-118,min(118,q)); vals.append(q&255)
     pcm=bytes(vals)
   off=len(bank);bank.extend(pcm)
   rows.append({**r,'offset':off,'length':len(pcm),'special_loop':sid>=SPECIAL,'expected_silent':sid in SILENT,'render_backend':'source-sequenced four-operator FM/PSG host reference @13379 Hz, polyphase-resampled to 31536 Hz'})
 a.output_dir.mkdir(parents=True,exist_ok=True);(a.output_dir/'ps4_sfx_bank.pcm').write_bytes(bank)
 manifest={**old,'source':'R22 PSIV source-sequenced SFX rerender; native 13,379 Hz synth + polyphase resample to 31,536 Hz; IDs $B5-$FA','bank_bytes':len(bank),'records':rows,'render_revision':'R22_NATIVE_RATE_REF4OP_RESAMPLED'}
 (a.output_dir/'ps4_sfx_runtime_manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
 h=['#ifndef PS4_SFX_BANK_H','#define PS4_SFX_BANK_H','','#include <stdint.h>','',f'#define PS4_SFX_FIRST_ID 0x{FIRST:02X}u',f'#define PS4_SFX_LAST_ID 0x{LAST:02X}u',f'#define PS4_SFX_COUNT {len(rows)}u',f'#define PS4_SFX_BANK_BYTES {len(bank)}u','', 'typedef struct { uint32_t offset; uint32_t length; uint8_t flags; } ps4_sfx_desc_t;','#define PS4_SFX_FLAG_SPECIAL_LOOP 1u','#define PS4_SFX_FLAG_SILENT 2u','', 'static const ps4_sfx_desc_t ps4_sfx_descs[PS4_SFX_COUNT] = {']
 for r in rows:
  flags=(1 if r['special_loop'] else 0)|(2 if r['expected_silent'] else 0);h.append(f'    {{{r["offset"]}u,{r["length"]}u,{flags}u}}, /* {r["id_hex"]} {r["name"]} */')
 h+=['};','','#endif',''];(a.output_dir/'ps4_sfx_bank.h').write_text('\n'.join(h))
 print('R22 source SFX bank',len(rows),'IDs',len(bank),'bytes')
if __name__=='__main__':main()
