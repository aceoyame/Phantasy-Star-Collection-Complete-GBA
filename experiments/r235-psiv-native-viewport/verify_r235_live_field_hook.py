#!/usr/bin/env python3
"""Compare R235 against the guarded field-hook canary at a real Academy state.
Requires the user's R235 overnight QA ZIP, standard mGBA headless toolkit, and ROMs.
Run with --baseline, --candidate, --toolkit, --qa-archive.
"""
import argparse, hashlib, json, os, subprocess, sys, tempfile, zlib
from pathlib import Path
from zipfile import ZipFile
SHA='4caf23ae8b030c16a32658fdc95085330de6c6dd4a51498f0e2b9c17c7d89308'
STATE='gba/after_principal_basement_settled/settled.state'
ACTIONS=[('',45),('RIGHT',30),('',15),('UP',30),('',20),('LEFT',35),('',20),('DOWN',35),('',20),('A',2),('',90)]
ENTRY=0x08CD3854;ORIGINAL=0x08CB9F00;RELOCATED=0x08F97D64
def sample(rom,state,toolkit):
    os.environ['CP08_PROBE_LIB']=str(toolkit/'qa/probe/probe.so')
    sys.path[:0]=[str(toolkit/'qa/probe')]
    from mgba2 import GBA,L
    g=GBA(rom);g.load(state)
    assert g.rd32(0x02024068)&255==18,'wrong starting map'
    frames=[]
    for keys,n in ACTIONS:
        g.run(n,keys)
        frames.append(dict(keys=keys,frames=n,screen_crc32='%08x'%zlib.crc32(g.shot().tobytes()),
                           map_id=g.rd32(0x02024068)&255,
                           pos=[g.rd32(0x020240e4)//8,g.rd32(0x020240e8)//8]))
    L.mg_pprof_init();g.pprof(2000000)
    profile={hex(a):int(L.mg_pprof_get(0,(a-0x08000000)//4)) for a in (ENTRY,ORIGINAL,RELOCATED)}
    return dict(frames=frames,profile=profile)
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for n in ('baseline','candidate','toolkit','qa_archive','state','rom'):p.add_argument('--'+n.replace('_','-'),type=Path)
    p.add_argument('--sample',action='store_true')
    p.add_argument('--output',type=Path)
    a=p.parse_args()
    if a.sample:
        print(json.dumps(sample(a.rom,a.state,a.toolkit)))
        return
    assert all((a.baseline,a.candidate,a.toolkit,a.qa_archive))
    assert hashlib.sha256(a.baseline.read_bytes()).hexdigest()==SHA
    with tempfile.TemporaryDirectory() as tmp:
        state=Path(tmp)/'academy.state'
        with ZipFile(a.qa_archive) as z:state.write_bytes(z.read(STATE))
        def read(rom):
            args=[sys.executable,__file__,'--baseline',str(a.baseline),'--candidate',str(a.candidate),
                  '--toolkit',str(a.toolkit),'--qa-archive',str(a.qa_archive),'--state',str(state),
                  '--rom',str(rom),'--sample']
            return json.loads(subprocess.check_output(args,text=True))
        normal=read(a.baseline);test=read(a.candidate)
    assert normal['frames']==test['frames'],'visual/game-state regression'
    assert normal['profile'][hex(ORIGINAL)]>0 and normal['profile'][hex(RELOCATED)]==0
    assert test['profile'][hex(RELOCATED)]>0 and test['profile'][hex(ORIGINAL)]==0
    result={'status':'PASS','type':'field-camera no-op relocation',
            'checkpoints':len(ACTIONS),'parent_sha256':SHA,
            'candidate_sha256':hashlib.sha256(a.candidate.read_bytes()).hexdigest(),
            'source_profile':normal['profile'],'candidate_profile':test['profile'],
            'snapshots':test['frames'],
            'limitation':'camera function rerouted, not an alternate visible viewport'}
    if a.output:a.output.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))
if __name__=='__main__': main()
