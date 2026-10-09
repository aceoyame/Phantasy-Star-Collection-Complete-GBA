#!/usr/bin/env python3
"""Native Vision commands/controller OAM trace and five stat-debuff commands."""
from pathlib import Path
import re
helpers=Path(__file__).with_name('check_r46_special_recovery_mgba.py')
base=helpers.read_text().split('\ndef special_fixture(')[0].replace("default='CP23'","default='CP25'")
exec(compile(base,str(helpers),'exec'),globals())
def le16(buf,off,value):buf[off:off+2]=(value&65535).to_bytes(2,'little')
def vision_case(name,caster=0,name_byte=None,base_dex=41,controller=False,trace=False,available=True,known=True,specs=None):
    specs=specs or [(caster,100,0),(6,100,0),(1,0,4),(7,100,0),(4,0,0)]
    p=ability_fixture(True,0x2f,specs,uses=1 if available else 0)
    for i,(cid,hp,st) in enumerate(specs):
        d=0xf500+cid*128
        g.wr8(md+d+0x22,base_dex);g.wr8(md+d+0x23,7)
        g.wr8(pp+i*62+26,7);g.wr8(pp+i*62+33,base_dex)
        g.wr8(pp+i*62+24,200)
    if name_byte is not None:g.wr8(md+0xf500+specs[0][0]*128,name_byte)
    if not known:g.wr8(md+0xf500+specs[0][0]*128+0x62,0)
    first=g.rd8(md+0xf500+specs[0][0]*128)
    saved=bytearray(g.mem(md+0xf100,0xa00));want=bytearray(g.mem(pp,len(specs)*62));enemy=g.mem(ee,62);seed=g.mem(md+0xef0c,4)
    allowed=available and known and bool(specs[0][1]);mask=0
    if allowed:
        saved[0x400+specs[0][0]*128+0x6a]-=1
        for i,(cid,hp,st) in enumerate(specs):
            if cid not in (6,7) and not st&0x44:
                mask|=1<<i;want[i*62+26]=(base_dex+first)&255
                saved[0x400+cid*128+0x23]=want[i*62+26]
    g.put(cp,bytes([3,0x2f,4,0xff]));g.wr32(tp,0)
    args=(pp,len(specs),ee,0,cp,tp);samples=[]
    if controller and trace:
        for i in range(16,26):g.put(0x07000000+i*8,bytes([160,0,0,0,0,0,0,0]))
        begin('battle_party_vision_source_skill',args)
        assert run_until_break(g,symbols['battle_apply_vision_skill'],60),name+' effect entry'
        ret=g.reg(14)
        assert run_until_break(g,ret,20),name+' effect return'
        manifest=json.loads((S/'generated_precomp/party_attack/manifest.json').read_text())['vision']
        holds=manifest['effect']['holds'];assert holds==[2,4,3,3,4,5,6]
        header=(S/'gba/precomp_assets.h').read_text()
        pixels_off=int(re.search(r'#define PS4_PRECOMP_VISION_EFFECT_PIXELS_OFF (\d+)u',header)[1])
        for tick in range(80):
            assert run_until_break(g,symbols['battle_action_idle_after_wait'],10),(name,tick,'idle missing')
            got=0
            for i in range(5):
                active=[g.rd16(0x07000000+(16+i*2+j)*8)&255!=160 for j in (0,1)]
                assert active[0]==active[1],(name,tick,i,active)
                if active[0]:got|=1<<i
            assert got==(mask if 30<=tick<57 else 0),(name,tick,got,mask)
            frame=None
            if got:
                age=tick-30
                for fr,hold in enumerate(holds):
                    if age<hold:frame=fr;break
                    age-=hold
                i=next(i for i in range(5) if got>>i&1)
                tile=g.rd16(0x07000000+(16+i*2)*8+4)&1023
                assert g.mem(0x06010000+tile*32,1024)==g.mem(symbols['ps4_precomp_assets']+pixels_off+frame*1024,1024),(name,tick,'frame bytes')
            samples.append({'tick':tick,'active_party_mask':got,'frame':frame})
            if tick==36:shot(name+'_ACTIVE')
            ret=g.reg(14);assert run_until_break(g,ret,10),(name,tick,'idle return')
        assert run_until_break(g,0x08800000,10),name+' controller return'
        assert g.reg(0)==1,name
        assert all(g.rd16(0x07000000+i*8)&255==160 for i in range(16,26)),name+' cleanup'
    elif controller:assert call('battle_party_vision_source_skill',args)==1,name
    else:
        g.wr8(symbols['battle_suppress_skill_source_audio'],1)
        call('battle_execute_cmd',args)
    assert g.mem(pp,len(want))==want and g.mem(ee,62)==enemy,name+' actor/enemy mismatch'
    assert g.mem(md+0xf100,0xa00)==saved and g.mem(md+0xef0c,4)==seed,name+' saved/RNG mismatch'
    call('battle_execute_cmd',args)
    assert g.mem(pp,len(want))==want and g.mem(md+0xf100,0xa00)==saved,name+' depleted repeated command'
    results.append({'case':name,'route':'complete Vision controller with per-update OAM/asset frame trace' if trace else 'complete Vision controller' if controller else 'actual battle_execute_cmd','caster_encoded_first_name_byte':first,'eligible_party_mask':mask,'source_30_update_child_delay_and_27_update_visual_span_checked':trace,'whole_actor_enemy_saved_RNG_guards':True,'trace':samples})
    print('PASS',name,flush=True)

