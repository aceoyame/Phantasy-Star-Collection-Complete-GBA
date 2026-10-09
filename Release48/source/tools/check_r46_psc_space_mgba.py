#!/usr/bin/env python3
"""Release 07 A/B guards for reclaimed padding and original-game startup.

Cold launcher presentation and direct original-game entries are compared with
the previous ROM. Direct entries are initialized by the test host; this does
not certify interactive launcher selection, full playthroughs or hardware.
"""
from pathlib import Path
import argparse,json,struct,sys,zlib
R=Path(__file__).resolve().parents[2];S=R/'source';O=R/'build_artifacts'
ap=argparse.ArgumentParser();ap.add_argument('--baseline-rom',type=Path,required=True);ap.add_argument('--label',default='RELEASE08');a=ap.parse_args()
sys.path.insert(0,str(S/'tools_host/r46_mgba'));from mgba import GBA
sys.path.insert(0,str(S/'tools'));from psc_rom_space import check_reclaimed_padding
current=R/'binaries/RUN_THIS_RELEASE48_PSC.gba';old=a.baseline_rom.read_bytes();new=current.read_bytes()
assert f'{zlib.crc32(old):08x}'=='979c0c64'
regions=check_reclaimed_padding(old[:0x800000],new[:0x800000])
entries=[struct.unpack_from('<I',old,0x1883c+4*i)[0] for i in range(3)]
assert entries==[0x08738000,0x08390000,0x08080000]
def snapshot(g):
    return (g.shot().tobytes(),g.mem(0x02000000,0x40000),g.mem(0x03000000,0x8000),
            g.mem(0x06000000,0x18000),g.mem(0x05000000,1024),g.mem(0x07000000,1024),
            g.mem(0x0e000000,0x8000),[g.reg(i) for i in range(16)],g.cycles())
rows=[]
for name,entry in [('PSC cold startup',0x08000000),*zip(('PSI','PSII','PSIII'),entries)]:
    controls=[]
    for which,rom in (('baseline',a.baseline_rom),('current',current)):
        g=GBA(rom,entry=entry)
        times=(120,600,1200) if name!='PSC cold startup' else (240,1200,2400)
        last=0
        for frames in times:
            g.run(frames-last);last=frames;got=snapshot(g)
            if which=='baseline':controls.append(got)
            else:assert got==controls[times.index(frames)],(name,frames,'startup differs')
        if name!='PSC cold startup':
            g.press('START',3,300);g.press('A',3,300);got=snapshot(g)
            if which=='baseline':controls.append(got)
            else:assert got==controls[-1],(name,'post-input startup differs')
        if which=='current':g.shot(str(O/('R46_'+a.label+'_PSC_'+name.replace(' ','_')+'.png')),2)
    rows.append({'route':name,'entry':hex(entry),'frame_samples':list(times),
                 'additional_START_A_frames':606 if name!='PSC cold startup' else 0,
                 'whole_frame_RAM_VRAM_palette_OAM_SRAM_CPU_and_cycles_identical':True})
    print('PASS',name,'A/B',flush=True)
report={'baseline_ROM_crc32':'979c0c64','ROM_crc32':f'{zlib.crc32(new):08x}',
        'all_PSC_bytes_outside_verified_padding_identical':True,'regions':regions,'routes':rows,
        'interactive_launcher_original_game_playthrough_or_physical_hardware_verified':False}
(O/('R46_'+a.label+'_MGBA_PSC_SPACE.json')).write_text(json.dumps(report,indent=2)+'\n')
print('PASS original-game padding and startup guards',flush=True)
