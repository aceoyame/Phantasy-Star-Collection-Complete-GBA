#!/usr/bin/env python3
"""Current-ROM Medic Power / Ataraxia commands, controllers and field menus.

Boot the current ROM, or explicitly reuse only its CRC-identified state. Controlled fixtures do not certify normal playthrough or animation parity.
"""
from pathlib import Path
import argparse, json, sys, zlib

S = Path(__file__).resolve().parents[1]
O = S.parent / 'build_artifacts'
ap = argparse.ArgumentParser()
ap.add_argument('--label', default='CP23')
ap.add_argument('--reuse', action='store_true')
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
if a.reuse:
    assert json.loads(state.with_suffix('.rom_crc.json').read_text()) == rom_id, 'state belongs to another ROM'
    g = GBA(rom)
else:
    import mgba
    sys.modules['gba'] = mgba
    sys.path.insert(0, str(S / 'tools_host/r46_gba_emu'))
    from boot_debug import boot_to_debug
    g = boot_to_debug(str(rom), str(state))
    state.with_suffix('.rom_crc.json').write_text(json.dumps(rom_id) + '\n')
    print('Fresh current-ROM debug state created', flush=True)
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


def special_fixture(id_, specs, unavailable=False, known=True, strength=73, mental=200):
    p = ability_fixture(True, id_, [(cid,hp,st) for cid,hp,st,tp_ in specs], uses=0 if unavailable else 1)
    for i,(cid,hp,st,tp_) in enumerate(specs):
        d=0xf500+cid*128
        w16(d+18,tp_);w16(d+20,300);g.wr16(pp+i*62+4,tp_);g.wr16(pp+i*62+6,300)
        g.wr8(md+d+0x19,strength);g.wr8(md+d+0x1c,mental)
        g.wr8(pp+i*62+23,strength);g.wr8(pp+i*62+24,mental)
        if i==0 and not known:g.wr8(md+d+0x62,0)
    return p

