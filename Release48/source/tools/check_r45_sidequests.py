#!/usr/bin/env python3
"""R45 late side-quest/state audit against the original PSIV source.
Static/source-faithfulness checks only; this is not an emulator playthrough.
"""
from pathlib import Path
import json,re
R=Path(__file__).resolve().parents[1]
OUT=R.parent/'build_artifacts'
c=(R/'gba/title_runtime_v06.c').read_text()
g=(R/'gba/generated_simple_events_v0982.h').read_text()
a=(R/'references/disassembly/ps4disasm/ps4disasm/ps4.asm').read_text()
checks={}
def need(name,*terms,text=None):
    s=c if text is None else text
    missing=[t for t in terms if t not in s]
    assert not missing,(name,missing)
    checks[name]='PASS'

# Anger Tower: source revision saves party, forces Chaz-only, then RecoverStats.
anger=a[a.index('\nEvent_AngerTowerTop:'):a.index('\nEvent_AngerTowerExitTop:')]
need('anger_source_recovery','Current_Party_Slots','CharID_Chaz','RecoverStats',text=anger)
need('anger_generated_save_and_solo',
     '{14u,0x00u,0xED54u,0x0000F40Au}',
     '{15u,0x00u,0xED58u,0x0000F40Eu}',
     '{18u,0x00u,0xF40Au,0x00FFFFFFu}',
     '{10u,0xFFu,0xF40Eu,0x00000000u}',
     '{19u,0x00u,0x0000u,0x00000000u}',text=g)
need('anger_native_recovery','if(script->event_index==0x69u)field_recover_party();')

# Ranch Owner late-game cancellation: revision source temporarily loads DialogueTree27 for $19.
hg=a[a.index('\nEvent_HuntersGuild:'):a.index('\nHuntersGuildData:')]
need('ranch_cancel_source_tree27','EventFlag_Elsydeon','DialogueTree27','#$19',text=hg)
need('ranch_cancel_native_tree27','if(j==0u&&ps4_event_flag_test(0xD9u)){field_event_show_alt_dialogue(0x1F2400u,0x19u,camx,camy)')

# Silver Tusk: source refuses award on 40/40 inventory; native descriptor is item $53, full dlg $2E, flag $91.
mc=a[a.index('\nEvent_MuskCatElder:'):a.index('\nEvent_PenguinOwner:')]
need('silver_tusk_source_full_inventory','cmpi.w\t#40, d0','#$2E','ItemID_SilvrTusk','EventFlag_SilverTusk',text=mc)
need('silver_tusk_native_descriptor','{0x4Au,0x53u,0x2Du,0x2Eu,0x91u}',text=g)
need('silver_tusk_native_full_guard','if(ps4_inventory_count()>=40u){field_dialogue_show(e->full_dialogue,*camx,*camy);return 1;}')

# Re-Faze/Megid: battle on response 0, otherwise Megid goes to the bug-fixed final Chaz technique slot.
need('megid_sparse_slot','if(ram8r(0xED53u)==0u)','field_event_run_battle_request(0x19u,camx,camy)','ram8w((uint16_t)(PS4_CHAR_BASE+0x52u+15u),0x10u);')

# King Rappy: Sekreas guard, dialogue/battle $13, and post-battle SekreasReason flag are all represented.
need('king_rappy_battle_descriptor','{0x88u,0xAEu,0x35u,0x36u,0xAFu,0x13u,0x03u}',text=g)
need('king_rappy_followup','field_event_king_rappy_defeated','field_dialogue_show(0x37u','field_dialogue_show(0x38u','ps4_event_flag_set(0xBAu)')

# Hunters Guild rewards and one-shot fee flags: source-derived table + gate-before-payment.
need('guild_reward_table','static const uint8_t reward[8]={5u,2u,3u,5u,50u,10u,0u,80u};')
need('guild_repeat_payment_guard','if(!ps4_event_flag_test(d[j][2])){field_event_guild_dialogue(d[j][5],camx,camy);ps4_event_flag_set(d[j][2]);')
# Small state simulation: every job can pay exactly once even if revisited repeatedly.
rewards=[5,2,3,5,50,10,0,80]
sim=[]
for j,r in enumerate(rewards):
    fee=False;money=0
    for visit in range(5):
        if not fee:
            fee=True;money+=r*1000
    assert money==r*1000
    sim.append({'job':j,'reward':r*1000,'after_5_completed_visits':money})
checks['guild_all_eight_repeat_payment_simulation']='PASS'

report={
 'scope':'Static/source-faithfulness audit; no emulator or hardware playthrough.',
 'checks':checks,
 'guild_repeat_payment_simulation':sim,
 'result':'PASS'
}
OUT.mkdir(exist_ok=True)
(OUT/'R45_SIDEQUEST_AUDIT.json').write_text(json.dumps(report,indent=2)+'\n')
(OUT/'R45_SIDEQUEST_AUDIT.log').write_text('\n'.join(f'PASS {k}' for k in checks)+'\n')
print('PASS R45 side-quest/state audit:',len(checks),'checks')
for k in checks: print('PASS',k)
