#!/usr/bin/env python3
"""Read-only SHA-gated boss and five-enemy targeting regression guard.
Requires user-owned R235, Stage12 and the R203 complete source archive.
Never embeds copyrighted game data. NOT a playable patch.
"""
from argparse import ArgumentParser
from pathlib import Path
from hashlib import sha256
from struct import unpack_from
from zipfile import ZipFile
import json

BASE = 0x08000000
R235 = '4caf23ae8b030c16a32658fdc95085330de6c6dd4a51498f0e2b9c17c7d89308'
STAGE12 = '5ff6d48606576e19ee46d14d7d8892b9835d4161afebde74f781caf76d38ff02'
ASSET = 0xB8B95C
NATIVE, ANIM, BODY, MAP, PALETTE = [ASSET + x for x in
    (1346048, 1350944, 1646592, 1781984, 1808608)]
BOSSES = (0x82, 0x83, 0x84, 0x85, 0x86, 0x87)

def guard(accepted, candidate, archive):
    assert len(accepted)==len(candidate)==33554432
    assert sha256(accepted).hexdigest()==R235
    assert sha256(candidate).hexdigest()==STAGE12
    with ZipFile(archive) as z:
        old = z.read('PSC_R192_BATTLE_EFFECTS_TEST.gba')
        src = z.read('r192/src/effects.c').decode()
    assert len(old)==33554432
    assert 'int anchors[8]' in src
    assert 'i < ENEMY_COUNT && i < 4' in src
    # Actual Thumb source: cmp r0,#3 ; bls ; movs r0,#120 ; bx lr
    entry=0x087F9AAC-BASE
    sig=bytes.fromhex('032801d978207047')
    assert accepted[entry:entry+len(sig)] == candidate[entry:entry+len(sig)] == sig
    for addr in (0x087F8574, 0x087F868C, 0x087F8708):
        at=addr-BASE
        assert old[at:at+4] == accepted[at:at+4] == candidate[at:at+4]
    bosses=[]
    for eid in BOSSES:
        slot=NATIVE+eid*32
        anim, anim_len, body, body_len, mapping, count, w, h = unpack_from('<8I',accepted,slot)
        assert accepted[slot:slot+32] == candidate[slot:slot+32]
        for off,size in ((ANIM+anim,anim_len),(BODY+body,body_len),
                         (MAP+mapping,count*2),(PALETTE+eid*32,32)):
            assert accepted[off:off+size]==candidate[off:off+size]
        bosses.append(dict(id=f'{eid:02X}',size_px=[w*8,h*8],
                           boss_art_unchanged=True))
    return dict(result='boss assets preserved; fifth target potentially unhandled',
                r235_sha256=R235,stage12_sha256=STAGE12,
                boss_art=bosses,four_enemy_fixture_slots=[8,16,24,32],
                four_enemy_x_centers=[48,96,144,192],
                fifth_enemy_x_helper_return=120,verified_five_enemy_live_test=False,
                limitations='Only four-enemy formation replayed; no confirmed 5-enemy gameplay bug. Do not patch without source-matched fifth encounter.')

if __name__=='__main__':
    p=ArgumentParser(description=__doc__)
    p.add_argument('accepted_r235',type=Path)
    p.add_argument('stage12_rom',type=Path)
    p.add_argument('r203_archive',type=Path)
    p.add_argument('--report',type=Path)
    a=p.parse_args()
    data=guard(a.accepted_r235.read_bytes(),a.stage12_rom.read_bytes(),a.r203_archive)
    print(json.dumps(data,indent=2))
    if a.report:a.report.write_text(json.dumps(data,indent=2)+'\n')
