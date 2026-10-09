#!/usr/bin/env python3
from pathlib import Path
import sys,json,zlib,struct
R=Path(__file__).resolve().parents[2];sys.path.insert(0,str(R/'source/tools_host/r46_mgba'));from mgba import GBA,L,run_until_break
rom=R/'binaries/RUN_THIS_RELEASE48_PSC.gba';data=rom.read_bytes();report={'rom_crc32':f'{zlib.crc32(data):08x}','logos_reclaimed_bytes':0x9cac-0x4170,'title_frame':60,'games':[]}
g=GBA(rom,entry=0x08000000);g.run(60);g.shot(R/'build_artifacts/RELEASE48_PSC_TITLE.png')
# Stop exactly after title -> card screen initialization, release START before
# the separate menu confirm handler observes it.
g.run(60);L.mg_break(0x080005f0);g.run(10,'START');g.run(900);assert L.mg_hit();L.mg_break(0xffffffff);g.run(40);g.shot(R/'build_artifacts/RELEASE48_PSC_MENU.png');g.save(str(R/'build_artifacts/RELEASE48_LAUNCHER_MENU.state'))
for i,entry in enumerate([0x08738000,0x08390000,0x08080000,0x08041800]):
 g.load(str(R/'build_artifacts/RELEASE48_LAUNCHER_MENU.state'))
 for n in range(i):g.press('DOWN',1,40)
 assert g.rd32(0x0202436c)==i,(i,g.rd32(0x0202436c))
 g.shot(R/'build_artifacts'/f'RELEASE48_CARD_{i+1}.png')
 L.mg_break(entry);g.run(10,'A');g.run(900);assert L.mg_hit(),(i,hex(g.reg(15)))
 L.mg_break(0xffffffff);g.run(500);g.shot(R/'build_artifacts'/f'RELEASE48_LAUNCH_{i+1}.png')
 report['games'].append({'slot':i+1,'selected_index':i,'native_entry':hex(entry),'launcher_handoff_pass':True,'frames_after_launch':500})
(R/'build_artifacts/RELEASE48_LAUNCHER.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS title at 60 frames, four card selections and native handoffs')
