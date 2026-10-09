#!/usr/bin/env python3
"""Build 12.5% zero-runtime wave preconditioning with bounded RMS retention."""
from pathlib import Path
import argparse,json,sys,zlib
R=Path(__file__).resolve().parent
ap=argparse.ArgumentParser(); ap.add_argument('--base',type=Path,default=R.parent/'binaries/RUN_THIS_RELEASE48_PSC.gba'); ap.add_argument('--out',type=Path,default=R.parent/'binaries/RUN_THIS_RELEASE48_PS23_AUDIO_W12R.gba'); a=ap.parse_args()
sys.path.insert(0,str(R/'tools'))
from release48_native_wave_smooth import apply as smooth
from release48_native_audio_quality import apply as quantize
raw=a.base.resolve().read_bytes()
if len(raw)!=0x2000000: raise SystemExit(f'expected 32 MiB Release 48 ROM, got {len(raw):#x}')
shell,wave_report=smooth(raw[:0x800000],1,8,preserve_rms=True,gain_cap=1.08); shell=quantize(shell); out=shell+raw[0x800000:]
a.out=a.out.resolve(); a.out.parent.mkdir(parents=True,exist_ok=True); a.out.write_bytes(out)
report={'base_crc32':f'{zlib.crc32(raw)&0xffffffff:08x}','candidate_crc32':f'{zlib.crc32(out)&0xffffffff:08x}','size_bytes':len(out),'scope':'PSII/PSIII native source waves only (12.5% forward linear-amplitude preblend + bounded per-wave RMS retention) + signed round-to-nearest final quantizer; no runtime interpolation helper; PSIV untouched','wave_patch':wave_report}
(R.parent/'build_artifacts/RELEASE48_PS23_AUDIO_W12R_PATCH.json').write_text(json.dumps(report,indent=2)+'\n')
print('PASS Release 48 PSII/PSIII AudioW12R',a.out,report['candidate_crc32'])
