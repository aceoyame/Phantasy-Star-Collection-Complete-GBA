#!/usr/bin/env python3
"""Native support commands/controllers using one fresh, CRC-identified ROM boot.
Reuse the established mGBA fixture helpers without running their case list.
"""
from pathlib import Path
helpers=Path(__file__).with_name('check_r46_special_recovery_mgba.py')
base=helpers.read_text().split('\ndef special_fixture(')[0].replace("default='CP23'","default='CP24'")
exec(compile(base,str(helpers),'exec'),globals())
records=[(False,id_,name) for id_,name in [(0x1e,'SHIFT'),(0x1f,'SANER'),(0x20,'DEBAN'),(0x21,'FEEVE')]]
records += [(True,id_,name) for id_,name in [(0x26,'BARRIER'),(0x27,'WAR_CRY'),(0x28,'BLESSING'),(0x29,'WARLA'),(0x35,'BARRIER2')]]
mix=[(0,100,0),(6,100,0),(1,0,4),(7,0,0x40),(8,100,0x13)]
def le16(buf,off,value):
    buf[off:off+2]=(value&65535).to_bytes(2,'little')
def support_fixture(skill,id_,specs,wrap=False,available=True,known=True,sealed=False):
    p=ability_fixture(skill,id_,specs,uses=2 if available else 0)
    cost=original_rom[p+1]
    for i,(cid,hp,st) in enumerate(specs):
        d=0xf500+cid*128;j=pp+i*62
        for ramoff,actoroff,value in [(0x19,23,201),(0x1c,24,37),(0x1f,25,250 if wrap else 30+i),(0x22,26,41+i)]:
            g.wr8(md+d+ramoff,value);g.wr8(j+actoroff,value)
        g.wr8(j+32,250 if wrap else 30+i);g.wr8(j+33,41+i)
        for ramoff,actoroff,baseoff,value in [(0x24,8,14,65530 if wrap else 111+i),(0x28,10,16,65531 if wrap else 71+i),(0x2c,12,18,65532 if wrap else 61+i)]:
            w16(d+ramoff,value);w16(d+ramoff+2,5+i)
            g.wr16(j+actoroff,5+i);g.wr16(j+baseoff,value)
        for e in range(14):
            g.wr8(md+d+0x30+e*2,2+e);g.wr8(md+d+0x31+e*2,3+e)
            g.wr8(j+34+e,2+e);g.wr8(j+48+e,3+e)
        w16(d+18,2*cost if not skill and available else 0)
        g.wr16(j+4,2*cost if not skill and available else 0)
    if sealed:g.wr8(pp+27,0x10);g.wr8(md+0xf500+specs[0][0]*128+22,0x10)
    if skill and not known:g.wr8(md+0xf500+specs[0][0]*128+0x62,0)
    return p

def expected_support(want,saved,p,skill,specs,chosen,available=True):
    if not available:return []
    cid=specs[0][0];d=0x400+cid*128
    if skill:saved[d+0x6a]-=1
    else:le16(want,4,int.from_bytes(want[4:6],'little')-original_rom[p+1])
    selector=original_rom[p+1] if skill else 2
    stat=want[23] if selector==1 else want[24]
    effect,range_=original_rom[p],original_rom[p+2]&15
    eligible=[]
    for i,(cid,hp,st) in enumerate(specs):
        if range_==3 and i!=0:continue
        if range_ in (4,6,8) and i!=(chosen if chosen<len(specs) else 0):continue
        if st&0x44:continue
        if range_ in (4,5) and cid in (6,7):continue
        if range_ in (6,7) and cid not in (6,7):continue
        eligible.append(i);d=0x400+cid*128;j=i*62
        if effect in (9,10,11):
            baseoff,liveoff,actoroff={9:(0x24,0x26,8),10:(0x28,0x2a,10),11:(0x2c,0x2e,12)}[effect]
            value=(read16(saved,d+baseoff)+stat)&65535
            write16(saved,d+liveoff,value);le16(want,j+actoroff,value)
        elif effect==12:
            value=(saved[d+0x1f]+stat)&255;saved[d+0x20]=value;want[j+25]=value
        else:
            assert effect==13 and original_rom[p+3]==11
            saved[d+0x39]=1;want[j+48+4]=1
    return eligible

