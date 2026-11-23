#!/usr/bin/env python3
"""Read-only, SHA-guarded Stage14 triple-target and Academy trace consistency gate.

Checks recorded evidence, NOT a fresh emulator run or Genesis fidelity.
"""
import argparse, hashlib, json, zipfile
from pathlib import Path
EXPECTED_ROM='c1ed9db09505fe2d17322efa9f7c4915e7a4cd57bdf2a3dc6f24d0f8f1d15609'
EXPECTED_PARENT='5ff6d48606576e19ee46d14d7d8892b9835d4161afebde74f781caf76d38ff02'
CORES={'mgba','vbam','delta'}
ORDER=['initial','open','pick','right1','right2','left1']
TARGETS=[None,None,54,120,186,120]
def validate(archive,rom):
    with zipfile.ZipFile(archive) as z:
        def load(name): return json.loads(z.read(name))
        target=load('stage14_xeathoul_targeting.json')
        world=load('stage14_overworld_snapshots.json')
        field=load('stage14_field_ui_regression.json')
    blob=Path(rom).read_bytes()
    assert len(blob)==33554432
    assert hashlib.sha256(blob).hexdigest()==EXPECTED_ROM
    assert set(field)==CORES
    assert target['matched_core_replays']==6
    runs=target['steps']
    assert len(runs)==3 and {r['core'] for r in runs}==CORES
    for run in runs:
        steps=run['steps']
        assert len(steps)==run['step_count']==6
        assert [s['checkpoint'] for s in steps]==ORDER
        assert [s['red_marker_mid_x'] for s in steps]==TARGETS
        assert [s['frame'] for s in steps]==[1855,1869,1883,1895,1907,1919]
        assert all(s['enemy_count']==3 and s['battle_state']==14 for s in steps)
        assert all(s['red_marker_y']==[31,32,33,34,35] for s in steps[2:])
    byname={r['rom']:r for r in world['data']}
    assert set(byname)=={'stage12','stage14'}
    assert byname['stage12']['sha256']==EXPECTED_PARENT
    assert byname['stage14']['sha256']==EXPECTED_ROM
    a,b=(byname[k]['steps'] for k in ('stage12','stage14'))
    assert len(a)==len(b)==7
    for left,right in zip(a,b):
        for key in ('step','frame','map_id','player_xy','screenshot_crc32','sprites'):
            assert left[key]==right[key],f'Overworld divergence {left["step"]} {key}'
        for sprite in left['sprites']:
            assert sprite['dims']==[16,32]
            assert len(sprite['raw_sha256'])==64
    return {'status':'RECORDED_EVIDENCE_CONSISTENT','sha256':EXPECTED_ROM,
            'cores':sorted(CORES),'target_steps_per_core':6,'overworld_pairs':7,
            'not_proven':['new emulator run','Genesis pose mapping','FIN','GBA Micro']}
if __name__=='__main__':
    p=argparse.ArgumentParser()
    p.add_argument('qa_zip');p.add_argument('stage14_rom');a=p.parse_args()
    print(json.dumps(validate(a.qa_zip,a.stage14_rom),indent=2))
