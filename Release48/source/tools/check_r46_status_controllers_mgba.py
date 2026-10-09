#!/usr/bin/env python3
"""Release 07: six native offensive TECH controllers, results and OBJ timelines.

Controlled level-60 party fixtures. Uses the original TECH records and authored
BattleObj delays; it does not certify an ordinary playthrough or VDP parity.
"""
from pathlib import Path
import re
h = Path(__file__).with_name('check_r46_special_recovery_mgba.py')
exec(compile(h.read_text().split('\ndef special_fixture(')[0].replace("default='CP23'", "default='RELEASE07'"), str(h), 'exec'), globals())

names = ['VOL', 'SAVOL', 'GELUN', 'DORAN', 'SEALS', 'RIMIT']
specs = [(0, 100, 0), (5, 100, 0), (3, 100, 0), (6, 100, 0), (9, 100, 0)]
bank = (S / 'gba/source_ability_effects.bin').read_bytes()
catalog = (S / 'gba/source_ability_effects.c').read_text()
def rows(type_, name):
    text = catalog.split(f'const {type_} {name}[]={{')[1].split('};')[0]
    return [[int(v.rstrip('u'), 0) for v in line.strip('{};,').split(',')] for line in text.splitlines() if line.startswith('{')]
parts = rows('Ps4SourceAbilityPart', 'ps4_source_ability_parts')
anims = rows('Ps4SourceAbilityAnim', 'ps4_source_ability_anims')
authored_holds = {0x25CDE0: [3]*4+[2]*8, 0x25CFC0: [4]*6,
                  0x25CFFE: [4,4,3,2,2,2,2,8,5,6], 0x25D0C4: [4,4,3,2,2,2,2,8,5,6],
                  0x25CE88: [3,3,3,3,3,5,4,2,2,2]}
assets = {}
for table, holds in authored_holds.items():
    v = next(v for v in anims if v[-1] == table)
    assert v[1] == 1 and list(bank[v[3]:v[3]+v[2]]) == holds
    assets[table] = (parts[v[0]], holds)

