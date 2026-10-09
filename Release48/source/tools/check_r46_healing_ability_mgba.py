#!/usr/bin/env python3
"""Current-ROM native healing ability commands and actual camp TECH/SKILL menus.

Reuse only a CRC-identified state from a fresh current-ROM native boot.
Controlled fixtures do not certify normal playthrough or animation parity.
"""
from pathlib import Path
import argparse, json, sys, zlib

S = Path(__file__).resolve().parents[1]
O = S.parent / 'build_artifacts'
ap = argparse.ArgumentParser()
ap.add_argument('--label', default='CP22')
a = ap.parse_args()
prefix = 'R46_' + a.label
sys.path.insert(0, str(S / 'tools_host/r46_mgba'))
from mgba import GBA, L, run_until_break
symbols = {}
for line in (S / 'build-psc-module/ps4_psc_module.map').read_text().splitlines():
    p = line.split()
    if len(p) >= 5:
        try:
            symbols[p[-1]] = int(p[0], 16)
        except ValueError:
            pass
rom = S.parent / 'binaries/RUN_THIS_RELEASE48_PSC.gba'
state = O / (prefix + '_MGBA_DEBUG.bin')
rom_id = {'rom_bytes': rom.stat().st_size, 'rom_crc32': f'{zlib.crc32(rom.read_bytes()):08x}'}
assert json.loads(state.with_suffix('.rom_crc.json').read_text()) == rom_id, 'run a fresh matching native boot first'
g = GBA(rom)
md, pp, ee, cp, tp = 0x02000000, 0x02033600, 0x02033800, 0x02033980, 0x02033990
original_rom = (S / 'references/roms/ps4built.bin').read_bytes()
results = []

def w16(off, value):
    g.put(md + off, value.to_bytes(2, 'big'))

def w32(off, value):
    g.put(md + off, value.to_bytes(4, 'big'))

def read16(data, off):
    return int.from_bytes(data[off:off + 2], 'big')

def write16(data, off, value):
    data[off:off + 2] = value.to_bytes(2, 'big')

def begin(name, args=()):
    L.mg_break(0xffffffff)
    L.mg_cpsr(0x1f)
    for i, value in enumerate(args[:4]):
        L.mg_set_reg(i, value)
    for i, value in enumerate(args[4:]):
        g.wr32(0x03007d80 + i * 4, value)
    for i, value in [(13, 0x03007d80), (14, 0x08800000), (15, symbols[name])]:
        L.mg_set_reg(i, value)

def call(name, args=()):
    begin(name, args)
    assert run_until_break(g, 0x08800000, 300), name
    return g.reg(0)

def press(key, hold=3, after=35):
    L.mg_break(0xffffffff)
    g.press(key, hold, after)

def shot(name):
    g.shot(str(O / (prefix + '_ABILITY_' + name + '.png')), 2)

def fixture(item, specs, slot=0):
    g.load(state)
    g.wr8(symbols['ps4_menu_overlay_active'], 0)
    g.wr8(symbols['battle_suppress_skill_source_audio'], 0)
    g.put(md + 0xf40a, bytes([cid for cid, _, _ in specs]) + bytes([0xff]) * (5 - len(specs)))
    inventory = bytearray([1 + i % 30 for i in range(40)])
    inventory[slot] = item
    g.put(md + 0xf410, inventory)
    for i, (cid, hp, status) in enumerate(specs):
        d = 0xf500 + cid * 128
        w16(d + 6, 5 if cid in (6, 7) else 0)
        w16(d + 14, hp)
        w16(d + 16, 100)
        g.wr8(md + d + 22, status)
        g.wr8(md + d + 31, 33 + i)
        g.wr8(md + d + 34, 44 + i)
        g.wr8(md + d + 32, 2)
        g.wr8(md + d + 35, 4)
        call('battle_load_char', (cid, pp + i * 62))
        # Deliberately unrelated caster Mental makes wrong item arithmetic obvious.
        g.wr8(pp + i * 62 + 24, 255)
    call('battle_load_enemy', (9, ee))
    g.wr32(symbols['battle_enemy_count'], 1)
    g.wr8(symbols['battle_event_index'], 1)
    call('battle_anim_reset')
    call('battle_ui_bind', (pp, len(specs), ee, 0))
    call('r45_pd_visual_reset', (ee,))
    call('battle_compose_igg', (pp, len(specs), ee, 0, 0))
    call('battle_present_page0')
    w32(0xef0c, 0x12345678)
    return bytes(inventory)