def battle_case(name,skill,id_,specs=mix,chosen=4,wrap=False,controller=False,available=True,known=True,sealed=False):
    p=support_fixture(skill,id_,specs,wrap,available,known,sealed)
    saved=bytearray(g.mem(md+0xf100,0xa00));want=bytearray(g.mem(pp,len(specs)*62));enemy=g.mem(ee,62)
    allowed=available and known and not sealed and bool(specs[0][1])
    eligible=expected_support(want,saved,p,skill,specs,chosen,allowed)
    g.put(cp,bytes([3 if skill else 2,id_,chosen,0xff]));g.wr32(tp,0)
    args=(pp,len(specs),ee,0,cp,tp)
    if controller:
        assert allowed and eligible
        if skill and id_ in (0x26,0x27,0x28,0x29,0x35):
            assert call('battle_party_source_support_skill',args)==1,name
            if id_!=0x27:
                count=g.rd8(symbols['battle_ability_track_count'])
                assert count<=10
                nts=[g.rd8(0x02033d80+32*i+8) for i in range(count)]
                expected={0x26:[1,len(eligible),1],0x35:[1,len(eligible),1],0x28:[2,len(eligible)],0x29:[1,1,len(eligible)]}[id_]
                assert nts==expected,(name,nts,expected)
        else:
            desc=symbols['ps4_source_skill' if skill else 'ps4_source_tech']+6*id_
            assert call('battle_party_source_generated_ability',args+(desc,int(skill)))==1,name
    else:call('battle_execute_cmd',args)
    assert g.mem(pp,len(want))==want and g.mem(ee,62)==enemy,name+' complete actor/enemy mismatch'
    assert g.mem(md+0xf100,0xa00)==saved,name+' saved records or charge mismatch'
    # Verify the actual presentation target helper independently of rendering.
    xs,ys=0x02033b80,0x02033ba0
    g.put(xs,bytes([0xa5])*40)
    nt=call('battle_source_ability_points',(pp,len(specs),ee,0,cp,0,xs,ys))
    assert nt==len(eligible) if allowed else True,(name,nt,eligible)
    assert g.mem(md+0xf100,0xa00)==saved and g.mem(pp,len(want))==want,name+' point helper mutated records'
    if controller and id_ in (0x26,0x28,0x1f):
        call('waitvb');call('waitvb');shot(name)
    if allowed:
        # A second confirmed use spends once, resets from base, and never stacks.
        expected_support(want,saved,p,skill,specs,chosen)
        call('battle_execute_cmd',args)
        assert g.mem(pp,len(want))==want and g.mem(md+0xf100,0xa00)==saved,name+' repeated buff stacked'
        call('battle_execute_cmd',args)
        assert g.mem(pp,len(want))==want and g.mem(md+0xf100,0xa00)==saved,name+' depleted buff executed'
    results.append({'case':name,'route':'native complete support controller' if controller else 'actual battle_execute_cmd','eligible_targets':eligible,'complete_actor_enemy_saved_record_guards':True,'repeat_nonstacking_depletion_checked':allowed,'native_target_points_checked':True})
    print('PASS',name,flush=True)

for skill,id_,name in records:
    battle_case(name+'_COMMAND',skill,id_)
    battle_case(name+'_BYTE_WORD_WRAP',skill,id_,wrap=True)
    battle_case(name+'_CONTROLLER',skill,id_,controller=True)
battle_case('SHIFT_ANDROID_TARGET_SPENT',False,0x1e,chosen=1)
battle_case('SHIFT_DEAD_TARGET_SPENT',False,0x1e,chosen=2)
for skill,id_,name in records:
    battle_case(name+'_NO_RESOURCE',skill,id_,available=False)
    if skill:battle_case(name+'_UNKNOWN_SKILL',skill,id_,known=False)
battle_case('SHIFT_SEALED_CASTER',False,0x1e,sealed=True)
battle_case('BARRIER_DEAD_CASTER',True,0x26,specs=[(0,0,4),(6,100,0),(1,100,0)])
# Feeve updates the cached restore byte too; a later source stat restore must
# consume it, leaving unrelated properties/actors and records guarded.
p=support_fixture(False,0x21,mix)
g.put(cp,bytes([2,0x21,4,0xff]));g.wr32(tp,0)
call('battle_execute_cmd',(pp,5,ee,0,cp,tp))
want=bytearray(g.mem(pp,5*62));saved=g.mem(md+0xf100,0xa00)
for i in (0,1,4):
    j=i*62
    want[j+23:j+27]=want[j+30:j+34]
    for off,baseoff in [(8,14),(10,16),(12,18)]:want[j+off:j+off+2]=want[j+baseoff:j+baseoff+2]
    want[j+34:j+48]=want[j+48:j+62]
    call('battle_apply_friendly',(pp,5,0,i,0x27,0,8))
assert g.mem(pp,5*62)==want and g.mem(md+0xf100,0xa00)==saved,'Feeve subsequent restore'
results.append({'case':'FEEVE_THEN_STAT_RESTORE','route':'actual native buff command followed by existing RestoreStats','cached_restore_property_coherent':True})
report={'core':'upstream mGBA 0.10.5','fresh_current_ROM_boot':not a.reuse,'matching_ROM_state_crc_verified':True,'cases':results,'normal_save_playthrough_exact_animation_timing_or_physical_persistence_verified':False}
(O/(prefix+'_MGBA_SUPPORT.json')).write_text(json.dumps(report,indent=2)+'\n')
print('PASS',len(results),'native support cases')