def le16(b, k, v): b[k:k+2] = v.to_bytes(2, 'little')
def rotate(v): return (((v >> 17) | ((v & 0x10000) >> 1)) << 16) | (v & 65535)
def trunc(n, d): return (abs(n) // d) * (-1 if n < 0 else 1)
def gx(x): return 120 + trunc((x-0x120)*3, 4)
def gy(y): return trunc((y-0x80)*5, 7)
places = [8,16,24,32]
dx = [-32,-29,-25,-19,-12,-4,4,12,19,25,29,32]
dy = [-6,-4,-3,-2,-1,-1,-1,-1,-2,-3,-4,-6]

def setup(id_, mode):
    ability_fixture(False, id_, specs, tp_value=500)
    g.wr8(pp+24, 255)
    for cid, _, _ in specs: w16(0xf500+cid*128+8, 60)
    for i in range(4):
        call('battle_load_enemy', (9, ee+i*62))
        for off in (0, 2, 14): g.wr16(ee+i*62+off, 1000)
        g.wr16(ee+i*62+8, 1000)
        g.wr8(ee+i*62+32, 200); g.wr8(ee+i*62+25, 200)
        for off in (23,24,26,27): g.wr8(ee+i*62+off, 0)
        factor = 0 if mode == 'immune' or (mode == 'mixed' and i == 1) else 4
        for j in range(14): g.wr8(ee+i*62+34+j, factor)
        if mode == 'mixed' and i == 2: g.wr8(ee+i*62+27, 4)
        if mode == 'mixed' and i == 3: g.wr16(ee+i*62, 0)
        if mode == 'already': g.wr8(ee+i*62+27, 16 if id_ == 0x16 else 8)
        if mode == 'repeat':
            if id_ == 0x14: g.wr16(ee+i*62+8, 745)
            else: g.wr8(ee+i*62+25, 1)
    if mode == 'sealed': g.wr8(pp+27, 16)
    if mode == 'no_tp': g.wr16(pp+4, 0)
    g.wr32(symbols['battle_enemy_count'], 4)
    g.put(symbols['battle_enemy_placement'], bytes(places))
    g.put(symbols['battle_enemy_reaction_flags'], bytes(4))
    call('battle_anim_reset'); call('battle_ui_bind', (pp,5,ee,0))
    call('battle_compose_igg', (pp,5,ee,0,0)); call('battle_present_page0')
    for i in range(64,112): g.put(0x07000000+i*8, bytes([160,0,0,0,0,0,0,0]))
    g.wr8(symbols['battle_defer_death_audio'], int(mode == 'outer_defer'))
    w32(0xef0c, 0x12345678)
    g.put(cp, bytes([2,id_,0,255])); g.wr32(tp, 0)
    return original_rom[0x2a9be8+(id_-1)*8:0x2a9be8+id_*8]

def oracle(id_, r, allowed):
    p = bytearray(g.mem(pp,310)); e = bytearray(g.mem(ee,248)); seed = 0x12345678; hits = 0
    if allowed:
        le16(p,4,int.from_bytes(p[4:6],'little')-r[1])
        for i in (range(4) if r[2]&15 == 2 else [0]):
            k=i*62; st=e[k+27]
            if not int.from_bytes(e[k:k+2],'little') or st&0x44 or (r[0] == 7 and st&10) or (r[0] == 8 and st&16): continue
            seed = rotate(seed)
            if not e[k+34+r[5]-1]: continue
            hits |= 1 << i
            if r[0] == 2: le16(e,k,0)
            elif r[0] == 3: le16(e,k+8,745)
            elif r[0] == 6: e[k+25]=1
            elif r[0] == 7: e[k+27]|=8
            elif r[0] == 8: e[k+27]|=16
            else: raise AssertionError(r.hex())
    return p,e,seed,hits

def active_objects():
    return [(g.rd16(0x07000000+i*8),g.rd16(0x07000000+i*8+2),g.rd16(0x07000000+i*8+4)) for i in range(64,112) if g.rd16(0x07000000+i*8)&255 != 160]

def check_objects(objects, expected, label, tick):
    assert len(objects) == len(expected), (label,tick,'OBJ count',len(objects),len(expected))
    for (a0,a1,a2),(table,age,x,y) in zip(objects,expected):
        part,holds=assets[table]; frame=0
        while age >= holds[frame]: age-=holds[frame]; frame+=1
        off,n,geom,_,ox,oy,_=part
        shape,size=geom&3,(geom>>2)&3
        hw=[[8,16,32,64],[16,32,32,64],[8,8,16,32]][shape][size]//2
        hh=[[8,16,32,64],[8,8,16,32],[16,32,32,64]][shape][size]//2
        wantx=(x+trunc((ox+hw)*3,4)-hw)&511
        wanty=(y+trunc((oy+hh)*5,7)-hh)&255
        assert a0 == wanty|0x100|(shape<<14) and a1 == wantx|(31<<9)|(size<<14), (label,tick,'placement',hex(a0),hex(a1),wantx,wanty)
        assert a2>>12 == 15 and (a2>>10)&3 == 1, (label,tick,'OBJ palette/priority')
        assert g.mem(0x06010000+(a2&1023)*32,n) == bank[off+frame*n:off+(frame+1)*n], (label,tick,'source pixels',table,frame)

def case(id_,mode,trace=True):
    label=names[id_-0x12]+'_'+mode.upper(); r=setup(id_,mode)
    allowed=mode not in ('sealed','no_tp'); party,enemy,seed,hits=oracle(id_,r,allowed)
    saved=g.mem(md+0xf100,0xa00); before_enemy=g.mem(ee,248); args=(pp,5,ee,0,cp,tp)
    # The exact same fixture exercises the command's returned success mask.
    scratch=O/(prefix+'_STATUS_TEST.bin'); g.save(scratch)
    assert call('battle_execute_cmd',args)==hits,(label,'command hit mask')
    assert g.mem(pp,310)==party and g.mem(ee,248)==enemy,(label,'direct result')
    assert g.mem(md+0xef0c,4)==seed.to_bytes(4,'big'),(label,'direct RNG')
    g.load(scratch); scratch.unlink()
    cast=12 if id_<0x14 else 30
    duration=0 if not allowed else cast+(20 if not hits else 44 if id_<0x14 else 100 if id_<0x16 else 38)
    samples=[]
    if allowed and trace:
        begin('battle_party_status_source_technique',args)
        for tick in range(duration):
            assert run_until_break(g,symbols['battle_action_idle_after_wait'],20),(label,tick,'controller ended early')
            expected=[]
            sx=0xc0 if r[2]&15==1 else 0x120
            if tick<cast and id_>=0x14:
                expected=[(0x25CE88,tick,gx(sx+16),gy(0x100))]
            elif tick>=cast and hits:
                t=tick-cast
                if id_<0x14:
                    if t<28:
                        expected=[(0x25CDE0,t,gx(sx),gy(0xd8))] if id_==0x12 else [(0x25CDE0,t,gx(0x80+places[i]*8),gy(0xd8)) for i in range(4) if hits>>i&1]
                elif id_<0x16:
                    for wave in range(12):
                        age=t-6*(wave+1)
                        if 0<=age<24:
                            expected += [(0x25CFC0,age,gx(0x80+places[i]*8+dx[wave]),gy(0xd8+dy[wave])) for i in range(4) if hits>>i&1]
                else:
                    expected=[(0x25CFFE if id_==0x16 else 0x25D0C4,t,gx(0x80+places[i]*8),gy(0xd8)) for i in range(4) if hits>>i&1]
            objects=active_objects(); check_objects(objects,expected,label,tick)
            if id_<0x14: assert g.mem(ee,248)==before_enemy,(label,tick,'HP changed before Vol completion')
            if id_==0x17: assert all(g.rd8(ee+i*62+27)==before_enemy[i*62+27] for i in range(4)),(label,tick,'sleep applied before completion')
            # Cues are observed at the production idle boundary immediately
            # after writes, before IRQ consumes Sound_Index on the next wait.
            if tick==0 and id_>=0x14: assert g.rd8(md+0x500a)==0xbc,(label,'cast cue')
            cue_tick=cast+(5 if id_ in (0x14,0x15) else 0)
            if tick==cue_tick and hits: assert g.rd8(md+0x500a)==(0xc8 if id_<0x14 else 0xca if id_<0x16 else 0xcb),(label,'hit cue')
            if tick>=cast and not hits: assert g.rd8(md+0x500a) not in (0xc8,0xca,0xcb),(label,'miss emitted success cue')
            samples.append({'tick':tick,'objects':len(objects)})
            if expected and tick==cast+12 and mode=='mixed': shot(label+'_ACTIVE')
            ret=g.reg(14); assert run_until_break(g,ret,20),(label,tick,'idle return')
        assert run_until_break(g,0x08800000,20),label+' return'
        assert g.reg(0)==1,label+' handled'
    else: assert call('battle_party_status_source_technique',args)==int(allowed),(label,'blocked controller')
    assert g.mem(pp,310)==party and g.mem(ee,248)==enemy,(label,'complete actor results')
    assert g.mem(md+0xf100,0xa00)==saved,(label,'saved state changed')
    assert g.mem(md+0xef0c,4)==seed.to_bytes(4,'big'),(label,'controller RNG stream')
    assert g.rd8(symbols['battle_defer_death_audio'])==int(mode=='outer_defer'),(label,'outer death ownership')
    if allowed:
        assert not active_objects(),label+' OBJ cleanup'
        assert g.rd8(symbols['battle_party_pose_actor'])==255 and g.rd8(symbols['battle_party_x_override_actor'])==255,label+' pose cleanup'
    flags=bytes((6|(16 if r[2]&15==2 else 0)) if hits>>i&1 else 0 for i in range(4))
    assert g.mem(symbols['battle_enemy_reaction_flags'],4)==flags,(label,'reaction flags')
    results.append({'case':label,'success_mask':hits,'updates':duration,'per_update_OAM_placement_palette_source_pixels_and_cues':bool(trace and allowed),'TP_once_saved_RNG_actor_guards':True,'trace':samples})
    print('PASS',label,duration,'updates; hits',hits,flush=True)

for id_ in range(0x12,0x18):
    for mode in ('all_hit','mixed','immune','sealed','no_tp'): case(id_,mode)
for id_ in (0x14,0x15): case(id_,'repeat')
for id_ in (0x16,0x17): case(id_,'already')
case(0x13,'outer_defer')
report={'core':'upstream mGBA 0.10.5','rom':rom_id,'fresh_current_ROM_boot':not a.reuse,'cases':results,
        'native_controller_cases':len(results),'instrumentation_changed':False,'normal_playthrough_or_physical_hardware_verified':False}
(O/(prefix+'_MGBA_STATUS_CONTROLLERS.json')).write_text(json.dumps(report,indent=2)+'\n')
print('PASS',len(results),'native status controller cases',flush=True)
