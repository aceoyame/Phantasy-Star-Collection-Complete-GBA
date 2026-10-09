#!/usr/bin/env python3
"""Byte-exact cartridge/runtime preservation and linked space checks."""
from pathlib import Path
import json,subprocess,zlib
R=Path(__file__).resolve().parents[2];V=R.parent/'validation_cp28'
before=(R/'binaries/RUN_THIS_R48_CP27_NATIVE_KERNING.gba').read_bytes();after=(R/'binaries/RUN_THIS_R48_CP28_NATIVE_GLYPH.gba').read_bytes();assert len(after)==33554432
preserved={}
for label,a,b in [('launcher',0,0x48000),('FM_bank',0x48000,0x60000),('PSII_III_IV',0x80000,0x6ca500),('PSC8_runtime',0x6ca500,0x6dbef8)]:
 assert before[a:b]==after[a:b],label;preserved[label]={'start':hex(a),'end':hex(b),'identical':True}
sy={q[2]:int(q[0],16) for line in subprocess.check_output(['llvm-nm',str(R/'source/additional/build/md.elf')],text=True).splitlines() if len(q:=line.split())==3}
assert sy['__hot_end']<=0x03007980 and sy['__bss_end']<=0x0203e000 and sy['__rom_end']<=0x08730000
report=json.loads((R/'build_artifacts/CP28_BUILD_REPORT.json').read_text());assert report['CRC32']==f'{zlib.crc32(after):08x}'
d={'all_pass':True,'reference':'CP27 535AAE54','size':len(after),'CRC32':f'{zlib.crc32(after):08X}','preserved':preserved,'IWRAM_end':hex(sy['__hot_end']),'IWRAM_spare_bytes':0x03007980-sy['__hot_end'],'EWRAM_end':hex(sy['__bss_end']),'EWRAM_spare_bytes':0x0203e000-sy['__bss_end'],'MD_ROM_end':hex(sy['__rom_end']),'MD_ROM_spare_bytes':0x08730000-sy['__rom_end']}
(V/'PRESERVATION_AND_SPACE.json').write_text(json.dumps(d,indent=2)+'\n');print(d)
