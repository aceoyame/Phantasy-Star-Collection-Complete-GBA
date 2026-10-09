#!/usr/bin/env python3
"""Telele/Shadow native outcomes, success-only children, source art and cues.

Controlled fixtures verify the real controller and every native controller
update. This does not certify whole-battle or original 68000 clock parity.
"""
from pathlib import Path
import re
h=Path(__file__).with_name('check_r46_special_recovery_mgba.py')
exec(compile(h.read_text().split('\ndef special_fixture(')[0].replace("default='CP23'","default='RELEASE08'"),str(h),'exec'),globals())
assert 'if(c->id==0x1Du||c->id==0x1Eu)return battle_party_source_debuff_skill(' in (S/'gba/title_runtime_v06.c').read_text()
catalog=(S/'gba/source_ability_effects.c').read_text();bank=(S/'gba/source_ability_effects.bin').read_bytes()
def rows(t,n):
    return [[int(v.rstrip('u'),0) for v in line.strip('{};,').split(',')] for line in catalog.split(f'const {t} {n}[]={{')[1].split('};')[0].splitlines() if line.startswith('{')]
parts=rows('Ps4SourceAbilityPart','ps4_source_ability_parts');anims=rows('Ps4SourceAbilityAnim','ps4_source_ability_anims')
holds={0x25D916:[2]*7+[6,4,4],0x25AF70:[2,6,5,4,3,3],0x25CFC0:[4]*6,
       0x25B0C8:[2]*12,0x25B008:[1,2]*6+[1]}
assets={}
for table,hh in holds.items():
    v=next(v for v in anims if v[-1]==table)
    assert v[1]==1 and list(bank[v[3]:v[3]+v[2]])==hh
    assets[table]=parts[v[0]],hh
def rotate(v):return (((v>>17)|((v&0x10000)>>1))<<16)|(v&65535)
def trunc(n,d):return abs(n)//d*(-1 if n<0 else 1)
def gx(x):return 120+trunc((x-0x120)*3,4)
def gy(y):return trunc((y-0x80)*5,7)
places=[8,16,24,32]
dx=[32,29,25,19,12,4,-4,-12,-19,-25,-29,-32]
dy=[-6,-4,-3,-2,-1,-1,-1,-1,-2,-3,-4,-6]
def active():
    return [(g.rd16(0x07000000+i*8),g.rd16(0x07000000+i*8+2),g.rd16(0x07000000+i*8+4)) for i in range(64,112) if g.rd16(0x07000000+i*8)&255!=160]
def check(objects,expected,label,tick):
    assert len(objects)==len(expected),(label,tick,'OBJ count',len(objects),len(expected))
    for (a0,a1,a2),(table,age,x,y) in zip(objects,expected):
        part,hh=assets[table];f=0
        while age>=hh[f]:age-=hh[f];f+=1
        off,n,geom,_,ox,oy,_=part;shape,size=geom&3,(geom>>2)&3
        hw=[[8,16,32,64],[16,32,32,64],[8,8,16,32]][shape][size]//2
        hv=[[8,16,32,64],[8,8,16,32],[16,32,32,64]][shape][size]//2
        x=(x+trunc((ox+hw)*3,4)-hw)&511;y=(y+trunc((oy+hv)*5,7)-hv)&255
        assert a0==y|0x100|(shape<<14) and a1==x|(31<<9)|(size<<14),(label,tick,'placement',table)
        assert a2>>12==15 and (a2>>10)&3==1,(label,tick,'palette/priority')
        assert g.mem(0x06010000+(a2&1023)*32,n)==bank[off+f*n:off+(f+1)*n],(label,tick,'source pixels',table,f)

