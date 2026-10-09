#!/usr/bin/env python3
"""Generate the PSIV source-backed SFX PCM bank for the GBA static runtime.

The input WAVs are the source-driver renders audited against PSIV's SFX pointer
order.  The static GBA runtime uses signed mono 8-bit PCM at the same 31,536 Hz
clock as the v18 AOT music path.  IDs remain the original PSIV $B5-$FA values.
"""
from __future__ import annotations
import argparse, json, shutil, subprocess
from pathlib import Path

RATE=31536
FIRST=0xB5
LAST=0xFA
SPECIAL_FIRST=0xF8
SILENT={0xF6,0xF7}

def decode(ffmpeg:str,src:Path)->bytes:
    p=subprocess.run([ffmpeg,'-v','error','-i',str(src),'-ac','1','-ar',str(RATE),'-f','s8','-'],
                     check=True,stdout=subprocess.PIPE)
    return p.stdout

def main()->None:
    ap=argparse.ArgumentParser()
    ap.add_argument('--wav-dir',required=True,type=Path)
    ap.add_argument('--source-report',required=True,type=Path)
    ap.add_argument('--output-dir',default=Path('gba/pcm_sfx'),type=Path)
    ap.add_argument('--ffmpeg',default=shutil.which('ffmpeg') or 'ffmpeg')
    a=ap.parse_args()
    report=json.loads(a.source_report.read_text())
    recs=report['records']
    if len(recs)!=(LAST-FIRST+1) or [int(r['id']) for r in recs]!=list(range(FIRST,LAST+1)):
        raise SystemExit('source report is not exact contiguous PSIV $B5-$FA coverage')
    a.output_dir.mkdir(parents=True,exist_ok=True)
    bank=bytearray(); rows=[]
    for r in recs:
        sid=int(r['id']); name=r['name']
        src=a.wav_dir/f'ps4_sfx_{name}.wav'
        if not src.is_file(): raise SystemExit(f'missing {src}')
        pcm=decode(a.ffmpeg,src)
        # Keep exact resampled length.  The frame mixer handles an effect ending
        # in the middle of a 528-sample frame without padding the ROM bank.
        off=len(bank); bank.extend(pcm)
        rows.append({
            'id':sid,'id_hex':f'${sid:02X}','name':name,
            'source_label':r.get('source_label',''),
            'offset':off,'length':len(pcm),
            'special_loop':sid>=SPECIAL_FIRST,
            'expected_silent':sid in SILENT,
            'source_duration_frames':int(r.get('duration_frames',0)),
        })
    (a.output_dir/'ps4_sfx_bank.pcm').write_bytes(bank)
    manifest={
        'format_version':1,
        'source':'PSIV source-driver SFX renders; IDs are PSIV constants $B5-$FA',
        'sample_format':'signed mono 8-bit PCM',
        'sample_rate_hz':RATE,
        'first_id':FIRST,'last_id':LAST,'count':len(rows),
        'bank_bytes':len(bank),'records':rows,
    }
    (a.output_dir/'ps4_sfx_runtime_manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    h=['#ifndef PS4_SFX_BANK_H','#define PS4_SFX_BANK_H','','#include <stdint.h>','',
       f'#define PS4_SFX_FIRST_ID 0x{FIRST:02X}u',f'#define PS4_SFX_LAST_ID 0x{LAST:02X}u',
       f'#define PS4_SFX_COUNT {len(rows)}u',f'#define PS4_SFX_BANK_BYTES {len(bank)}u','',
       'typedef struct { uint32_t offset; uint32_t length; uint8_t flags; } ps4_sfx_desc_t;',
       '#define PS4_SFX_FLAG_SPECIAL_LOOP 1u','#define PS4_SFX_FLAG_SILENT 2u','',
       'static const ps4_sfx_desc_t ps4_sfx_descs[PS4_SFX_COUNT] = {']
    for r in rows:
        flags=(1 if r['special_loop'] else 0)|(2 if r['expected_silent'] else 0)
        h.append(f'    {{{r["offset"]}u,{r["length"]}u,{flags}u}}, /* {r["id_hex"]} {r["name"]} */')
    h += ['};','','#endif','']
    (a.output_dir/'ps4_sfx_bank.h').write_text('\n'.join(h))
    print(f'PSIV SFX bank: {len(rows)} IDs, {len(bank)} bytes ({len(bank)/1048576:.3f} MiB)')

if __name__=='__main__': main()
