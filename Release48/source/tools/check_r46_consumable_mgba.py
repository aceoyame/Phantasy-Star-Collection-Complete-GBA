#!/usr/bin/env python3
"""Current-ROM native battle commands and actual field item-menu decisions.

Reuse only the CRC-identified matching state created by the fresh full-drop
runner. Controlled fixtures do not certify normal playthrough or animation parity.
"""
from pathlib import Path
import argparse, json, sys, zlib

S = Path(__file__).resolve().parents[1]
O = S.parent / 'build_artifacts'
ap = argparse.ArgumentParser()
ap.add_argument('--label', default='CP21')
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
assert json.loads(state.with_suffix('.rom_crc.json').read_text()) == rom_id, 'run fresh matching full-drop test first'
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
    g.shot(str(O / (prefix + '_ITEM_' + name + '.png')), 2)

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

def battle_case(name, item, specs, target=0, slot=0, command_slot=None, repeat=False, missing=False):
    inventory = fixture(item, specs, slot)
    if missing:
        inventory = bytes(0 if i == slot else value for i, value in enumerate(inventory))
        g.put(md + 0xf410, inventory)
    expected = bytearray(g.mem(md + 0xf100, 0xa00))
    actors = g.mem(pp, 62 * len(specs))
    enemy = g.mem(ee, 62)
    g.put(cp, bytes([4, item, target, slot if command_slot is None else command_slot]))
    g.wr32(tp, 0)
    call('battle_execute_cmd', (pp, len(specs), ee, 0, cp, tp))
    after = g.mem(pp, 62 * len(specs))
    assert g.mem(ee, 62) == enemy, name + ' enemy mutation'
    ip = 0x2a8e28 + (item - 1) * 22
    effect, range_ = original_rom[ip], original_rom[ip + 2] & 15
    targets = []
    for i, (cid, hp, status) in enumerate(specs):
        before_actor = bytearray(actors[i * 62:(i + 1) * 62])
        got = after[i * 62:(i + 1) * 62]
        eligible = (range_ in (5, 7, 9) or i == target) and not missing
        eligible &= not (range_ in (4, 5) and cid in (6, 7))
        eligible &= not (range_ in (6, 7) and cid not in (6, 7))
        dead = status & 0x44
        eligible &= bool(dead) if effect == 0x15 else effect == 0x16 or not dead
        if eligible:
            maximum = int.from_bytes(before_actor[2:4], 'little')
            if effect == 0x12:
                value = int.from_bytes(got[0:2], 'little')
                lo = (8 * original_rom[ip + 1] // 64 + original_rom[ip + 1] + 2 * original_rom[ip + 3]) // 2
                hi = (120 * original_rom[ip + 1] // 64 + original_rom[ip + 1] + 2 * original_rom[ip + 3]) // 2
                assert min(maximum, hp + lo) <= value <= min(maximum, hp + hi), (name, value, lo, hi)
                before_actor[0:2] = got[0:2]
            if effect == 0x15:
                before_actor[0:2] = (maximum // 4).to_bytes(2, 'little')
                before_actor[27] &= 0x90
            if effect == 0x16:
                before_actor[0:2] = maximum.to_bytes(2, 'little')
                if item == 0x90:
                    before_actor[27] &= 0xbf
                    if dead & 0x40:
                        before_actor[27] |= 0x80
                elif dead:
                    before_actor[27] &= 0x90
            if effect == 0x13:
                before_actor[27] &= 0xfe
            if effect == 0x14:
                before_actor[27] &= 0xfd
            if 0x13 <= effect <= 0x16:
                before_actor[25] = before_actor[32] = 33 + i
                before_actor[26] = before_actor[33] = 44 + i
                d = 0x400 + cid * 128
                expected[d + 32], expected[d + 35] = 33 + i, 44 + i
            targets.append(i)
        assert got == before_actor, (name, i, 'unexpected actor change')
    expected[0x310:0x338] = inventory if missing else inventory[:slot] + inventory[slot + 1:] + bytes([0])
    assert g.mem(md + 0xf100, 0xa00) == expected, name + ' persistent state'
    if repeat:
        # The same command cannot heal a second time after consuming its sole item.
        frozen = g.mem(pp, 62 * len(specs)) + g.mem(md + 0xf100, 0xa00)
        call('battle_execute_cmd', (pp, len(specs), ee, 0, cp, tp))
        assert g.mem(pp, 62 * len(specs)) + g.mem(md + 0xf100, 0xa00) == frozen, name + ' duplicate use'
    call('battle_store_party', (pp, len(specs)))
    call('battle_compose_igg', (pp, len(specs), ee, 0, 0))
    call('battle_present_page0')
    # A return breakpoint can precede mGBA's next displayed frame. Advance
    # through native VBlank waits so the screenshot shows the published HUD.
    call('waitvb')
    call('waitvb')
    assert g.mem(pp, 62 * len(specs)) == after, name + ' presentation altered actors'
    for i, (cid, _, _) in enumerate(specs):
        d = 0xf500 + cid * 128
        hp = int.from_bytes(after[i * 62:i * 62 + 2], 'little')
        assert int.from_bytes(g.mem(md + d + 14, 2), 'big') == hp, name + ' party store HP'
        assert g.rd8(md + d + 22) == after[i * 62 + 27], name + ' party store status'
    shot(name)
    results.append({'case': name, 'route': 'actual battle_execute_cmd', 'targets': targets,
                    'entire_actor_and_saved_state_checks': True, 'repeated_command_blocked': repeat})
    print('PASS', name, flush=True)

def field_oracle(saved, item, seed):
    ip = 0x2a8e28 + (item - 1) * 22
    effect, range_ = original_rom[ip], original_rom[ip + 2] & 15
    def healing():
        nonlocal seed
        if effect == 0x16:
            return 999
        if effect != 0x12:
            return 0
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
        return (total + 8) * original_rom[ip + 1] // 128 + original_rom[ip + 1] // 2 + original_rom[ip + 3]
    return healing

def field_case(name, item, specs, selected=0, cancel=False, no_effect=False):
    inventory = fixture(item, specs)
    saved = bytearray(g.mem(md + 0xf100, 0xa00))
    multi = (original_rom[0x2a8e28 + (item - 1) * 22 + 2] & 15) in (5, 7, 9)
    # The compiler inlines the ITEM submenu. Enter through its actual parent
    # and select ITEM, rather than forcing an additional production symbol.
    g.wr8(symbols['field_event_reload_pending'], 0)
    begin('ps4_field_menu', (0, 0))
    g.run(100)
    press('A', after=80)
    shot(name + '_LIST')
    if not multi:
        L.mg_break(symbols['ps4_ui_select_party'])
        g.press('A', 3, 60)
        assert L.mg_hit(), name + ' target menu missing'
        L.mg_break(0xffffffff)
        g.run(70)
        shot(name + '_TARGET')
        assert g.mem(md + 0xf100, 0xa00) == saved, name + ' premature mutation'
        if cancel:
            press('B', after=70)
            assert g.mem(md + 0xf100, 0xa00) == saved, name + ' cancellation mutated state'
            press('B', after=80)  # ITEM list -> main camp menu
            L.mg_break(0x08800000)
            g.press('B', 3, 80)
            assert L.mg_hit(), name + ' cancelled menu did not return'
            L.mg_break(0xffffffff)
            results.append({'case': name, 'route': 'actual field ITEM/target cancel', 'saved_state_unchanged': True})
            print('PASS', name, flush=True)
            return
        for _ in range(selected):
            press('DOWN')
    L.mg_break(symbols['ps4_field_use_item'])
    g.press('A', 3, 60)
    assert L.mg_hit(), name + ' field transaction missing'
    healing = field_oracle(saved, item, int.from_bytes(g.mem(md + 0xef0c, 4), 'big'))
    effect = original_rom[0x2a8e28 + (item - 1) * 22]
    indices = range(5) if multi else [selected]
    for i in indices:
        add = healing()
        if i == len(specs):
            break
        cid, hp, st = specs[i]
        android = cid in (6, 7)
        if multi:
            if android or st & 4:
                continue
        elif item == 0x90:
            if not android:
                continue
        elif android or (item == 0x82 and not st & 4) or (item not in (0x82, 0x86) and st & 4):
            continue
        d = 0x400 + cid * 128
        maximum = read16(saved, d + 16)
        if item == 0x82:
            add = maximum // 4
        write16(saved, d + 14, min(maximum, hp + add))
        saved[d + 22] = st & (15 if effect == 0x12 else 14 if effect == 0x13 else 13 if effect == 0x14 else 0)
    saved[0x310:0x338] = inventory[1:] + bytes([0])
    L.mg_break(symbols['ps4_ui_message'])
    g.run(80)
    assert L.mg_hit(), name + ' outcome message missing'
    assert g.mem(md + 0xf100, 0xa00) == saved, name + ' field source state mismatch'
    msg = bytes(g.mem(g.reg(0), 16)).split(bytes([0]))[0]
    assert msg.startswith(b'NO EFFECT' if no_effect else b'ITEM USED'), (name, msg)
    L.mg_break(0xffffffff)
    g.run(60)
    shot(name + '_RESULT')
    press('A', after=80)
    press('B', after=80)  # ITEM list -> main camp menu
    L.mg_break(0x08800000)
    g.press('B', 3, 80)
    assert L.mg_hit(), name + ' field menu exit missing'
    assert g.mem(md + 0xf100, 0xa00) == saved, name + ' acknowledgement duplicated use'
    L.mg_break(0xffffffff)
    results.append({'case': name, 'route': 'actual field ITEM/use/ack/exit', 'group': multi,
                    'complete_saved_state_matches_independent_field_oracle': True})
    print('PASS', name, flush=True)

battle_case('BATTLE_SHORTCAKE_ITEM_POWER', 0x91, [(0, 1, 0)], repeat=True)
battle_case('BATTLE_MONOMATE_MACRO', 0x7d, [(0, 1, 0)], command_slot=0xff)
battle_case('BATTLE_SHIFTED_SLOT', 0x91, [(0, 1, 0)], slot=16, command_slot=17)
battle_case('BATTLE_MISSING_ITEM', 0x7d, [(0, 1, 0)], missing=True)
battle_case('BATTLE_STAR_DEW_GROUP', 0x83, [(0, 1, 0), (1, 1, 0), (7, 1, 0)])
battle_case('BATTLE_HUMAN_ITEM_ANDROID_NO_EFFECT', 0x7d, [(0, 10, 0), (6, 1, 0)], target=1)
battle_case('BATTLE_MOON_DEW_QUARTER', 0x82, [(0, 10, 0), (1, 0, 0xfd)], target=1)
battle_case('BATTLE_MOON_DEW_LIVING_NO_EFFECT', 0x82, [(0, 10, 0)])
battle_case('BATTLE_SOL_DEW_DEAD', 0x86, [(0, 10, 0), (1, 0, 0xfd)], target=1)
battle_case('BATTLE_SOL_DEW_LIVING', 0x86, [(0, 1, 3)])
battle_case('BATTLE_REPAIR_KIT_DEAD_ANDROID', 0x90, [(0, 10, 0), (6, 0, 0x43)], target=1)
battle_case('BATTLE_ANTIDOTE_RESTORE_STATS', 0x80, [(0, 10, 3)])
battle_case('BATTLE_CURE_PARALYSIS_RESTORE_STATS', 0x81, [(0, 10, 3)])
field_case('FIELD_CANCEL_TARGET', 0x7d, [(0, 1, 0), (1, 1, 0)], cancel=True)
field_case('FIELD_SHORTCAKE_ITEM_POWER', 0x91, [(0, 1, 0)])
field_case('FIELD_PEROL_ROUNDING', 0x93, [(0, 1, 0)])
field_case('FIELD_STAR_DEW_GROUP', 0x83, [(0, 1, 0), (1, 1, 0), (7, 1, 0)])
field_case('FIELD_MOON_DEW_QUARTER', 0x82, [(0, 10, 0), (1, 0, 0xfd)], selected=1)
field_case('FIELD_MOON_DEW_LIVING_CONSUMED', 0x82, [(0, 10, 0)], no_effect=True)
field_case('FIELD_SOL_DEW_LIVING', 0x86, [(0, 1, 3)])
field_case('FIELD_REPAIR_KIT_ANDROID', 0x90, [(0, 10, 0), (6, 0, 0x43)], selected=1)
field_case('FIELD_HUMAN_ITEM_ANDROID_CONSUMED', 0x7d, [(0, 10, 0), (7, 1, 0)], selected=1, no_effect=True)
field_case('FIELD_ANTIDOTE_STATUS_MASK', 0x80, [(0, 10, 0x93)])
field_case('FIELD_CURE_PARALYSIS_STATUS_MASK', 0x81, [(0, 10, 0x93)])
report = {'core': 'upstream mGBA 0.10.5', 'matching_ROM_state_crc_verified': True,
          'fresh_state_producer': 'check_r46_full_drop_mgba.py --label ' + a.label,
          'cases': results, 'normal_playthrough_physical_power_cycle_animation_or_audio_quality_verified': False}
(O / (prefix + '_MGBA_CONSUMABLES.json')).write_text(json.dumps(report, indent=2) + '\n')
print('PASS', len(results), 'native battle-command/field-menu cases')