def case(id_,mode):
    label=('TELELE' if id_==0x1d else 'SHADOW')+'_'+mode.upper()
    cid=7 if mode=='wren' else 0
    specs=[(cid,100,0),(5,100,0),(3,100,0),(6,100,0),(9,100,0)]
    ability_fixture(True,id_,specs,uses=0 if mode=='depleted' else 1)
    g.wr8(pp+24,37)
    if mode=='sealed':g.wr8(pp+27,16)
    if mode=='unknown':g.wr8(md+0xf500+cid*128+0x62,0)
    if mode=='dead_caster':g.wr16(pp,0)
    for i in range(4):
        call('battle_load_enemy',(9,ee+i*62))
        for off in (0,2,14):g.wr16(ee+i*62+off,1000)
        g.wr16(ee+i*62+8,963 if mode=='repeat' else 2000)
        g.wr8(ee+i*62+32,10 if mode=='floor' else 100)
        g.wr8(ee+i*62+25,1 if mode=='repeat' else 200)
        g.wr8(ee+i*62+24,255 if mode=='miss' or (mode=='mixed' and i==1) else 0)
        g.wr8(ee+i*62+27,4 if mode=='mixed' and i==2 else 0)
        if mode=='mixed' and i==3:g.wr16(ee+i*62,0)
        for j in range(14):g.wr8(ee+i*62+34+j,0 if mode=='immune' else 4)
        if mode=='floor':g.wr16(ee+i*62+14,10)
    g.wr32(symbols['battle_enemy_count'],4);g.put(symbols['battle_enemy_placement'],bytes(places))
    g.put(symbols['battle_enemy_reaction_flags'],bytes(4))
    call('battle_anim_reset');call('battle_ui_bind',(pp,5,ee,0));call('battle_compose_igg',(pp,5,ee,0,0));call('battle_present_page0')
    for i in range(64,112):g.put(0x07000000+i*8,bytes([160,0,0,0,0,0,0,0]))
    g.wr8(symbols['battle_defer_death_audio'],1);w32(0xef0c,0x12345678)
    g.put(cp,bytes([3,id_,0,255]));g.wr32(tp,0)
    party=g.mem(pp,310);enemies=bytearray(g.mem(ee,248));saved=bytearray(g.mem(md+0xf100,0xa00));seed=0x12345678
    allowed=mode not in ('depleted','unknown','dead_caster')
    hits=(1 if mode=='mixed' else 0 if mode in ('miss','immune') else 15) if allowed else 0
    if allowed:
        saved[0x400+cid*128+0x6a]-=1
        for i in range(2 if mode=='mixed' else 4):seed=rotate(seed)
        for i in range(4):
            if hits>>i&1:
                if id_==0x1d:enemies[i*62+8:i*62+10]=max(0,int.from_bytes(enemies[i*62+14:i*62+16],'little')-37).to_bytes(2,'little')
                else:enemies[i*62+25]=max(1,enemies[i*62+32]-37)
    shadow=id_==0x1e;recovery=31+(126 if hits else 26)
    duration=(recovery+21 if shadow else 31+(131 if hits else 31)) if allowed else 0
    args=(pp,5,ee,0,cp,tp);samples=[]
    if allowed:
        begin('battle_party_source_debuff_skill',args)
        for tick in range(duration):
            assert run_until_break(g,symbols['battle_action_idle_after_wait'],20),(label,tick,'early return')
            expected=[]
            if shadow:
                if 30<=tick<54:expected.append((0x25B0C8,tick-30,gx(0x130),gy(0x110 if cid==7 else 0x100)))
            else:
                if 16<=tick<44:expected.append((0x25D916,tick-16,gx(0x120),gy(0x100)))
                if 30<=tick<53:expected.append((0x25AF70,tick-30,gx(0x120),gy(0x100)))
            if hits:
                for wave in range(12):
                    age=tick-(108-6*wave)
                    if 0<=age<24:
                        expected += [(0x25CFC0,age,gx(0x80+places[i]*8+dx[wave]),gy(0xd8+dy[wave])) for i in range(4) if hits>>i&1]
            if shadow and recovery<=tick<recovery+19:expected.append((0x25B008,tick-recovery,gx(0x130),gy(0x110 if cid==7 else 0x100)))
            check(active(),expected,label,tick)
            cue=0xcd if not shadow and tick==16 else (0xcb if shadow else 0xbe) if tick==30 else 0xca if hits and tick==42 else None
            if cue is not None:assert g.rd8(md+0x500a)==cue,(label,tick,'cue')
            if not hits:assert g.rd8(md+0x500a)!=0xca,(label,tick,'miss success cue')
            assert g.mem(pp,310)==party and g.mem(ee,248)==enemies,(label,tick,'results changed twice')
            samples.append({'tick':tick,'objects':len(expected)})
            if tick==60 and mode=='mixed':shot(label+'_ACTIVE')
            ret=g.reg(14);assert run_until_break(g,ret,20),(label,tick,'idle return')
        assert run_until_break(g,0x08800000,20) and g.reg(0)==1,label+' return'
    else:assert call('battle_party_source_debuff_skill',args)==0,label+' blocked dispatch'
    assert g.mem(pp,310)==party and g.mem(ee,248)==enemies and g.mem(md+0xf100,0xa00)==saved,label+' complete guards'
    assert g.mem(md+0xef0c,4)==seed.to_bytes(4,'big'),label+' RNG stream'
    assert g.rd8(symbols['battle_defer_death_audio'])==1,label+' defer ownership'
    if allowed:
        assert not active(),label+' OBJ cleanup'
        assert g.rd8(symbols['battle_party_pose_actor'])==255 and g.rd8(symbols['battle_party_x_override_actor'])==255,label+' pose cleanup'
        assert call('battle_party_source_debuff_skill',args)==0,label+' depleted repeated dispatch'
        assert g.mem(ee,248)==enemies and g.mem(md+0xf100,0xa00)==saved and g.mem(md+0xef0c,4)==seed.to_bytes(4,'big'),label+' repeat guards'
    assert g.mem(symbols['battle_enemy_reaction_flags'],4)==bytes(18 if hits>>i&1 else 0 for i in range(4)),label+' reaction flags'
    results.append({'case':label,'hit_mask':hits,'native_updates':duration,'source_OAM_pixels_palette_anchors_cues_per_update':allowed,
                    'whole_actor_saved_RNG_charge_and_cleanup_guards':True,'trace':samples})
    print('PASS',label,'hits',hits,'updates',duration,flush=True)
for id_ in (0x1d,0x1e):
    for mode in ('all_hit','mixed','miss','immune','floor','repeat','depleted','sealed','unknown','dead_caster','wren'):case(id_,mode)
report={'core':'upstream mGBA 0.10.5','rom':rom_id,'cases':results,'native_cases':len(results),
        'dispatcher_source_route_verified':True,'normal_playthrough_physical_hardware_or_full_68000_clock_parity_verified':False}
(O/(prefix+'_MGBA_DEBUFF_SKILLS.json')).write_text(json.dumps(report,indent=2)+'\n')
print('PASS',len(results),'Telele/Shadow cases',flush=True)
