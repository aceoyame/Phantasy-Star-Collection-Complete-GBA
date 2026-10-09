#!/usr/bin/env python3
"""Build frame-aligned PSIV v19 PCM assets with source-derived loop points.

The host-side v19 mastering is decoded with ffmpeg to signed mono 8-bit PCM at
31,536 Hz. Loop metadata comes from the PSIV SMPS source analyzer. Since the
GBA transport commits exactly 528 samples per video frame, loop boundaries are
rounded to the nearest complete DMA frame. A tiny 256-sample endpoint blend
makes that quantization inaudible without touching the musical intro.
"""
from __future__ import annotations
import argparse, json, shutil, subprocess
from pathlib import Path

RATE=31536
FRAME=528
XFADE=256
DEFAULT_IDS=(0x84,0x8C,0x95,0x9F,0xAC)

def nearest_frame(seconds: float) -> int:
    return max(0, int(round(seconds * RATE / FRAME)))

def decode_s8_mono(ffmpeg: str, src: Path) -> bytearray:
    p=subprocess.run([ffmpeg,'-v','error','-i',str(src),'-ac','1','-ar',str(RATE),'-f','s8','-'],check=True,stdout=subprocess.PIPE)
    return bytearray(p.stdout)

def s8(b:int)->int: return b if b < 128 else b-256
def u8(v:int)->int: return v & 255

def main()->None:
    ap=argparse.ArgumentParser()
    ap.add_argument('--mp3-dir',required=True,type=Path)
    ap.add_argument('--loop-manifest',default=Path('tools/ps4_music_loops_phase14.json'),type=Path)
    ap.add_argument('--output-dir',default=Path('gba/pcm_v19'),type=Path)
    ap.add_argument('--ffmpeg',default=shutil.which('ffmpeg') or 'ffmpeg')
    ap.add_argument('--ids',nargs='*',default=[f'{x:02X}' for x in DEFAULT_IDS])
    a=ap.parse_args()
    manifest=json.loads(a.loop_manifest.read_text())
    by_id={int(t['music_id']):t for t in manifest['tracks']}
    a.output_dir.mkdir(parents=True,exist_ok=True)
    rows=[]
    for token in a.ids:
        mid=int(token,16) if isinstance(token,str) else int(token)
        t=by_id[mid]
        if not t.get('exact_capture_loop_ready'):
            raise SystemExit(f'${mid:02X} is not source-global-loop ready')
        candidates=sorted(a.mp3_dir.glob(f'{mid:02X}_*.mp3'))
        if len(candidates)!=1:
            raise SystemExit(f'expected one v19 MP3 for ${mid:02X}, got {candidates}')
        raw=decode_s8_mono(a.ffmpeg,candidates[0])
        start_s=float(t['source_loop_start_seconds_from_music_start'])
        end_s=float(t['source_loop_end_seconds_from_music_start'])
        start_frame=nearest_frame(start_s); end_frame=nearest_frame(end_s)
        if end_frame <= start_frame:
            raise SystemExit(f'bad loop frames for ${mid:02X}')
        loop_offset=start_frame*FRAME; end_offset=end_frame*FRAME
        if end_offset > len(raw):
            raise SystemExit(f'v19 MP3 ${mid:02X} too short: need {end_offset}, have {len(raw)}')
        if loop_offset < XFADE:
            raise SystemExit(f'loop intro too short for endpoint blend ${mid:02X}')
        out=bytearray(raw[:end_offset])
        # Morph only the final ~8.1 ms toward the samples immediately before
        # the chosen loop start. Playback then wraps to loop_start itself.
        for i in range(XFADE):
            ai=end_offset-XFADE+i
            bi=loop_offset-XFADE+i
            a_s=s8(out[ai]); b_s=s8(raw[bi])
            num=i+1
            v=round((a_s*(XFADE-num)+b_s*num)/XFADE)
            out[ai]=u8(max(-128,min(127,v)))
        dst=a.output_dir/f'track_{mid:02X}.pcm'
        dst.write_bytes(out)
        rows.append({
            'music_id':mid,'music_id_hex':f'${mid:02X}','source_mp3':candidates[0].name,
            'sample_rate_hz':RATE,'samples_per_frame':FRAME,
            'source_loop_start_seconds':start_s,'source_loop_end_seconds':end_s,
            'loop_start_frame':start_frame,'loop_end_frame':end_frame,
            'loop_offset_samples':loop_offset,'length_samples':end_offset,
            'actual_loop_start_seconds':loop_offset/RATE,
            'actual_loop_end_seconds':end_offset/RATE,
            'actual_loop_period_seconds':(end_offset-loop_offset)/RATE,
            'endpoint_crossfade_samples':XFADE,
            'global_period_is_lcm':bool(t.get('global_period_is_lcm',False)),
        })
        print(f'${mid:02X}: {dst} intro={loop_offset/RATE:.6f}s period={(end_offset-loop_offset)/RATE:.6f}s bytes={len(out)}')
    report={'format_version':1,'transport':'signed mono s8 31536 Hz; 528 samples/GBA frame','tracks':rows}
    (a.output_dir/'loop_runtime_manifest.json').write_text(json.dumps(report,indent=2)+'\n')
    hdr=['#ifndef PS4_PCM_LOOP_OFFSETS_H','#define PS4_PCM_LOOP_OFFSETS_H','']
    for r in rows:
        hdr.append(f'#define PS4_PCM_TRACK_{r["music_id"]:02X}_LOOP_OFFSET {r["loop_offset_samples"]}u')
        hdr.append(f'#define PS4_PCM_TRACK_{r["music_id"]:02X}_LENGTH {r["length_samples"]}u')
    hdr.extend(['','#endif',''])
    (a.output_dir/'ps4_pcm_loop_offsets.h').write_text('\n'.join(hdr))

if __name__=='__main__': main()
