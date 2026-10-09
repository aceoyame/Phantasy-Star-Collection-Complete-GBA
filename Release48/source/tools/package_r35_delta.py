#!/usr/bin/env python3
"""Make and byte-verify a compact R34-to-R35 ROM recovery package.
Exact byte matching only; no digests or third-party patch dependency.
"""
from pathlib import Path
import argparse,struct,subprocess,tempfile,zipfile,json
R=Path(__file__).resolve().parents[1]
ap=argparse.ArgumentParser();ap.add_argument('--baseline',type=Path,required=True);ap.add_argument('--out',type=Path,required=True);a=ap.parse_args()
old=a.baseline.read_bytes();new=(R.parent/'binaries/RUN_THIS_R35_PSC_WREN_CRASH.gba').read_bytes()
assert len(old)==len(new)==0x2000000 and old[:0x800000]==new[:0x800000]
# Fixed byte keys locate immutable banks even when their ROM offsets move.
index={}
for i in range(0x800000,len(old)-63,32):index.setdefault(old[i:i+64],i)
patch=bytearray(b'PSC-R35-DLT1'+struct.pack('<I',len(new)));patch+=b'C'+struct.pack('<II',0,0x800000)
i=start=0x800000;copies=1;literal=0
while i+64<=len(new):
 match=index.get(new[i:i+64])
 if match is None:i+=1;continue
 if start<i:
  patch+=b'L'+struct.pack('<I',i-start)+new[start:i];literal+=i-start
 n=64
 while i+n+4096<=len(new) and match+n+4096<=len(old) and new[i+n:i+n+4096]==old[match+n:match+n+4096]:n+=4096
 while i+n<len(new) and match+n<len(old) and new[i+n]==old[match+n]:n+=1
 patch+=b'C'+struct.pack('<II',match,n);copies+=1;i+=n;start=i
if start<len(new):patch+=b'L'+struct.pack('<I',len(new)-start)+new[start:];literal+=len(new)-start
patch+=b'E'
samples=b''.join(old[i:i+32] for i in range(0,len(old),65536))
readme='''R35 ROM recovery (Python standard library only)

1. Unzip this package to a folder.
2. Put the saved PSIV_R34_ROM_2026-09-30.zip in that folder, or provide its full path.
3. Run: python3 restore_r35.py PSIV_R34_ROM_2026-09-30.zip

The result is RUN_THIS_R35_PSC_WREN_CRASH.gba, a 32 MiB PSC ROM.
You may instead supply the unzipped R34 GBA ROM. No source compiler is needed.

The reconstruction was compared byte for byte with the final R35 build.
The storage limit prevented saving a second full ROM archive; this small
package preserves the exact result using the already saved R34 ROM.
R35 adds Wren and crash/Raja scenes. InsideSpaceship and SpaceshipSabotage
remain route blockers. No emulator/hardware playthrough is certified.
See R35_NOTES_2026-09-30.md for detailed validation and remaining work.
'''
a.out.parent.mkdir(parents=True,exist_ok=True)
with zipfile.ZipFile(a.out,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
 z.write(R/'tools/restore_r35.py','restore_r35.py');z.writestr('r35_delta.bin',patch);z.writestr('r34_samples.bin',samples);z.writestr('README.txt',readme)
with tempfile.TemporaryDirectory() as folder:
 p=Path(folder)
 with zipfile.ZipFile(a.out) as z:assert z.testzip() is None;z.extractall(p)
 subprocess.run(['python3',str(p/'restore_r35.py'),str(a.baseline.resolve()),'--out',str(p/'restored.gba')],check=True)
 assert (p/'restored.gba').read_bytes()==new
report={'baseline':'RUN_THIS_R34_PSC_STORY_CONTINUATIONS.gba','output':'RUN_THIS_R35_PSC_WREN_CRASH.gba','byte_exact_reconstruction_verified':True,'rom_bytes':len(new),'copy_records':copies,'literal_bytes':literal,'package_bytes':a.out.stat().st_size,'hashes':False,'third_party_dependencies':False}
(R.parent/'build_artifacts/R35_ROM_RECOVERY.json').write_text(json.dumps(report,indent=2)+'\n')
print('PASS exact reconstruction:',report)
