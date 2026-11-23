#!/usr/bin/env python3
"""Audit captured Academy field OBJ coverage; NOT a fresh emulator run."""
import argparse, collections, hashlib, json, pathlib
def audit(data):
    assert data['snapshots']==7
    records={d['rom']:d['steps'] for d in data['data']}
    assert set(records)=={'stage12','stage14'}
    a,b=records['stage12'],records['stage14']
    assert len(a)==len(b)==7
    coverage=collections.defaultdict(lambda: {'steps':set(),'slots':set(),'maps':set(),'palettes':set(),'tiles':set()})
    for x,y in zip(a,b):
        assert x['step']==y['step'] and x['frame']==y['frame']
        for field in ('map_id','player_xy','screenshot_crc32','sprites'):
            assert x[field]==y[field], (x['step'],field)
        for sp in x['sprites']:
            assert sp['dims']==[16,32]
            h=sp['raw_sha256']; c=coverage[h]
            c['steps'].add(x['step']);c['slots'].add(sp['slot']);c['maps'].add(x['map_id'])
            c['palettes'].add(sp['palette_bank']);c['tiles'].add(sp['tile_index'])
    return {'status':'PASS_EXISTING_PAIR_PARITY_ONLY','snapshots_compared':len(a),
            'sprite_records':sum(len(s['sprites']) for s in a),
            'distinct_obj_tile_hashes':len(coverage),
            'unique_tile_indices':sorted({t for c in coverage.values() for t in c['tiles']}),
            'maps_observed':sorted({s['map_id'] for s in a}),
            'steps_with_no_visible_obj_record':[s['step'] for s in a if not s['sprites']],
            'per_step':[{'step':s['step'],'map':s['map_id'],'frame':s['frame'],
                         'visible_sprite_records':len(s['sprites'])} for s in a],
            'source_pose_identification':'UNVERIFIED',
            'missing_evidence':['Genesis field character sprite pointer/table and animation frame identity',
                                'Actor identity for each OAM slot and pose',
                                'Paired same-action Genesis screenshot and palette-index comparison',
                                'Actual walk-cycle phase coverage beyond sampled checkpoints',
                                'Fresh emulator run for any proposed sprite modification'],
            'note':'Existing saved snapshots, not newly executed emulator frames.'}
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('snapshot_json',type=pathlib.Path);p.add_argument('--output',type=pathlib.Path)
    args=p.parse_args();source=args.snapshot_json.read_bytes();out=audit(json.loads(source))
    out['source_json_sha256']=hashlib.sha256(source).hexdigest()
    report=json.dumps(out,indent=2)+'\n'
    if args.output: args.output.write_text(report)
    print(report)
