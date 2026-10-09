#!/usr/bin/env python3
"""Matching-ROM first-open/cache menu diagnostics, HLE not hardware cycles."""
from pathlib import Path
import argparse,ctypes,json,re,sys
R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts'
sys.path.insert(0,str(R/'tools_host/r46_gba_emu'))
from gba import GBA,L,run_until_break
ap=argparse.ArgumentParser();ap.add_argument('--prefix',required=True);a=ap.parse_args();prefix='R46_'+a.prefix
s={}
for line in (R/'build-psc-module/ps4_psc_module.map').read_text().splitlines():
 p=line.split()
 if len(p)>=5:
  try:s[p[-1]]=int(p[0],16)
  except ValueError:pass
g=GBA(str(R.parent/'binaries/RUN_THIS_RELEASE48_PSC.gba'),entry=0x08800000);g.load(str(O/(prefix+'_DEBUG_field.bin')))
def call(name,args=()):
 for i,v in enumerate(args):L.gba_set_reg(i,v)
 for i,v in [(13,0x03007d80),(14,0x08800000),(15,s[name])]:L.gba_set_reg(i,v)
 f=g.frame();ins=L.gba_insns();assert run_until_break(g,0x08800000,500),name
 return {'display_frames':g.frame()-f,'instructions':int(L.gba_insns()-ins)}
x,y=g.rd32(s['ps4_menu_camx']),g.rd32(s['ps4_menu_camy'])
g.wr8(s['ps4_menu_base_cached'],0)
first=call('ps4_ui_clear_field',(x,y))
cached=call('ps4_ui_clear_field',(x,y));g.shot(str(O/(prefix+'_MENU_BACKDROP.png')),scale=2)
r={'first_snapshot':first,'cached_snapshot':cached,'camera':[x,y],'scope':'Fresh debug field matching this ROM; native snapshot/cache calls. Bundled HLE is not cycle accurate and has no audio output; input/submenu/TALK end-to-end times remain open.'}
(O/(prefix+'_MENU_PROFILE.json')).write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r),flush=True)