def ability_fixture(skill, id_, specs, uses=1, tp_value=None, max_hp=1000):
    inventory = fixture(1, specs)
    p = (0x2a9d28 if skill else 0x2a9be8) + (id_ - 1) * 8
    cost = original_rom[p + 1]
    for i, (cid, hp, st) in enumerate(specs):
        d = 0xf500 + cid * 128
        w16(d + 16, max_hp)
        g.wr16(pp + i * 62 + 2, max_hp)
        g.put(md + d + 0x52, bytes(16))
        g.put(md + d + 0x62, bytes(8))
        g.put(md + d + 0x6a, bytes(16))
        for off, val in [(0x19, 0), (0x1a, 0), (0x1c, 31), (0x1d, 31)]:
            g.wr8(md + d + off, val)
        g.wr8(pp + i * 62 + 23, 0)
        g.wr8(pp + i * 62 + 24, 31)
        if i == 0:
            g.wr8(md + d + (0x62 if skill else 0x52), id_)
            g.wr8(md + d + 0x6a, uses)
            amount = (100 if skill else cost) if tp_value is None else tp_value
            w16(d + 18, amount)
            g.wr16(pp + 4, amount)
    return p

def battle_case(name, skill, id_, specs, target=0, unavailable=False, repeat=False):
    p = ability_fixture(skill, id_, specs, uses=0 if unavailable else 1, tp_value=0 if unavailable else None)
    effect, range_ = original_rom[p], original_rom[p + 2] & 15
    expected = bytearray(g.mem(md + 0xf100, 0xa00))
    actors = g.mem(pp, 62 * len(specs))
    enemy = g.mem(ee, 62)
    g.put(cp, bytes([3 if skill else 2, id_, target, 0xff]))
    g.wr32(tp, 0)
    call('battle_execute_cmd', (pp, len(specs), ee, 0, cp, tp))
    after = g.mem(pp, len(actors))
    want = bytearray(actors)
    if not unavailable:
        if skill:
            expected[0x400 + specs[0][0] * 128 + 0x6a] -= 1
        else:
            want[4:6] = bytes(2)
        for i, (cid, hp, st) in enumerate(specs):
            eligible = range_ in (5, 7, 9) or i == (0 if range_ == 3 else target)
            eligible &= not (range_ in (4, 5) and cid in (6, 7))
            eligible &= bool(st & 0x44) if effect == 0x15 else effect == 0x16 or not st & 0x44
            if not eligible:
                continue
            j = i * 62
            if effect == 0x12:
                stat = (0 if original_rom[p + 1] == 1 else 31) if skill else 31
                lo = (8 * stat // 64 + stat + 2 * original_rom[p + 3]) // 2
                hi = (120 * stat // 64 + stat + 2 * original_rom[p + 3]) // 2
                value = int.from_bytes(after[j:j + 2], 'little')
                assert min(1000, hp + lo) <= value <= min(1000, hp + hi), (name, value, lo, hi)
                want[j:j + 2] = after[j:j + 2]
            elif effect in (0x15, 0x16):
                want[j:j + 2] = (250 if effect == 0x15 else 1000).to_bytes(2, 'little')
                want[j + 27] &= 0x90
            elif effect == 0x13:
                want[j + 27] &= 0xfe
            elif effect == 0x14:
                want[j + 27] &= 0xfd
            if effect >= 0x13:
                want[j + 25] = want[j + 32] = 33 + i
                want[j + 26] = want[j + 33] = 44 + i
                d = 0x400 + cid * 128
                expected[d + 32], expected[d + 35] = 33 + i, 44 + i
    assert after == want and g.mem(ee, 62) == enemy, name + ' actor/target mismatch'
    assert g.mem(md + 0xf100, 0xa00) == expected, name + ' resource/saved-state mismatch'
    if repeat:
        call('battle_execute_cmd', (pp, len(specs), ee, 0, cp, tp))
        assert g.mem(pp, len(actors)) == after and g.mem(md + 0xf100, 0xa00) == expected, name + ' depleted repeated command mutated'
    call('battle_store_party', (pp, len(specs)))
    for i, (cid, _, _) in enumerate(specs):
        d = 0xf500 + cid * 128
        assert read16(g.mem(md + d, 128), 14) == int.from_bytes(after[i*62:i*62+2], 'little')
        assert g.rd8(md + d + 22) == after[i*62+27]
    results.append({'case': name, 'route': 'actual battle_execute_cmd and battle_store_party', 'entire_actor_saved_state_enemy_checks': True})
    print('PASS', name, flush=True)

def field_expected(saved, seed, skill, id_, specs, selected):
    p = (0x2a9d28 if skill else 0x2a9be8) + (id_ - 1) * 8
    effect, range_ = original_rom[p], original_rom[p + 2] & 15
    caster = 0x400 + specs[0][0] * 128
    if skill:
        saved[caster + 0x6a] -= 1
        stat = saved[caster + {1:0x19, 2:0x1c, 3:0x1f, 4:0x22}[original_rom[p + 1] & 15]]
    else:
        write16(saved, caster + 18, read16(saved, caster + 18) - original_rom[p + 1])
        stat = saved[caster + 0x1c]
    multi = range_ in (5, 7, 9)
    for i in (range(5) if multi else [0 if range_ == 3 else selected]):
        add = 999 if effect == 0x16 else 0
        if effect == 0x12:
            previous, total, accepted = 0, 0, 0
            while accepted < 16:
                if seed & 0xffff == 0:
                    seed = 0x2a6d365b
                product = seed * 41 & 0xffffffff
                low, high = product & 0xffff, product >> 16
                value = (low + high) & 0xffff
                seed = value << 16 | low
                roll = value & 7
                if roll != previous:
                    previous, total, accepted = roll, total + roll, accepted + 1
            add = (total + 8) * stat // 128 + stat // 2 + original_rom[p + 3]
        if i == len(specs):
            break
        cid, hp, st = specs[i]
        if not skill and cid in (6, 7):
            continue
        if skill and id_ in (0x2a, 0x36) and cid not in (6, 7):
            continue
        if (effect == 0x15 and not st & 4) or (effect not in (0x15, 0x16) and st & 4):
            continue
        d = 0x400 + cid * 128
        maximum = read16(saved, d + 16)
        if effect == 0x15:
            add = maximum // 4
        write16(saved, d + 14, min(maximum, hp + add))
        saved[d + 22] = st & (15 if effect == 0x12 else 14 if effect == 0x13 else 13 if effect == 0x14 else 0)
    return seed

def field_case(name, skill, id_, specs, selected=0, cancel=False, no_effect=False, unavailable=False, caster_blocked=False):
    p = ability_fixture(skill, id_, specs, uses=0 if unavailable else 1, tp_value=0 if unavailable else None)
    saved = bytearray(g.mem(md + 0xf100, 0xa00))
    range_ = original_rom[p + 2] & 15
    single = range_ not in (3, 5, 7, 9)
    g.wr8(symbols['field_event_reload_pending'], 0)
    begin('ps4_field_menu', (0, 0))
    g.run(100)
    for _ in range(2 if skill else 1):
        press('DOWN')
    press('A', after=80) # camp -> caster
    if caster_blocked:
        L.mg_break(symbols['ps4_ui_message'])
        g.press('A', 3, 80)
        assert L.mg_hit() and bytes(g.mem(g.reg(0), 16)).startswith(b"CAN'T ACT"), name
        assert g.mem(md + 0xf100, 0xa00) == saved
        L.mg_break(0xffffffff)
        g.run(60)
        press('A', after=80)
        L.mg_break(0x08800000)
        g.press('B', 3, 80)
        assert L.mg_hit(), name + ' blocked caster exit'
        L.mg_break(0xffffffff)
        results.append({'case':name,'route':'actual field caster status rejection','state_unchanged':True})
        print('PASS', name, flush=True)
        return
    press('A', after=80) # caster -> list
    shot(name + '_LIST')
    if unavailable:
        L.mg_break(symbols['ps4_ui_message'])
        g.press('A', 3, 80)
        assert L.mg_hit(), name + ' resource message'
        assert bytes(g.mem(g.reg(0), 16)).startswith(b'NO USES LEFT' if skill else b'NOT ENOUGH TP')
        assert g.mem(md + 0xf100, 0xa00) == saved
        L.mg_break(0xffffffff)
        g.run(60)
        press('A', after=80)
    else:
        if single:
            L.mg_break(symbols['ps4_ui_select_party'])
            g.press('A', 3, 80)
            assert L.mg_hit(), name + ' target menu missing'
            L.mg_break(0xffffffff)
            g.run(70)
            assert g.mem(md + 0xf100, 0xa00) == saved, name + ' premature cost/effect'
            if cancel:
                press('B', after=80)
                assert g.mem(md + 0xf100, 0xa00) == saved, name + ' canceled cost/effect'
            else:
                for _ in range(selected):
                    press('DOWN')
        if not cancel:
            L.mg_break(symbols['ps4_field_use_ability'])
            g.press('A', 3, 80)
            assert L.mg_hit(), name + ' transaction missing'
            seed = int.from_bytes(g.mem(md + 0xef0c, 4), 'big')
            wantseed = field_expected(saved, seed, skill, id_, specs, selected)
            L.mg_break(symbols['ps4_ui_message'])
            g.run(80)
            assert L.mg_hit(), name + ' outcome missing'
            assert g.mem(md + 0xf100, 0xa00) == saved, name + ' field source state mismatch'
            assert int.from_bytes(g.mem(md + 0xef0c, 4), 'big') == wantseed, name + ' RNG calls/order mismatch'
            msg = bytes(g.mem(g.reg(0), 20)).split(bytes([0]))[0]
            assert msg.startswith(b'NO EFFECT' if no_effect else b'SKILL USED' if skill else b'TECHNIQUE USED'), (name, msg)
            L.mg_break(0xffffffff)
            g.run(60)
            shot(name + '_RESULT')
            press('A', after=80)
    press('B', after=80)
    L.mg_break(0x08800000)
    g.press('B', 3, 80)
    assert L.mg_hit(), name + ' camp exit missing'
    assert g.mem(md + 0xf100, 0xa00) == saved, name + ' acknowledgement repeated use'
    L.mg_break(0xffffffff)
    results.append({'case':name, 'route':'actual camp TECH/SKILL/caster/list/target/use/ack/exit', 'complete_saved_state_matches_independent_ROM_oracle':True, 'canceled':cancel, 'resource_blocked':unavailable})
    print('PASS', name, flush=True)

# Every corrected record reaches the actual production command branch.
for id_ in (0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x22, 0x23, 0x24, 0x25):
    battle_case('BATTLE_TECH_%02X' % id_, False, id_, [(0, 1, 0), (1, 0, 0xfd), (6, 1, 0)], target=1 if id_ in (0x24, 0x25) else 0, repeat=True)
for id_ in (0x2a, 0x2b, 0x2c, 0x36):
    battle_case('BATTLE_SKILL_%02X' % id_, True, id_, [(6, 1, 0), (0, 1, 0), (7, 0, 0x40)], target=1, repeat=True)
battle_case('BATTLE_RES_DEAD_REFUSED', False, 0x18, [(0, 1, 0), (1, 0, 4)], target=1)
battle_case('BATTLE_REVER_LIVING_SPENT', False, 0x24, [(0, 1, 0)])
battle_case('BATTLE_REGEN_LIVING_STATUS90', False, 0x25, [(0, 1, 0), (1, 1, 0x93)], target=1)
battle_case('BATTLE_MEDICE_ANDROID', True, 0x2b, [(0, 1, 0), (7, 1, 0)], target=1)
battle_case('BATTLE_NO_TP', False, 0x18, [(0, 1, 0)], unavailable=True)
battle_case('BATTLE_NO_SKILL_USES', True, 0x2b, [(0, 1, 0)], unavailable=True)
for id_ in (0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x22, 0x23, 0x24, 0x25):
    field_case('FIELD_TECH_%02X' % id_, False, id_, [(0, 1, 0), (1, 0, 0xfd), (6, 1, 0)], selected=1 if id_ in (0x24, 0x25) else 0, no_effect=id_ in (0x22, 0x23))
for id_ in (0x2a, 0x2b, 0x2c, 0x36):
    field_case('FIELD_SKILL_%02X' % id_, True, id_, [(6, 1, 0), (0, 1, 0), (7, 1, 0)], selected=2)
field_case('FIELD_REGEN_LIVING', False, 0x25, [(0, 1, 0x90)])
field_case('FIELD_RES_DEAD_SPENT', False, 0x18, [(0, 1, 0), (1, 0, 4)], selected=1, no_effect=True)
field_case('FIELD_REVER_LIVING_SPENT', False, 0x24, [(0, 1, 0)], no_effect=True)
field_case('FIELD_TECH_ANDROID_SPENT', False, 0x18, [(0, 1, 0), (7, 1, 0)], selected=1, no_effect=True)
field_case('FIELD_ANTI_MASK', False, 0x22, [(0, 1, 0x91)])
field_case('FIELD_RIMPA_MASK', False, 0x23, [(0, 1, 0), (1, 1, 0x93)], selected=1)
field_case('FIELD_RES_CANCEL', False, 0x18, [(0, 1, 0)], cancel=True)
field_case('FIELD_MEDICE_CANCEL', True, 0x2b, [(0, 1, 0)], cancel=True)
field_case('FIELD_NO_TP', False, 0x18, [(0, 1, 0)], unavailable=True)
field_case('FIELD_NO_SKILL_USES', True, 0x2b, [(0, 1, 0)], unavailable=True)
field_case('FIELD_DEAD_CASTER', False, 0x18, [(0, 0, 4)], caster_blocked=True)
field_case('FIELD_PARALYZED_CASTER', True, 0x2b, [(0, 1, 2)], caster_blocked=True)
report = {'core':'upstream mGBA 0.10.5', 'matching_ROM_state_crc_verified':True, 'cases':results, 'normal_playthrough_physical_power_cycle_animation_or_audio_quality_verified':False}
(O / (prefix + '_MGBA_HEALING_ABILITIES.json')).write_text(json.dumps(report, indent=2) + '\n')
print('PASS', len(results), 'native healing ability command/menu cases')
