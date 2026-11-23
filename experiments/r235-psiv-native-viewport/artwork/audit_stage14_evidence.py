#!/usr/bin/env python3
"""Independent Stage14 artifact/recorded-trace integrity check. Does NOT run emulators."""
import hashlib,json,pathlib,sys,collections
root=pathlib.Path(sys.argv[1]); rom=pathlib.Path(sys.argv[2]).read_bytes()
sha=hashlib.sha256(rom).hexdigest()
assert len(rom)==33554432 and sha=='c1ed9db09505fe2d17322efa9f7c4915e7a4cd57bdf2a3dc6f24d0f8f1d15609'
assert rom[0xB2]==0x96
assert ((-sum(rom[0xA0:0xBD])-0x19)&255)==rom[0xBD]
battle=json.loads((root/'stage14_battle_checks.json').read_text())
rows=battle['results']
assert battle['candidate_sha256']==sha and len(rows)==battle['total_snapshots']==36
groups=collections.Counter((x['core'],x['enemy']) for x in rows)
assert len(groups)==9 and set(groups.values())=={4}
assert all(x['party_and_hud_unchanged'] and x['changed_pixels']>0 for x in rows)
assert all(x['enemy_count']==(3 if 'Xe-A-Thoul' in x['enemy'] else 1) for x in rows)
field=json.loads((root/'stage14_field_ui_regression.json').read_text())
assert set(field)=={'mgba','vbam','delta'}
overworld=json.loads((root/'stage14_overworld_snapshots.json').read_text())
assert len(overworld['data'])==2
a,b=(x['steps'] for x in overworld['data'])
assert len(a)==len(b)==7
assert all(x['screenshot_crc32']==y['screenshot_crc32'] and x['map_id']==y['map_id'] and x['player_xy']==y['player_xy'] for x,y in zip(a,b))
print(json.dumps(dict(rom_sha256=sha,rom_size=len(rom),header_checksum='PASS',recorded_battle_rows=len(rows),recorded_battle_groups=len(groups),recorded_overworld_pairs=len(a),field_core_keys=list(field),limitation='Recorded trace audit only; no emulator run'),indent=2))
