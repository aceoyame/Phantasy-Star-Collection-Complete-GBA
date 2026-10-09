#!/usr/bin/env python3
"""R22: build the PSIV SFX PCM bank through a source-driver -> VGM -> YM2612/SN76489 reference path.

The PSIV source sequencer remains authoritative for commands/timing. A host-only
instrumented copy of chip_audio emits the equivalent chip register state once
per source VBlank, ffmpeg/libgme renders those registers with an actual Genesis
chip emulator, and this script stores signed-8 31,536 Hz PCM for the GBA.
No VGM/emulation work runs on GBA hardware.
"""
from pathlib import Path
import argparse, json, math, subprocess
import numpy as np

RATE=31536; FIRST=0xB5; LAST=0xFA; SILENT={0xF6,0xF7}; SPECIAL=0xF8

def signed_bytes(blob):
    return np.frombuffer(blob,dtype=np.int8).astype(np.float64)

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--base-manifest',type=Path,default=Path('gba/pcm_sfx/ps4_sfx_runtime_manifest.r22_ratefix_ref4op.json'))
    ap.add_argument('--calibration-bank',type=Path,default=Path('gba/pcm_sfx/ps4_sfx_bank.r19.pcm'))
    ap.add_argument('--calibration-manifest',type=Path,default=Path('gba/pcm_sfx/ps4_sfx_runtime_manifest.r19.json'))
    ap.add_argument('--vgm-renderer',type=Path,default=Path('tools_host/audio_sim/render_sfx_vgm'))
    ap.add_argument('--ffmpeg',default='ffmpeg')
    ap.add_argument('--output-dir',type=Path,default=Path('gba/pcm_sfx'))
    a=ap.parse_args()
    old=json.loads(a.base_manifest.read_text()); recs=old['records']
    calbank=a.calibration_bank.read_bytes(); cal=json.loads(a.calibration_manifest.read_text())
    vdir=a.output_dir/'vgm_reference'; vdir.mkdir(parents=True,exist_ok=True)
    bank=bytearray(); rows=[]; stats=[]
    for r in recs:
        sid=int(r['id']); frames=int(r.get('source_duration_frames') or r.get('duration_frames') or 1)
        target_len=round(frames*RATE/60)
        if sid in SILENT:
            pcm=np.zeros(target_len,dtype=np.int8); gain=0.0; raw_rms=0.0; raw_peak=0.0
        else:
            vgm=vdir/f'{sid:02X}_{r["name"]}.vgm'
            subprocess.run([str(a.vgm_renderer),f'{sid:02X}',str(frames),str(vgm)],check=True)
            p=subprocess.run([a.ffmpeg,'-hide_banner','-loglevel','error','-f','libgme','-i',str(vgm),'-ar',str(RATE),'-ac','1','-f','s8','-'],check=True,stdout=subprocess.PIPE)
            raw=signed_bytes(p.stdout)
            if raw.size < target_len: raw=np.pad(raw,(0,target_len-raw.size))
            raw=raw[:target_len]
            raw_rms=float(np.sqrt(np.mean(raw*raw))) if raw.size else 0.0
            raw_peak=float(np.max(np.abs(raw))) if raw.size else 0.0
            cr=cal['records'][sid-FIRST]; oldpcm=signed_bytes(calbank[cr['offset']:cr['offset']+cr['length']])
            old_rms=float(np.sqrt(np.mean(oldpcm*oldpcm))) if oldpcm.size else 0.0
            # Preserve the old bank's broad loudness target, but never obtain it
            # by flattening a real YM transient. Peak headroom wins when the
            # authentic chip render has a higher crest factor.
            rms_gain=(old_rms/raw_rms) if raw_rms>0.01 else 1.0
            peak_gain=(110.0/raw_peak) if raw_peak>0.01 else 1.0
            gain=min(rms_gain,peak_gain)
            y=raw*gain
            pcm=np.clip(np.rint(y),-110,110).astype(np.int8)
        off=len(bank); blob=pcm.tobytes(); bank.extend(blob)
        out_rms=float(np.sqrt(np.mean(pcm.astype(np.float64)**2))) if pcm.size else 0.0
        out_peak=int(np.max(np.abs(pcm.astype(np.int16)))) if pcm.size else 0
        stats.append({'id_hex':r['id_hex'],'name':r['name'],'raw_rms':round(raw_rms,3),'raw_peak':round(raw_peak,3),'gain':round(gain,4),'out_rms':round(out_rms,3),'out_peak':out_peak})
        rows.append({**r,'offset':off,'length':len(blob),'special_loop':sid>=SPECIAL,'expected_silent':sid in SILENT,
            'render_backend':'PSIV source sequencer -> VGM register bridge -> libgme YM2612/SN76489 @31536 Hz'})
    a.output_dir.mkdir(parents=True,exist_ok=True)
    (a.output_dir/'ps4_sfx_bank.pcm').write_bytes(bank)
    manifest={**old,'source':'R22 PSIV source SFX via VGM register bridge and libgme Genesis chip emulation; 31,536 Hz signed-8 PCM',
      'bank_bytes':len(bank),'records':rows,'render_revision':'R22_VGM_LIBGME_SOURCE_CHIP','normalization':'R19 RMS target subject to authentic-transient peak <=110; no hard RMS clipping'}
    (a.output_dir/'ps4_sfx_runtime_manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    (a.output_dir/'R22_VGM_SFX_RENDER_STATS.json').write_text(json.dumps(stats,indent=2)+'\n')
    h=['#ifndef PS4_SFX_BANK_H','#define PS4_SFX_BANK_H','','#include <stdint.h>','',f'#define PS4_SFX_FIRST_ID 0x{FIRST:02X}u',f'#define PS4_SFX_LAST_ID 0x{LAST:02X}u',f'#define PS4_SFX_COUNT {len(rows)}u',f'#define PS4_SFX_BANK_BYTES {len(bank)}u','', 'typedef struct { uint32_t offset; uint32_t length; uint8_t flags; } ps4_sfx_desc_t;','#define PS4_SFX_FLAG_SPECIAL_LOOP 1u','#define PS4_SFX_FLAG_SILENT 2u','', 'static const ps4_sfx_desc_t ps4_sfx_descs[PS4_SFX_COUNT] = {']
    for r in rows:
        flags=(1 if r['special_loop'] else 0)|(2 if r['expected_silent'] else 0);h.append(f'    {{{r["offset"]}u,{r["length"]}u,{flags}u}}, /* {r["id_hex"]} {r["name"]} */')
    h+=['};','','#endif',''];(a.output_dir/'ps4_sfx_bank.h').write_text('\n'.join(h))
    print('R22 VGM/libgme source-chip SFX bank',len(rows),'IDs',len(bank),'bytes')

if __name__=='__main__': main()