def debuff_case(name,skill,id_,floor=False,available=True,sealed=False,known=True,controller=False):
    specs=[(0,100,0),(6,100,0),(1,100,0)]
    p=ability_fixture(skill,id_,specs,uses=1 if available else 0,tp_value=None if available else 0)
    if sealed:g.wr8(pp+27,0x10);g.wr8(md+0xf500+22,0x10)
    if skill and not known:g.wr8(md+0xf500+0x62,0)
    g.wr8(pp+24,37);g.wr8(pp+25,73)
    for i in range(4):
        call('battle_load_enemy',(9,ee+i*62))
        g.wr16(ee+i*62,0 if i==3 else 100)
        g.wr16(ee+i*62+14,10 if floor else 100)
        g.wr8(ee+i*62+32,10 if floor else 100)
        g.wr16(ee+i*62+8,201);g.wr8(ee+i*62+25,201)
        g.wr8(ee+i*62+24,255 if i==1 else 0)
        g.wr8(ee+i*62+27,4 if i==2 else 0)
        for e in range(14):g.wr8(ee+i*62+34+e,4)
    g.wr32(symbols['battle_enemy_count'],4)
    g.wr8(symbols['battle_suppress_skill_source_audio'],1)
    saved=bytearray(g.mem(md+0xf100,0xa00));party=bytearray(g.mem(pp,len(specs)*62));want=bytearray(g.mem(ee,4*62));seed0=g.mem(md+0xef0c,4)
    allowed=available and known and not sealed
    effect=original_rom[p];stat=73 if skill and original_rom[p+1]&127==3 else 37
    if allowed:
        if skill:saved[0x400+0x6a]-=1
        else:le16(party,4,int.from_bytes(party[4:6],'little')-original_rom[p+1])
        # Target 0 succeeds for every RNG word; target 1 always fails. Source
        # status/HP gates skip targets 2/3 before RNG2. Whole structs stay guarded.
        if effect==3:le16(want,8,max(0,(10 if floor else 100)-stat))
        else:want[25]=max(1,(10 if floor else 100)-stat)
    g.put(cp,bytes([3 if skill else 2,id_,0,0xff]));g.wr32(tp,0)
    args=(pp,len(specs),ee,0,cp,tp)
    if controller:
        assert allowed
        desc=symbols['ps4_source_skill' if skill else 'ps4_source_tech']+id_*6
        assert call('battle_party_source_generated_ability',args+(desc,int(skill)))==1,name
    else:call('battle_execute_cmd',args)
    assert g.mem(pp,len(party))==party and g.mem(ee,len(want))==want,name+' actor/enemy mismatch'
    assert g.mem(md+0xf100,0xa00)==saved,name+' resource/saved mismatch'
    # Animation paths can use their own clocks; command-only cases prove the
    # exact two original RNG2 calls and no rolls for dead/absent targets.
    if not controller:
        seed=int.from_bytes(seed0[:2],'big')
        if allowed:
            for _ in range(2):seed=(seed>>1)|((seed&1)<<15)
        assert g.mem(md+0xef0c,4)==seed.to_bytes(2,'big')+seed0[2:],name+' RNG call count'
    call('battle_execute_cmd',args)
    assert g.mem(pp,len(party))==party and g.mem(ee,len(want))==want and g.mem(md+0xf100,0xa00)==saved,name+' repeated depleted command'
    results.append({'case':name,'route':'actual generated ability controller' if controller else 'actual battle_execute_cmd','selected_caster_stat':stat,'success_miss_dead_absent_targets_guarded':True,'whole_party_enemy_saved_guards':True,'exact_two_RNG2_calls_verified':not controller and allowed})
    print('PASS',name,flush=True)

for cid,label in [(0,'CHAZ'),(2,'HAHN'),(5,'RIKA'),(8,'RAJA'),(9,'KYRA')]:vision_case('VISION_'+label,caster=cid)
vision_case('VISION_NAME_BYTE_WRAP',name_byte=254,base_dex=250)
vision_case('VISION_ZERO_NAME_BYTE',name_byte=0)
vision_case('VISION_NO_USES',available=False)
vision_case('VISION_UNKNOWN_SKILL',known=False)
vision_case('VISION_DEAD_CASTER',specs=[(0,0,4),(8,100,0)])
vision_case('VISION_CONTROLLER_TARGET_DELAY',controller=True,trace=True)
vision_case('VISION_CONTROLLER_RIKA_NAME',caster=5,controller=True)
vision_case('VISION_NO_HUMAN_TARGETS_COMMAND',specs=[(6,100,0),(7,100,0)])
vision_case('VISION_NO_HUMAN_TARGETS_CONTROLLER',specs=[(6,100,0),(7,100,0)],controller=True,trace=True)
records=[(False,0x14,'GELUN'),(False,0x15,'DORAN'),(True,0x1c,'ILLUSION'),(True,0x1d,'TELELE'),(True,0x1e,'SHADOW')]
for skill,id_,label in records:
    debuff_case(label+'_HIT_MISS_DEAD_ABSENT',skill,id_)
    debuff_case(label+'_SOURCE_FLOOR',skill,id_,floor=True)
    debuff_case(label+'_NO_RESOURCE',skill,id_,available=False)
    if skill:debuff_case(label+'_UNKNOWN_SKILL',skill,id_,known=False)
    else:debuff_case(label+'_SEALED_CASTER',skill,id_,sealed=True)
report={'core':'upstream mGBA 0.10.5','fresh_current_ROM_boot':not a.reuse,'matching_ROM_state_crc_verified':True,'cases':results,'full_original_controller_timing_normal_save_playthrough_or_physical_persistence_verified':False}
(O/(prefix+'_MGBA_STATS.json')).write_text(json.dumps(report,indent=2)+'\n')
print('PASS',len(results),'native Vision/stat cases')