def battle_case(name,id_,specs,controller=False,unavailable=False,known=True,strength=73,mental=200):
    p=special_fixture(id_,specs,unavailable,known,strength,mental)
    saved=bytearray(g.mem(md+0xf100,0xa00));actors=g.mem(pp,len(specs)*62);want=bytearray(actors);enemy=g.mem(ee,62)
    g.put(cp,bytes([3,id_,4,0xff]));g.wr32(tp,0)
    if controller:
        assert call('battle_party_source_support_skill',(pp,len(specs),ee,0,cp,tp))==1,name+' controller did not execute'
    else:call('battle_execute_cmd',(pp,len(specs),ee,0,cp,tp))
    after=g.mem(pp,len(actors));eligible=[]
    if not unavailable and known:
        saved[0x400+specs[0][0]*128+0x6a]-=1
        for i,(cid,hp,st,tp_) in enumerate(specs):
            j=i*62
            if cid not in (6,7) and (id_==0x2d or not st&0x44):
                eligible.append(i);stat=(strength if original_rom[p+1]==1 else mental);power=original_rom[p+3]
                lo=(8*stat//64+stat+2*power)//2;hi=(120*stat//64+stat+2*power)//2
                off=0 if id_==0x2d else 4;old=hp if off==0 else tp_;maximum=1000 if off==0 else 300
                got=int.from_bytes(after[j+off:j+off+2],'little')
                assert min(maximum,old+lo)<=got<=min(maximum,old+hi),(name,i,got,lo,hi)
                want[j+off:j+off+2]=after[j+off:j+off+2]
                if id_==0x2d:want[j+27]&=0x90
            if id_==0x2e:want[j+27]&=0xf7
    assert after==want and g.mem(ee,62)==enemy,name+' actor or enemy mismatch'
    assert g.mem(md+0xf100,0xa00)==saved,name+' charge or saved-state mismatch'
    if controller:
        # Production track layout is 32 bytes; nt is at offset 8. Caster
        # helpers always exist. Target fanout excludes dead Ataraxia targets.
        count=g.rd8(symbols['battle_ability_track_count'])
        nts=[g.rd8(0x02033d80+32*i+8) for i in range(count)]
        assert nts==([1,len(eligible),1] if id_==0x2d and eligible else [1,1] if id_==0x2d else [2,len(eligible),len(eligible),len(eligible)] if eligible else [2]),(name,nts,eligible)
        shot(name)
    # A depleted repeated command must not heal, restore TP, clear status or
    # spend again, whether the first execution used the controller or command.
    call('battle_execute_cmd',(pp,len(specs),ee,0,cp,tp))
    assert g.mem(pp,len(actors))==after and g.mem(md+0xf100,0xa00)==saved,name+' repeated use'
    call('battle_store_party',(pp,len(specs)))
    for i,(cid,_,_,_) in enumerate(specs):
        stored=g.mem(md+0xf500+cid*128,128)
        assert read16(stored,14)==int.from_bytes(after[i*62:i*62+2],'little')
        assert read16(stored,18)==int.from_bytes(after[i*62+4:i*62+6],'little')
        assert stored[22]==after[i*62+27]
    results.append({'case':name,'route':'actual source skill controller' if controller else 'actual battle_execute_cmd','eligible_targets':eligible,'complete_actor_enemy_resource_guards':True,'HP_TP_status_party_store':True})
    print('PASS',name,flush=True)

def field_oracle(saved,seed,specs,strength):
    caster=0x400+specs[0][0]*128;saved[caster+0x6a]-=1;changed=False
    p=0x2a9d28+(0x2d-1)*8
    for i in range(5):
        accepted,previous,total=0,0,0
        while accepted<16:
            if seed&65535==0:seed=0x2a6d365b
            product=seed*41&0xffffffff;low=product&65535;value=(low+(product>>16))&65535
            seed=value<<16|low;q=value%8
            if q!=previous:previous=q;total+=q;accepted+=1
        amount=(total+8)*strength//128+strength//2+original_rom[p+3]
        if i==len(specs):break
        cid,hp,st,_=specs[i]
        if cid in (6,7):continue
        d=0x400+cid*128;next_=min(read16(saved,d+16),hp+amount)
        changed|=next_!=hp or st!=0;write16(saved,d+14,next_);saved[d+22]=0
    return seed,changed

def field_case(name,specs,strength=73,unavailable=False,cancel=False,id_=0x2d,blocked=False):
    special_fixture(id_,specs,unavailable,strength=strength)
    saved=bytearray(g.mem(md+0xf100,0xa00))
    g.wr8(symbols['field_event_reload_pending'],0)
    begin('ps4_field_menu',(0,0));g.run(100);press('DOWN');press('DOWN');press('A',after=80)
    if blocked or id_==0x2e:
        L.mg_break(symbols['ps4_ui_message']);g.press('A',3,80);assert L.mg_hit(),name+' rejection message missing'
        msg=bytes(g.mem(g.reg(0),20)).split(bytes([0]))[0]
        assert msg.startswith(b"CAN'T ACT" if blocked else b'NO FIELD SKILLS'),(name,msg)
        assert g.mem(md+0xf100,0xa00)==saved
        L.mg_break(0xffffffff);g.run(60);press('A',after=80)
    else:
        press('A',after=80);shot(name+'_LIST')
        assert g.mem(md+0xf100,0xa00)==saved,name+' pre-confirm mutation'
        if cancel:pass
        elif unavailable:
            L.mg_break(symbols['ps4_ui_message']);g.press('A',3,80);assert L.mg_hit(),name+' no-use message missing'
            assert bytes(g.mem(g.reg(0),20)).startswith(b'NO USES LEFT')
            assert g.mem(md+0xf100,0xa00)==saved
            L.mg_break(0xffffffff);g.run(60);press('A',after=80)
        else:
            L.mg_break(symbols['ps4_field_use_ability']);g.press('A',3,80);assert L.mg_hit(),name+' transaction missing'
            seed=int.from_bytes(g.mem(md+0xef0c,4),'big');wantseed,changed=field_oracle(saved,seed,specs,strength)
            L.mg_break(symbols['ps4_ui_message']);g.run(80);assert L.mg_hit(),name+' outcome missing'
            assert g.mem(md+0xf100,0xa00)==saved,name+' field source mismatch'
            assert int.from_bytes(g.mem(md+0xef0c,4),'big')==wantseed,name+' source RNG mismatch'
            msg=bytes(g.mem(g.reg(0),20)).split(bytes([0]))[0]
            assert msg.startswith(b'SKILL USED' if changed else b'NO EFFECT'),(name,msg)
            L.mg_break(0xffffffff);g.run(60);shot(name+'_RESULT');press('A',after=80)
        press('B',after=80)
    L.mg_break(0x08800000);g.press('B',3,80);assert L.mg_hit(),name+' camp exit missing'
    assert g.mem(md+0xf100,0xa00)==saved,name+' acknowledgment duplicated skill'
    L.mg_break(0xffffffff)
    results.append({'case':name,'route':'actual camp SKILL/caster/list/use/ack/exit','complete_saved_state_source_oracle':True,'depleted_or_canceled_or_filtered':unavailable or cancel or blocked or id_==0x2e})
    print('PASS',name,flush=True)

mix=[(6,1,0,9),(0,1,0x93,0),(1,0,0xfd,5),(7,0,0x48,8),(8,1,0x28,300)]
battle_case('MEDIC_COMMAND_REVIVE_CLASS_STATUS',0x2d,mix)
battle_case('MEDIC_CONTROLLER_REVIVE_CLASS_STATUS',0x2d,mix,controller=True)
battle_case('ATARAXIA_COMMAND_TP_SLEEP',0x2e,mix)
battle_case('ATARAXIA_CONTROLLER_TP_SLEEP',0x2e,mix,controller=True)
battle_case('MEDIC_NO_USES',0x2d,mix,unavailable=True)
battle_case('ATARAXIA_NO_USES',0x2e,mix,unavailable=True)
battle_case('MEDIC_UNKNOWN_SKILL',0x2d,mix,known=False)
battle_case('ATARAXIA_UNKNOWN_SKILL',0x2e,mix,known=False)
battle_case('MEDIC_NO_HUMANS_COMMAND',0x2d,[(6,1,0,9),(7,0,0x48,8)])
battle_case('MEDIC_NO_HUMANS_CONTROLLER',0x2d,[(6,1,0,9),(7,0,0x48,8)],controller=True)
battle_case('ATARAXIA_NO_TP_TARGETS_COMMAND',0x2e,[(6,1,8,9),(7,0,0x48,8)])
battle_case('ATARAXIA_NO_TP_TARGETS_CONTROLLER',0x2e,[(6,1,8,9),(7,0,0x48,8)],controller=True)
battle_case('MEDIC_ZERO_STRENGTH',0x2d,mix,strength=0)
battle_case('ATARAXIA_ZERO_MENTAL',0x2e,mix,mental=0)
field_case('MEDIC_FIELD_MIXED_PARTY',mix)
field_case('MEDIC_FIELD_PARTIAL_PARTY',mix[:3])
field_case('MEDIC_FIELD_NO_HUMANS_SPENT',[(6,1,0,9),(7,1,0,8)])
field_case('MEDIC_FIELD_ZERO_STRENGTH_STATUS',mix,strength=0)
field_case('MEDIC_FIELD_FULL_HP_STATUS',[(6,1000,0,9),(0,1000,0x91,10),(1,1000,0,10)])
field_case('MEDIC_FIELD_NO_USES',mix,unavailable=True)
field_case('MEDIC_FIELD_CANCEL_LIST',mix,cancel=True)
field_case('MEDIC_FIELD_PARALYZED_CASTER',[(6,1,2,9),(0,1,0,0)],blocked=True)
field_case('ATARAXIA_FIELD_FILTERED',mix,id_=0x2e)
report={'core':'upstream mGBA 0.10.5','fresh_current_ROM_boot':not a.reuse,'matching_ROM_state_crc_verified':True,'cases':results,'original_animation_timing_audio_quality_normal_save_playthrough_or_physical_persistence_verified':False}
(O/(prefix+'_MGBA_SPECIAL_RECOVERY.json')).write_text(json.dumps(report,indent=2)+'\n')
print('PASS',len(results),'native special recovery cases')
