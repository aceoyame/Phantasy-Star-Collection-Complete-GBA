#!/usr/bin/env python3
"""Compare generated PCM from the actual ARM/Thumb ROM builds, before FIFO playback."""
from pathlib import Path
import json,os,subprocess,sys,tempfile
R=Path(__file__).resolve().parents[2]
if len(sys.argv)>1:
    tag,index,command,out=sys.argv[1:];index=int(index);command=int(command,0)
    sys.path.insert(0,str(R/'source/tools_host/cp09_prof'))
    from gtest import boot,L,sym
    g=boot(R/f'binaries/RUN_THIS_R48_{tag}_TEST.gba',index)
    entry=sym('mdsnd_frame');zram=sym('md_zram');raw=bytearray()
    for frame in range(500):
        L.mg_break(entry);g.run(100);assert L.mg_hit(),('sound entry',frame)
        left,right=g.reg(0),g.reg(1);ret=g.reg(14)&~1
        if frame==120:g.wr8(zram+0x1c0a,command)
        L.mg_break(ret);g.run(100);assert L.mg_hit(),('sound return',frame)
        raw+=g.mem(left,224)+g.mem(right,224)
        # Advance past the current return before waiting for the next entry.
    Path(out).write_bytes(raw)
    sys.exit(0)
rows=[]
with tempfile.TemporaryDirectory(prefix='cp13-gba-pcm-') as tmp:
    for index,command in [(2,0x81),(9,0x84)]:
        paths=[]
        for tag in ['CP11','CP15']:
            p=Path(tmp)/f'{index}_{tag}.pcm';paths.append(p)
            elf=R/'source/references/cp11_speed/md.elf' if tag=='CP11' else R/'source/additional/build/md.elf'
            subprocess.run([sys.executable,__file__,tag,str(index),hex(command),str(p)],env=dict(os.environ,MD_ELF=str(elf)),check=True,capture_output=True)
        a,b=[p.read_bytes() for p in paths];same=a==b
        rows.append({'index':index,'command':hex(command),'sound_frames':500,'bytes_compared':len(a),'identical':same})
        print(f'GBA PCM index {index}: {"exact" if same else "FAIL"}',flush=True)
result={'all_pass':all(r['identical'] for r in rows),'cases':rows}
(R.parent/'validation_cp15/GBA_PCM_EXACT.json').write_text(json.dumps(result,indent=2)+'\n')
assert result['all_pass']
