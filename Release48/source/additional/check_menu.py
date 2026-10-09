from pathlib import Path
import argparse,sys,json,zlib
R=Path(__file__).resolve().parents[2];sys.path.insert(0,str(R/'source/tools_host/r46_mgba'))
from mgba import GBA,L
ap=argparse.ArgumentParser();ap.add_argument('--rom',type=Path,default=R/'binaries/R48_ADDITIONAL_CONTENT_PREVIEW.gba');a=ap.parse_args()
rom=a.rom.resolve();O=R/'build_artifacts';g=GBA(rom,entry=0x08000000)
g.run(120);L.mg_break(0x080005f0);g.run(10,'START');g.run(900);assert L.mg_hit();L.mg_break(0xffffffff);g.run(50)
g.save(str(O/'ADDITIONAL_LAUNCHER.state'));rows=[]
for i in range(5):
 g.load(str(O/'ADDITIONAL_LAUNCHER.state'))
 for _ in range(i):g.press('DOWN',1,60)
 assert g.rd32(0x0202436c)==i,(i,g.rd32(0x0202436c))
 g.shot(O/f'ADDITIONAL_CARD_{i+1}.png')
 visible_entry=[0x08004f00,0x08390000,0x08080000,0x08041800,0x08005000][i]
 target=0x086ca500 if i==0 else visible_entry
 L.mg_break(target);g.run(10,'A');g.run(500);assert L.mg_hit(),(i,hex(g.reg(15)));L.mg_break(0xffffffff)
 if i==0:
  cfg=[g.rd32(0x0203ff00+x*4) for x in range(4)]
  assert cfg[0]==0x082d3b80 and cfg[1]==0x80000 and cfg[2]==10,cfg
  rows.append({'slot':1,'entry':hex(visible_entry),'engine':hex(target),'main_PSI_config':[hex(x) for x in cfg],'launch':True})
 else: rows.append({'slot':i+1,'entry':hex(visible_entry),'launch':True})
 g.run(80)
 if i==4:g.shot(O/'ADDITIONAL_SUBMENU.png');g.save(str(O/'ADDITIONAL_SUBMENU.state'))
g.load(str(O/'ADDITIONAL_LAUNCHER.state'));g.press('UP',1,70);assert g.rd32(0x0202436c)==4;g.press('DOWN',1,70);assert g.rd32(0x0202436c)==0
for i in range(9):
 g.load(str(O/'ADDITIONAL_SUBMENU.state'))
 for _ in range(i):g.press('DOWN',1,10)
 entry=0x086ca500 if i<2 else 0x086e2000;L.mg_break(entry);g.run(5,'A');assert L.mg_hit(),('cart handoff',i,hex(g.reg(15)));L.mg_break(0xffffffff);g.run(900)
 g.shot(O/f'ADDITIONAL_CART_{i}.png');diag=[hex(g.rd32(0x0203e000+x*4)) for x in range(4)];print(i,diag,flush=True);rows.append({'cart':i,'entry':hex(entry),'diagnostics_after_900_display_frames':diag})
g.load(str(O/'ADDITIONAL_SUBMENU.state'));g.press('B',1,80);g.shot(O/'ADDITIONAL_RETURN.png')
(O/'ADDITIONAL_MENU_CHECK.json').write_text(json.dumps({'CRC32':f'{zlib.crc32(rom.read_bytes()):08x}','results':rows,'physical_hardware_or_playthrough_verified':False},indent=2)+'\n');print('PASS five collection entries, main PSI retranslation/FM handoff and nine additional cartridge handoffs')
