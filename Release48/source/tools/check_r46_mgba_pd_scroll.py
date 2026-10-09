#!/usr/bin/env python3
"""Linked background scroll against original ROM arithmetic, all 256 angles."""
from pathlib import Path
import argparse,json,struct,sys
R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts'
sys.path.insert(0,str(R/'tools_host/r46_mgba'))
from mgba import GBA,L,run_until_break
ap=argparse.ArgumentParser();ap.add_argument('--label',required=True);a=ap.parse_args()
symbols={}
for line in (R/'build-psc-module/ps4_psc_module.map').read_text().splitlines():
 p=line.split()
 if len(p)>=5:
  try:symbols[p[-1]]=int(p[0],16)
  except ValueError:pass
g=GBA(R.parent/'binaries/RUN_THIS_RELEASE48_PSC.gba')
g.load(O/f'R46_{a.label}_MGBA_DEBUG.bin')
rom=(R/'references/roms/ps4built.bin').read_bytes()
base=0x0203b000;size=0x5000;original=bytes((i*37+11)&255 for i in range(size))
for angle in range(256):
 g.put(base,original);ram=g.mem(0x02000000,0x10000)
 expected=bytearray(original)
 for row in range(120):
  phase=(angle+row*4+64)&255
  value=-rom[0x423cc+((~phase)&127)] if phase&128 else rom[0x423cc+phase]
  struct.pack_into('<h',expected,512+row*2,value//8)
 address=symbols['r46_pd_background_scroll']
 L.mg_break(0xffffffff);L.mg_cpsr(0x3f if address&1 else 0x1f)
 for register,value in [(0,angle),(13,0x03007d80),(14,0x08800000),(15,address&~1)]:L.mg_set_reg(register,value)
 assert run_until_break(g,0x08800000,100),angle
 assert g.mem(base,size)==expected,('panel guard or original scroll mismatch',angle)
 assert g.mem(0x02000000,0x10000)==ram,('MD RAM changed',angle)
report={'core':'upstream mGBA 0.10.5','angles':256,'original_scroll_words':30720,'entire_panel_bytes_guarded_per_case':size,'MD_RAM_bytes_guarded_per_case':65536,'all_match':True,'scope':'linked new scroll function only; independent original ROM signed arithmetic; unrelated scroll, palettes, art and RAM remain guarded'}
(O/f'R46_{a.label}_MGBA_PD_SCROLL.json').write_text(json.dumps(report,indent=2)+'\n')
print('PASS all 256 linked source scroll angles, entire panel and MD RAM guards')
