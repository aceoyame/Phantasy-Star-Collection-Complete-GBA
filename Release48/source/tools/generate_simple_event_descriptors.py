#!/usr/bin/env python3
"""Generate exact PSIV EventPtrs dialogue+EventFlag descriptors.

This is static translation, not a runtime 68000 interpreter.  The generator only
accepts event routines whose complete body is one of the exact source templates:

    moveq #dialogue,d0
    jsr   Event_GetAndRunDialogue
    move{q,b,w} #flag,d0
    jmp   EventFlags_Set

Anything with extra choreography, item/party changes, battles, map edits, etc. is
rejected and remains in the explicit source-driven executor.
"""
from __future__ import annotations
import argparse, hashlib
from pathlib import Path

EXPECTED = '511f35cc11f88316f8b8940e28ab298bd75a4da193672a80172884d6eb913b6a'
EVENT_PTRS = 0x5A2B4
EVENT_COUNT = 0xA1
RUN_EVENTS = 0x560E8
RUN_EVENT_COUNT = 0x80
EVENT_FLAG_TEST = bytes.fromhex('4e b9 00 05 76 24')
CHEST_FLAG_TEST = bytes.fromhex('4e b9 00 05 76 2e')
TEMP_FLAG_TEST = bytes.fromhex('4e b9 00 05 76 38')
RUN_EVENT_NOEVENT = 0x57620
FACE_OBJECT_CALL = bytes.fromhex('4e b9 00 05 a9 36')
DIALOGUE_CALL = bytes.fromhex('4e b9 00 05 ac 66')
RESUME_DIALOGUE_CALL = bytes.fromhex('4e b9 00 05 ac 6c')
QUICK_DIALOGUE_CALL = bytes.fromhex('4e b9 00 05 ac dc')
DIALOGUE_TREE_LOAD = bytes.fromhex('4e b9 00 05 3f 00')
FLAG_SET_JMP = bytes.fromhex('4e f9 00 05 76 66')
FLAG_SET_CALL = bytes.fromhex('4e b9 00 05 76 66')
FLAG_CLEAR_CALL = bytes.fromhex('4e b9 00 05 76 a8')
TEMP_FLAG_CLEAR_JMP = bytes.fromhex('4e f9 00 05 76 bc')
TEMP_FLAG_SET_CALL = bytes.fromhex('4e b9 00 05 76 7a')
FIND_ITEM_CALL = bytes.fromhex('4e b9 00 05 b1 00')
COMPACT_INVENTORY_CALL = bytes.fromhex('4e b9 00 05 cb 72')
RECOVERY_EVENT = 0x06D37C
PALETTE_ALARM_DOWN_CALL = bytes.fromhex('4e b9 00 05 b2 be')
PALETTE_ALARM_RESTORE_CALL = bytes.fromhex('4e b9 00 05 b3 0e')
PALETTE_FIRST32_BLACK_CALL = bytes.fromhex('4e b9 00 05 b2 22')
FIELD_GFX_RELOAD_CALL = bytes.fromhex('4e b9 00 05 1a dc')

def read_ptr(data: bytes, idx: int) -> int:
    o = EVENT_PTRS + idx * 4
    return int.from_bytes(data[o:o+4], 'big')

def classify_noop(body: bytes):
    """Exact source no-event routine: RTS only."""
    return body == b'\x4e\x75'


def classify(body: bytes):
    if len(body) not in (16, 18):
        return None
    if len(body) < 14 or body[0] != 0x70 or body[2:8] != DIALOGUE_CALL or body[-6:] != FLAG_SET_JMP:
        return None
    dialogue = body[1]
    mid = body[8:-6]
    if len(mid) == 2 and mid[0] == 0x70:              # moveq #imm,d0
        flag = mid[1]
    elif len(mid) == 4 and mid[:2] == b'\x10\x3c':  # move.b #imm,d0
        flag = int.from_bytes(mid[2:], 'big')
    elif len(mid) == 4 and mid[:2] == b'\x30\x3c':  # move.w #imm,d0
        flag = int.from_bytes(mid[2:], 'big')
    else:
        return None
    if flag > 0xFF:
        return None
    return dialogue, flag


def classify_face_dialogue_flag(body: bytes):
    # lea SourceFieldObj,a4; lea Character_1,a3; derive opposite facing;
    # Event_UpdateObjFacing; dialogue; EventFlags_Set.
    if len(body) not in (38,40):
        return None
    if body[:2]!=b'\x49\xf8' or body[4:8]!=b'\x47\xf8\xc0\x00':
        return None
    if body[8:12]!=b'\x30\x2b\x00\x06' or body[12:16]!=b'\x08\x40\x00\x02' or body[16:22]!=FACE_OBJECT_CALL:
        return None
    if body[22]!=0x70 or body[24:30]!=DIALOGUE_CALL or body[-6:]!=FLAG_SET_JMP:
        return None
    source_obj=int.from_bytes(body[2:4], 'big');dialogue=body[23];mid=body[30:-6]
    if source_obj<0xC300 or (source_obj-0xC300)%0x40:
        return None
    slot=(source_obj-0xC300)//0x40
    if len(mid)==2 and mid[0]==0x70:
        flag=mid[1]
    elif len(mid)==4 and mid[:2] in (b'\x10\x3c',b'\x30\x3c'):
        flag=int.from_bytes(mid[2:], 'big')
    else:
        return None
    if slot>31 or flag>0xFF:
        return None
    return slot,dialogue,flag,source_obj


def decode_imm_d0(q: bytes):
    if len(q)==2 and q[0]==0x70:
        return q[1]
    if len(q)==4 and q[:2] in (b'\x10\x3c',b'\x30\x3c'):
        v=int.from_bytes(q[2:4],'big')
        return v if v<=0xFF else None
    return None

def classify_simple_battle_event(body: bytes):
    """Exact EventPtrs bodies that only dialogue/set a flag then request battle.

    RunEventBattle is selected by Event_Battle_Index ($ECFC) plus bit 3 of
    Routine_Exit_Flags ($EC27). Some source routines also set map-load bit 3 in
    $EC4E before the shared wrapper runs the battle.
    """
    # Strip final RTS and optional moveq #1,d0 return value.
    if not body.endswith(b'\x4e\x75'): return None
    q=body[:-2];return_one=0
    if q.endswith(b'\x70\x01'):
        return_one=1;q=q[:-2]
    tail=bytes.fromhex('08 f8 00 03 ec 27')
    if not q.endswith(tail): return None
    q=q[:-len(tail)]
    # move.b #battle,$ECFC
    if len(q)<6 or q[-6:-4]!=b'\x11\xfc' or q[-2:]!=b'\xec\xfc': return None
    battle=int.from_bytes(q[-4:-2],'big')&0xFF;q=q[:-6]
    mapload=0
    mapbit=bytes.fromhex('08 f8 00 03 ec 4e')
    if q.endswith(mapbit):mapload=1;q=q[:-len(mapbit)]
    # Prefix may be flag-only or standard Event_GetAndRunDialogue + flag.
    dialogue=0xFF
    # Find terminal direct EventFlags_Set call (JSR, not JMP, because battle tail follows).
    fcall=bytes.fromhex('4e b9 00 05 76 66')
    if not q.endswith(fcall):return None
    q0=q[:-6]
    flag=None
    for n in (2,4):
        if len(q0)>=n:
            v=decode_imm_d0(q0[-n:])
            if v is not None:flag=v;q0=q0[:-n];break
    if flag is None:return None
    if q0:
        dcall=bytes.fromhex('4e b9 00 05 ac 66')
        if not q0.endswith(dcall):return None
        q1=q0[:-6];dialogue=None
        for n in (2,4):
            if len(q1)>=n:
                v=decode_imm_d0(q1[-n:])
                if v is not None:dialogue=v;q1=q1[:-n];break
        if dialogue is None or q1:return None
    return dialogue,flag,battle,mapload,return_one

def classify_temp_flag_clear(body: bytes):
    # moveq #TempFlag,d0; jmp TempEveFlags_Clear ($576BC, RAM $F140).
    if len(body)==8 and body[0]==0x70 and body[2:]==TEMP_FLAG_CLEAR_JMP:
        return body[1]
    return None


def classify_dialogue_two_flags(body: bytes):
    # dialogue; EventFlags_Set(first); EventFlags_Set(second) tail-jump.
    if len(body)<24 or body[0]!=0x70 or body[2:8]!=DIALOGUE_CALL:
        return None
    dialogue=body[1];pos=8
    first=None
    for n in (2,4):
        v=decode_imm_d0(body[pos:pos+n])
        if v is not None and body[pos+n:pos+n+6]==FLAG_SET_CALL:
            first=v;pos+=n+6;break
    if first is None:return None
    second=None
    for n in (2,4):
        v=decode_imm_d0(body[pos:pos+n])
        if v is not None and body[pos+n:]==FLAG_SET_JMP:
            second=v;break
    if second is None:return None
    return dialogue,first,second


def classify_dialogue_recovery(body: bytes):
    # Event_GetAndRunDialogue; jsr Event_Recovery; rts.
    if len(body)!=16 or body[0]!=0x70 or body[2:8]!=DIALOGUE_CALL:
        return None
    if body[8:14] != b'\x4e\xb9'+RECOVERY_EVENT.to_bytes(4,'big') or body[14:]!=b'\x4e\x75':
        return None
    return body[1]


def classify_tree_dialogue_flag(body: bytes):
    """Exact alternate-dialogue-tree + dialogue + EventFlag template.

    DialogueTreesToRAM ($53F00) takes a ROM pointer in d0, Kosinski-decodes it
    to source dialogue RAM $FFFF3000, then the routine uses the ordinary
    Event_GetAndRunDialogue path and commits one EventFlag.
    """
    if len(body)<28 or body[:2]!=b'\x20\x3c' or body[6:12]!=DIALOGUE_TREE_LOAD:
        return None
    tree=int.from_bytes(body[2:6],'big')
    if tree>=0x300000:return None
    if body[12]!=0x70 or body[14:20]!=DIALOGUE_CALL:return None
    dialogue=body[13];pos=20;flag=None
    for n in (2,4):
        v=decode_imm_d0(body[pos:pos+n])
        if v is not None and body[pos+n:]==FLAG_SET_JMP:
            flag=v;break
    if flag is None:return None
    return tree,dialogue,flag


def classify_dialogue_camera_flag(body: bytes):
    """Dialogue, restore the event camera to Character_1, then set a flag.

    Source template (EventPtr $50):
      dialogue; lea Character_1,a4; move.w curr_x_pos(a4),d0;
      move.w curr_y_pos(a4),d1; jsr Event_MoveCamera; EventFlags_Set.

    Event_MoveCamera is source address $5AAEE.  Its prologue subtracts the
    Genesis screen center ($98,$58) from D0/D1 before interpolating camera
    coordinates, so this descriptor deliberately models camera restoration,
    not character movement.
    """
    if len(body)<34 or body[0]!=0x70 or body[2:8]!=DIALOGUE_CALL:
        return None
    # lea (Character_1).w,a4; move.w $30(a4),d0; move.w $34(a4),d1
    if body[8:20] != bytes.fromhex('49 f8 c0 00 30 2c 00 30 32 2c 00 34'):
        return None
    if body[20:26] != bytes.fromhex('4e b9 00 05 aa ee'):
        return None
    pos=26;flag=None
    for n in (2,4):
        v=decode_imm_d0(body[pos:pos+n])
        if v is not None and body[pos+n:]==FLAG_SET_JMP:
            flag=v;break
    if flag is None:return None
    return body[1],flag


def classify_conditional_dialogue_flag(body: bytes):
    """Exact one-shot/repeat dialogue EventPtr template.

    Source shape:
      move{q,b,w} #flag,d0; jsr EventFlags_Test; bne.s repeat
      dialogue(first); move{q,b,w} #flag,d0; jmp EventFlags_Set
    repeat:
      dialogue(repeat) via tail jump
    """
    # Current source instance ($90) is 38 bytes. Keep the classifier structural
    # so another byte-identical template with moveq/word immediates can be emitted.
    if len(body)<32:return None
    pos=0;flag=None
    for n in (2,4):
        v=decode_imm_d0(body[pos:pos+n])
        if v is not None and body[pos+n:pos+n+6]==EVENT_FLAG_TEST:
            flag=v;pos+=n+6;break
    if flag is None or pos+2>len(body) or body[pos]!=0x66:return None
    branch_pos=pos;disp=body[pos+1];pos+=2
    first_pos=pos
    if body[pos]!=0x70 or body[pos+2:pos+8]!=DIALOGUE_CALL:return None
    first=body[pos+1];pos+=8
    flag2=None
    for n in (2,4):
        v=decode_imm_d0(body[pos:pos+n])
        if v is not None and body[pos+n:pos+n+6]==FLAG_SET_JMP:
            flag2=v;pos+=n+6;break
    if flag2!=flag:return None
    repeat_pos=pos
    if branch_pos+2+disp!=repeat_pos:return None
    if pos+8!=len(body) or body[pos]!=0x70 or body[pos+2:pos+8]!=DIALOGUE_CALL.replace(b'\x4e\xb9',b'\x4e\xf9'):
        return None
    repeat=body[pos+1]
    return flag,first,repeat



def classify_object_dialogue_patch(body: bytes):
    """Dialogue event that rewrites consecutive source field-object dialogue IDs.

    Source field-object byte $14 is proven to be the dialogue selector: the
    original event/interaction code loads it directly into Event_GetAndRunDialogue.
    Accepted routines may swap in one source dialogue tree before the message and
    another afterward, then patch one or more consecutive 0x40-byte object slots
    at offset $14 and set one EventFlag.
    """
    pos=0;pre_tree=0xFFFFFFFF;post_tree=0xFFFFFFFF
    if body[pos:pos+2]==b'\x20\x3c' and body[pos+6:pos+12]==DIALOGUE_TREE_LOAD:
        pre_tree=int.from_bytes(body[pos+2:pos+6],'big');pos+=12
        if pre_tree>=0x300000:return None
    if pos+8>len(body) or body[pos]!=0x70 or body[pos+2:pos+8]!=DIALOGUE_CALL:return None
    dialogue=body[pos+1];pos+=8
    if pos+4>len(body) or body[pos:pos+2] not in (b'\x41\xf8',b'\x49\xf8'):return None
    lea_op=body[pos:pos+2];source_obj=int.from_bytes(body[pos+2:pos+4],'big');pos+=4
    if source_obj<0xC300 or (source_obj-0xC300)%0x40:return None
    start_slot=(source_obj-0xC300)//0x40
    write_op=b'\x11\x7c' if lea_op==b'\x41\xf8' else b'\x19\x7c'
    count=0;object_dialogue=None
    while pos+6<=len(body) and body[pos:pos+2]==write_op:
        value=int.from_bytes(body[pos+2:pos+4],'big')&0xFF
        disp=int.from_bytes(body[pos+4:pos+6],'big')
        if disp!=0x14+count*0x40:return None
        if object_dialogue is None:object_dialogue=value
        elif value!=object_dialogue:return None
        count+=1;pos+=6
    if not count or count>8:return None
    if body[pos:pos+2]==b'\x20\x3c' and body[pos+6:pos+12]==DIALOGUE_TREE_LOAD:
        post_tree=int.from_bytes(body[pos+2:pos+6],'big');pos+=12
        if post_tree>=0x300000:return None
    flag=None
    for n in (2,4):
        v=decode_imm_d0(body[pos:pos+n])
        if v is not None and body[pos+n:]==FLAG_SET_JMP:
            flag=v;pos=len(body);break
    if flag is None or pos!=len(body):return None
    return pre_tree,dialogue,start_slot,count,object_dialogue,post_tree,flag

def classify_quick_dialogue_battle_event(body: bytes):
    """Exact $5ACDC dialogue-then-battle source family.

    $5ACDC shares the normal event-dialogue loop but returns through the light
    cleanup that clears $ED02/$EC9A/$ECA4.  The accepted EventPtrs then set one
    story flag, optionally save a music byte, optionally BSET one authored bit
    in byte Map_Load_Flags ($EC4E), and request RunEventBattle.
    """
    if not body.endswith(b'\x4e\x75'):return None
    q=body[:-2];return_one=0
    if q.endswith(b'\x70\x01'):
        return_one=1;q=q[:-2]
    exit_tail=bytes.fromhex('08 f8 00 03 ec 27')
    if not q.endswith(exit_tail):return None
    q=q[:-6]
    if len(q)<6 or q[-6:-4]!=b'\x11\xfc' or q[-2:]!=b'\xec\xfc':return None
    battle=int.from_bytes(q[-4:-2],'big')&0xFF;q=q[:-6]
    mapload_bit=0xFF
    if len(q)>=6 and q[-6:-4]==b'\x08\xf8' and q[-2:]==b'\xec\x4e':
        mapload_bit=int.from_bytes(q[-4:-2],'big')&0xFF;q=q[:-6]
        if mapload_bit>7:return None
    saved_sound=0xFF
    if len(q)>=6 and q[-6:-4]==b'\x11\xfc' and q[-2:]==b'\xec\xec':
        saved_sound=int.from_bytes(q[-4:-2],'big')&0xFF;q=q[:-6]
    if not q.endswith(FLAG_SET_CALL):return None
    q=q[:-6];flag=None
    for n in (2,4):
        if len(q)>=n:
            v=decode_imm_d0(q[-n:])
            if v is not None:flag=v;q=q[:-n];break
    if flag is None:return None
    if len(q)!=8 or q[0]!=0x70 or q[2:8]!=QUICK_DIALOGUE_CALL:return None
    return q[1],flag,battle,mapload_bit,return_one,saved_sound


def classify_guarded_quick_battle_or_dialogue(body: bytes):
    """EventFlag guard: fallback normal dialogue, else $5ACDC + flag + battle."""
    pos=0;guard=None
    for n in (2,4):
        v=decode_imm_d0(body[pos:pos+n])
        if v is not None and body[pos+n:pos+n+6]==EVENT_FLAG_TEST:
            guard=v;pos+=n+6;break
    if guard is None or pos+2>len(body) or body[pos]!=0x67:return None
    branch_pos=pos;disp=body[pos+1];pos+=2
    # Taken-path battle body ends before the fallback dialogue target.
    fallback_pos=branch_pos+2+disp
    if fallback_pos<=pos or fallback_pos+8!=len(body):return None
    q=body[pos:fallback_pos]
    if q[-4:]!=b'\x70\x01\x4e\x75':return None
    q=q[:-4]
    exit_tail=bytes.fromhex('08 f8 00 03 ec 27')
    if not q.endswith(exit_tail):return None
    q=q[:-6]
    if len(q)<6 or q[-6:-4]!=b'\x11\xfc' or q[-2:]!=b'\xec\xfc':return None
    battle=int.from_bytes(q[-4:-2],'big')&0xFF;q=q[:-6]
    mapload_bit=0xFF
    if len(q)>=6 and q[-6:-4]==b'\x08\xf8' and q[-2:]==b'\xec\x4e':
        mapload_bit=int.from_bytes(q[-4:-2],'big')&0xFF;q=q[:-6]
        if mapload_bit>7:return None
    if not q.endswith(FLAG_SET_CALL):return None
    q=q[:-6];flag=None
    for n in (2,4):
        if len(q)>=n:
            v=decode_imm_d0(q[-n:])
            if v is not None:flag=v;q=q[:-n];break
    if flag is None or len(q)!=8 or q[0]!=0x70 or q[2:8]!=QUICK_DIALOGUE_CALL:return None
    quick_dialogue=q[1]
    fb=body[fallback_pos:]
    if fb[0]!=0x70 or fb[2:8]!=DIALOGUE_CALL.replace(b'\x4e\xb9',b'\x4e\xf9'):return None
    return guard,fb[1],quick_dialogue,flag,battle,mapload_bit


def classify_extended_battle_event(body: bytes):
    """Source-shaped battle request variants not covered by the basic template.

    Supports an optional EventFlags_Test guard (return if already set) and an
    optional direct byte write to Saved_Sound_Index ($ECEC) before the common
    map-load/battle tail.  No unknown helper calls are accepted.
    """
    if not body.endswith(b'\x4e\x75'):return None
    q=body[:-2];return_one=0
    if q.endswith(b'\x70\x01'):
        return_one=1;q=q[:-2]
    # Shared battle exit tail.
    exit_tail=bytes.fromhex('08 f8 00 03 ec 27')
    if not q.endswith(exit_tail):return None
    q=q[:-6]
    if len(q)<6 or q[-6:-4]!=b'\x11\xfc' or q[-2:]!=b'\xec\xfc':return None
    battle=int.from_bytes(q[-4:-2],'big')&0xFF;q=q[:-6]
    mapload=0;mapbit=bytes.fromhex('08 f8 00 03 ec 4e')
    if q.endswith(mapbit):mapload=1;q=q[:-6]
    saved_sound=0xFF
    if len(q)>=6 and q[-6:-4]==b'\x11\xfc' and q[-2:]==b'\xec\xec':
        saved_sound=int.from_bytes(q[-4:-2],'big')&0xFF;q=q[:-6]
    # Required EventFlags_Set immediately before optional writes.
    if not q.endswith(FLAG_SET_CALL):return None
    q=q[:-6];flag=None
    for n in (2,4):
        if len(q)>=n:
            v=decode_imm_d0(q[-n:])
            if v is not None:flag=v;q=q[:-n];break
    if flag is None:return None
    dialogue=0xFF;guard=0xFF
    if q:
        # Optional already-set guard: imm flag; EventFlags_Test; BEQ +2; RTS.
        test_call=bytes.fromhex('4e b9 00 05 76 24')
        matched=False
        for n in (2,4):
            if len(q)>=n+10:
                v=decode_imm_d0(q[:n])
                if v is not None and q[n:n+6]==test_call and q[n+6:n+10]==bytes.fromhex('67 02 4e 75'):
                    guard=v;q=q[n+10:];matched=True;break
        if q:
            # Or ordinary dialogue prefix.
            if q[0]==0x70 and q[2:8]==DIALOGUE_CALL and len(q)==8:
                dialogue=q[1];q=b''
            else:return None
        if not matched and dialogue==0xFF:return None
    # Exclude the six basic descriptors; generator emits this as a separate table.
    if guard==0xFF and saved_sound==0xFF:
        return None
    if guard!=0xFF and guard!=flag:return None
    return dialogue,flag,battle,mapload,return_one,guard,saved_sound


def classify_inventory_reward_event(body: bytes):
    """Exact packed-inventory capacity gate, dialogue, item grant and EventFlag.

    Source scans all 40 bytes at $F410, branches to the failure dialogue when
    full, otherwise runs the success dialogue, seeks the first zero slot, writes
    one item there and commits one story EventFlag.
    """
    prefix=bytes.fromhex(
        '41 f8 f4 10 70 00 72 27 4a 18 67 02 52 40 51 c9 ff f8 '
        '0c 40 00 28 65 00 00 0a')
    if len(body)<50 or body[:len(prefix)]!=prefix:return None
    pos=len(prefix)
    if body[pos]!=0x70 or body[pos+2:pos+8]!=DIALOGUE_CALL.replace(b'\x4e\xb9',b'\x4e\xf9'):return None
    full_dialogue=body[pos+1];pos+=8
    if body[pos]!=0x70 or body[pos+2:pos+8]!=DIALOGUE_CALL:return None
    success_dialogue=body[pos+1];pos+=8
    if body[pos:pos+4]!=bytes.fromhex('41 f8 f4 10'):return None
    pos+=4
    if body[pos:pos+4]!=bytes.fromhex('4a 18 66 fc'):return None
    pos+=4
    if body[pos:pos+2]!=b'\x11\x7c' or body[pos+4:pos+6]!=b'\xff\xff':return None
    item=int.from_bytes(body[pos+2:pos+4],'big')&0xFF;pos+=6
    flag=None
    for n in (2,4):
        v=decode_imm_d0(body[pos:pos+n])
        if v is not None and body[pos+n:]==FLAG_SET_JMP:
            flag=v;pos=len(body);break
    if flag is None or pos!=len(body):return None
    return item,success_dialogue,full_dialogue,flag


def classify_money_dialogue_transaction(body: bytes):
    """Meseta-gated $F5 Yes/No transaction with a source field object.

    The two source forms establish opposite affordability EventFlag predicates,
    face one field object toward Character_1, run a normal dialogue containing
    the RunText $F5 choice command, require response 0 (YES) and the source
    post-dialogue marker $3A, subtract the authored price, optionally patch the
    object's dialogue byte $14, then set one completion EventFlag.
    """
    pos=0;insufficient=None;cost=None;source_obj=None;flag_when_affordable=0
    face_pat_tail=bytes.fromhex('47 f8 c0 00 30 2b 00 06 08 40 00 02')+FACE_OBJECT_CALL
    def parse_face(at):
        if at+22>len(body) or body[at:at+2]!=b'\x49\xf8' or body[at+4:at+22]!=face_pat_tail:return None
        obj=int.from_bytes(body[at+2:at+4],'big')
        if obj<0xC300 or (obj-0xC300)%0x40:return None
        return obj,at+22

    # Form A ($4B): face; cmpi.l #price,Current_Money; set/clear fail flag.
    f=parse_face(pos)
    if f:
        source_obj,pos=f
        if pos+12>len(body) or body[pos:pos+2]!=b'\x0c\xb8' or body[pos+6:pos+10]!=b'\xf4\x38\x64\x0c':return None
        cost=int.from_bytes(body[pos+2:pos+6],'big');pos+=10
        fail=None
        for n in (2,4):
            v=decode_imm_d0(body[pos:pos+n])
            if v is not None and body[pos+n:pos+n+6]==FLAG_SET_CALL:
                fail=v;pos+=n+6;break
        if fail is None or body[pos:pos+2]!=b'\x60\x0a':return None
        pos+=2;fail2=None
        for n in (2,4):
            v=decode_imm_d0(body[pos:pos+n])
            if v is not None and body[pos+n:pos+n+6]==FLAG_CLEAR_CALL:
                fail2=v;pos+=n+6;break
        if fail2!=fail:return None
        insufficient=fail
    else:
        # Form B ($82): clear flag; subtract price in D0; BMI skips flag-set.
        # Bail is therefore set when affordable, unlike Form A's failure flag.
        flag_when_affordable=1
        fail=None
        for n in (2,4):
            v=decode_imm_d0(body[pos:pos+n])
            if v is not None and body[pos+n:pos+n+6]==FLAG_CLEAR_CALL:
                fail=v;pos+=n+6;break
        if fail is None or pos+12>len(body):return None
        if body[pos:pos+4]!=bytes.fromhex('20 38 f4 38') or body[pos+4:pos+6]!=b'\x04\x80' or body[pos+10:pos+12]!=b'\x6b\x08':return None
        cost=int.from_bytes(body[pos+6:pos+10],'big');pos+=12
        v=decode_imm_d0(body[pos:pos+2])
        if v!=fail or body[pos+2:pos+8]!=FLAG_SET_CALL:return None
        pos+=8;f=parse_face(pos)
        if not f:return None
        source_obj,pos=f;insufficient=fail

    if source_obj is None or cost is None or insufficient is None or insufficient>0xFF:return None
    if pos+8>len(body) or body[pos]!=0x70 or body[pos+2:pos+8]!=DIALOGUE_CALL:return None
    dialogue=body[pos+1];pos+=8
    # tst.b ($ED53).w ; bne.s final_rts
    if body[pos:pos+4]!=bytes.fromhex('4a 38 ed 53') or pos+6>len(body) or body[pos+4]!=0x66:return None
    br=pos+4;disp=int.from_bytes(body[br+1:br+2],'big',signed=True)
    if br+2+disp!=len(body)-2:return None
    pos+=6
    # Saved_Dialogue_Addr -> A0; require marker $3A at -2(A0); bne final_rts.
    if body[pos:pos+8]!=bytes.fromhex('70 ff 30 38 ec f0 20 40'):return None
    pos+=8
    if body[pos:pos+6]!=bytes.fromhex('0c 28 00 3a ff fe') or body[pos+6]!=0x66:return None
    br=pos+6;disp=int.from_bytes(body[br+1:br+2],'big',signed=True)
    if br+2+disp!=len(body)-2:return None
    pos+=8
    # subi.l #price,(Current_Money).w
    if body[pos:pos+2]!=b'\x04\xb8' or int.from_bytes(body[pos+2:pos+6],'big')!=cost or body[pos+6:pos+8]!=b'\xf4\x38':return None
    pos+=8
    patch_dialogue=0xFF
    if pos+10<=len(body) and body[pos:pos+2]==b'\x41\xf8':
        patch_obj=int.from_bytes(body[pos+2:pos+4],'big')
        if patch_obj!=source_obj or body[pos+4:pos+6]!=b'\x11\x7c' or body[pos+8:pos+10]!=b'\x00\x14':return None
        patch_dialogue=int.from_bytes(body[pos+6:pos+8],'big')&0xFF;pos+=10
    success=None
    for n in (2,4):
        v=decode_imm_d0(body[pos:pos+n])
        if v is not None and body[pos+n:pos+n+6]==FLAG_SET_CALL:
            success=v;pos+=n+6;break
    if success is None or success>0xFF or body[pos:pos+2]!=b'\x4e\x75' or pos+2!=len(body):return None
    slot=(source_obj-0xC300)//0x40
    if slot>31:return None
    return slot,dialogue,insufficient,success,cost,patch_dialogue,flag_when_affordable,source_obj


def classify_rng_dialogue_event(body: bytes):
    """Face one source object then choose dialogue from source RNG/flag state."""
    if len(body)!=82 or body[:2]!=b'\x49\xf8' or body[4:8]!=b'\x47\xf8\xc0\x00':return None
    if body[8:16]!=bytes.fromhex('30 2b 00 06 08 40 00 02') or body[16:22]!=FACE_OBJECT_CALL:return None
    obj=int.from_bytes(body[2:4],'big')
    if obj<0xC300 or (obj-0xC300)%0x40:return None
    slot=(obj-0xC300)//0x40
    if body[22:24]!=b'\x10\x3c' or body[26:32]!=EVENT_FLAG_TEST or body[32:34]!=b'\x66\x26':return None
    flag=int.from_bytes(body[24:26],'big')
    if body[34:40]!=bytes.fromhex('4e b9 00 04 23 6c'):return None
    if body[40:48]!=bytes.fromhex('30 38 ef 0c 02 40 00 03'):return None
    if body[48:50]!=b'\x67\x0a' or body[50:54]!=bytes.fromhex('0c 40 00 01') or body[54:56]!=b'\x67\x08':return None
    if body[56]!=0x70 or body[58:60]!=b'\x60\x06' or body[60]!=0x70 or body[62:64]!=b'\x60\x02' or body[64]!=0x70:return None
    d_other=body[57];d_zero=body[61];d_one=body[65]
    if body[66:72]!=b'\x4e\xf9'+DIALOGUE_CALL[2:]:return None
    if body[72:74]!=b'\x10\x3c' or body[76:82]!=b'\x4e\xf9'+DIALOGUE_CALL[2:]:return None
    flagged=int.from_bytes(body[74:76],'big')
    if max(flag,flagged)>0xFF:return None
    return slot,flag,d_zero,d_one,d_other,flagged,obj


def classify_paired_object_dialogue_state(body: bytes):
    """Two adjacent source objects with story-flag-selected dialogue state.

    Exact EventPtr $4F template: face two authored objects; if primary story
    flag is clear and secondary is clear, tail-jump to base dialogue.  Otherwise
    run the selected progressed dialogue, patch both objects to that dialogue ID
    and set one temporary flag.
    """
    if len(body)!=136:return None
    face_prefix=bytes.fromhex('47 f8 c0 00 30 2b 00 06 08 40 00 02')
    if body[:2]!=b'\x49\xf8' or body[4:16]!=face_prefix or body[16:22]!=FACE_OBJECT_CALL:return None
    obj0=int.from_bytes(body[2:4],'big')
    if body[22:24]!=b'\x49\xf8' or body[26:38]!=face_prefix or body[38:44]!=FACE_OBJECT_CALL:return None
    obj1=int.from_bytes(body[24:26],'big')
    for obj in (obj0,obj1):
        if obj<0xC300 or (obj-0xC300)%0x40:return None
    slot0=(obj0-0xC300)//0x40;slot1=(obj1-0xC300)//0x40
    if slot1!=slot0+1:return None
    if body[44:46]!=b'\x10\x3c' or body[48:54]!=EVENT_FLAG_TEST or body[54:56]!=b'\x66\x14':return None
    primary=int.from_bytes(body[46:48],'big')
    if body[56:58]!=b'\x10\x3c' or body[60:66]!=EVENT_FLAG_TEST or body[66:68]!=b'\x66\x22':return None
    secondary=int.from_bytes(body[58:60],'big')
    if body[68]!=0x70 or body[70:76]!=b'\x4e\xf9'+DIALOGUE_CALL[2:]:return None
    base_dialogue=body[69]
    if body[76]!=0x70 or body[78:84]!=DIALOGUE_CALL:return None
    primary_dialogue=body[77]
    if body[84:88]!=b'\x49\xf8'+obj0.to_bytes(2,'big'):return None
    if body[88:94]!=bytes.fromhex('19 7c 00 00 00 14')[:2]+primary_dialogue.to_bytes(2,'big')+bytes.fromhex('00 14'):
        return None
    if body[94:100]!=bytes.fromhex('19 7c 00 00 00 54')[:2]+primary_dialogue.to_bytes(2,'big')+bytes.fromhex('00 54'):
        return None
    if body[100:102]!=b'\x60\x18' or body[102]!=0x70 or body[104:110]!=DIALOGUE_CALL:return None
    secondary_dialogue=body[103]
    if body[110:114]!=b'\x49\xf8'+obj0.to_bytes(2,'big'):return None
    if body[114:120]!=bytes.fromhex('19 7c 00 00 00 14')[:2]+secondary_dialogue.to_bytes(2,'big')+bytes.fromhex('00 14'):
        return None
    if body[120:126]!=bytes.fromhex('19 7c 00 00 00 54')[:2]+secondary_dialogue.to_bytes(2,'big')+bytes.fromhex('00 54'):
        return None
    if body[126:128]!=b'\x10\x3c' or body[130:136]!=b'\x4e\xf9'+TEMP_FLAG_SET_CALL[2:]:return None
    temp_flag=int.from_bytes(body[128:130],'big')
    if max(primary,secondary,temp_flag)>0xFF:return None
    return slot0,slot1,primary,secondary,base_dialogue,primary_dialogue,secondary_dialogue,temp_flag,obj0,obj1


def classify_coord_object_conditional_dialogue(body: bytes, source_addr: int):
    """Exact coordinate table -> face authored object -> conditional dialogue.

    EventPtr $93 scans four {x,y,object_ptr} rows for Character_1, faces the
    selected source object toward the lead character, tests one EventFlag and
    tail-jumps to one of two ordinary dialogue entries.  The table is inline
    after the routine RTS and is part of the EventPtr source extent.
    """
    if len(body) < 84 or body[:4] != bytes.fromhex('41 f8 c0 00'):
        return None
    if body[4:6] != b'\x43\xf9' or body[10:12] != b'\x70\x03':
        return None
    table_ptr=int.from_bytes(body[6:10],'big')
    prefix=bytes.fromhex(
        '3e 11 be 68 00 30 66 0a 3e 29 00 02 be 68 00 34 67 08 50 49 51 c8 ff ea 4e 75 '
        '28 69 00 04 47 f8 c0 00 30 2b 00 06 08 40 00 02')
    if body[12:54] != prefix or body[54:60] != FACE_OBJECT_CALL:
        return None
    if body[60:62] != b'\x10\x3c' or body[64:70] != EVENT_FLAG_TEST:
        return None
    flag=int.from_bytes(body[62:64],'big')
    if flag > 0xFF or body[70:72] != b'\x66\x04' or body[72] != 0x70 or body[74:76] != b'\x60\x02' or body[76] != 0x70:
        return None
    clear_dialogue=body[73];set_dialogue=body[77]
    if body[78:84] != b'\x4e\xf9'+DIALOGUE_CALL[2:]:
        return None
    # The source LEA must point exactly to the inline 4x8-byte table.
    if table_ptr != source_addr + 84:return None
    table=body[84:]
    if len(table)!=32:return None
    rows=[]
    for q in range(4):
        x=int.from_bytes(table[q*8:q*8+2],'big');y=int.from_bytes(table[q*8+2:q*8+4],'big')
        obj=int.from_bytes(table[q*8+4:q*8+8],'big') & 0xFFFF
        if obj<0xC300 or (obj-0xC300)%0x40:return None
        slot=(obj-0xC300)//0x40
        if slot>31:return None
        rows.append((x,y,slot,obj))
    return flag,clear_dialogue,set_dialogue,table_ptr,rows


def classify_dialogue_successor_flag_event(body: bytes):
    """Dialogue branch whose completion flag depends on the post-$FF text cursor.

    PSIV RunText consumes $FF with (A0)+, then stores A0 to Saved_Dialogue_Addr
    ($ECF0).  The accepted routine tests that saved successor entry's first byte
    against one marker before committing the same EventFlag used by its initial
    first/repeat dialogue branch.
    """
    # move.b #flag,d0; EventFlags_Test; bne.s repeat; moveq #first,d0;
    # bra.s run; moveq #repeat,d0; Event_GetAndRunDialogue
    if len(body)<38 or body[:2]!=b'\x10\x3c':return None
    flag=int.from_bytes(body[2:4],'big')
    if flag>0xFF or body[4:10]!=EVENT_FLAG_TEST:return None
    if body[10:12]!=b'\x66\x04' or body[12]!=0x70 or body[14:16]!=b'\x60\x02' or body[16]!=0x70:return None
    first=body[13];repeat=body[17]
    if body[18:24]!=DIALOGUE_CALL:return None
    # moveq #-1,d0; move.w Saved_Dialogue_Addr,d0; movea.l d0,a0
    if body[24:32]!=bytes.fromhex('70 ff 30 38 ec f0 20 40'):return None
    # cmpi.b #marker,(a0); bne.s skip-set
    if body[32:34]!=b'\x0c\x10' or body[36:38]!=b'\x66\x0a':return None
    marker=int.from_bytes(body[34:36],'big')&0xFF
    pos=38
    if body[pos:pos+4]!=b'\x10\x3c'+flag.to_bytes(2,'big'):return None
    pos+=4
    if body[pos:pos+6]!=FLAG_SET_CALL:return None
    pos+=6
    if body[pos:pos+2]!=b'\x4e\x75' or pos+2!=len(body):return None
    return flag,first,repeat,marker


def classify_party_formation_battle_event(body: bytes):
    """Quick-dialogue battle that consumes one item and authors Character_1..5 positions.

    The accepted 68K body contains no opaque helper calls: $5ACDC quick dialogue,
    EventFlags_Set, the already-audited FindItem/compact pair, five Character
    records with identical current/destination X/Y plus facing, then the normal
    Map_Load_Flags/Event_Battle_Index/Routine_Exit_Flags battle tail.
    """
    pos=0
    if len(body)<80 or body[pos]!=0x70 or body[pos+2:pos+8]!=QUICK_DIALOGUE_CALL:return None
    dialogue=body[pos+1];pos+=8
    flag=None
    for n in (2,4):
        v=decode_imm_d0(body[pos:pos+n])
        if v is not None and body[pos+n:pos+n+6]==FLAG_SET_CALL:
            flag=v;pos+=n+6;break
    if flag is None:return None
    item=None
    for n in (2,4):
        v=decode_imm_d0(body[pos:pos+n])
        if v is not None and body[pos+n:pos+n+6]==FIND_ITEM_CALL:
            item=v;pos+=n+6;break
    if item is None:return None
    if body[pos:pos+2]!=b'\x42\x10' or body[pos+2:pos+8]!=COMPACT_INVENTORY_CALL:return None
    pos+=8
    # moveq #source_facing,d0; lea Character_1 ($C000),a4
    if body[pos]!=0x70 or body[pos+2:pos+6]!=bytes.fromhex('49 f8 c0 00'):return None
    source_facing=body[pos+1];pos+=6
    coords=[]
    for slot in range(5):
        vals=[]
        for disp in (0x30,0x34,0x38,0x3A):
            if body[pos:pos+2]!=b'\x39\x7c' or int.from_bytes(body[pos+4:pos+6],'big')!=disp:return None
            vals.append(int.from_bytes(body[pos+2:pos+4],'big'));pos+=6
        if vals[0]!=vals[2] or vals[1]!=vals[3]:return None
        if body[pos:pos+4]!=bytes.fromhex('39 40 00 06'):return None
        pos+=4;coords.append((vals[0],vals[1]))
        if slot<4:
            if body[pos:pos+4]!=bytes.fromhex('49 ec 00 40'):return None
            pos+=4
    if pos+20!=len(body):return None
    # bset #bit,$EC4E; move.b #battle,$ECFC; bset #3,$EC27; rts
    if body[pos:pos+2]!=b'\x08\xf8' or body[pos+4:pos+6]!=b'\xec\x4e':return None
    mapload_bit=int.from_bytes(body[pos+2:pos+4],'big');pos+=6
    if mapload_bit>7:return None
    if body[pos:pos+2]!=b'\x11\xfc' or body[pos+4:pos+6]!=b'\xec\xfc':return None
    battle=int.from_bytes(body[pos+2:pos+4],'big')&0xFF;pos+=6
    if body[pos:pos+6]!=bytes.fromhex('08 f8 00 03 ec 27'):return None
    pos+=6
    if body[pos:pos+2]!=b'\x4e\x75':return None
    return dialogue,flag,item,battle,mapload_bit,source_facing,coords




def classify_palette_alarm_dialogue_event(body: bytes):
    """Exact BioPlant-style red-alarm pulse -> dialogue -> TempFlag template.

    The source writes the frame delay at $ED52, calls $5B2BE to save all 64
    palette words and remove blue/green over eight steps, then calls $5B30E to
    restore the saved palette.  The trailing $51ADC is the standard source
    field-graphics reload after dialogue; native field graphics remain resident.
    """
    if len(body)!=64:return None
    if body[0:2]!=b'\x13\xfc' or body[4:8]!=bytes.fromhex('ff ff 50 0a'):return None
    sound=int.from_bytes(body[2:4],'big')&0xFF
    if body[8:10]!=b'\x11\xfc' or body[12:14]!=b'\xed\x52':return None
    delay_down=int.from_bytes(body[10:12],'big')&0xFF
    if body[14:20]!=PALETTE_ALARM_DOWN_CALL:return None
    if body[20:22]!=b'\x11\xfc' or body[24:26]!=b'\xed\x52':return None
    delay_restore=int.from_bytes(body[22:24],'big')&0xFF
    if body[26:32]!=PALETTE_ALARM_RESTORE_CALL:return None
    if body[32]!=0x70 or body[34:40]!=DIALOGUE_CALL:return None
    dialogue=body[33]
    if body[40:56]!=bytes.fromhex('31 fc 00 00 ec 20 20 78 ec 2c 4e b9 00 05 1a dc'):return None
    flag=decode_imm_d0(body[56:60])
    if flag is None:
        flag=decode_imm_d0(body[56:58]);n=2
    else:n=4
    if flag is None or body[56+n:56+n+6]!=bytes.fromhex('4e f9 00 05 76 7a') or 56+n+6!=len(body):return None
    return sound,delay_down,delay_restore,dialogue,flag


def classify_palette_black_battle_event(body: bytes):
    """Face NPC -> source first-32 palette blackout -> quick dialogue -> battle."""
    if len(body)!=64 or body[:2]!=b'\x49\xf8' or body[4:8]!=b'\x47\xf8\xc0\x00':return None
    if body[8:16]!=bytes.fromhex('30 2b 00 06 08 40 00 02') or body[16:22]!=FACE_OBJECT_CALL:return None
    source_obj=int.from_bytes(body[2:4],'big')
    if source_obj<0xC300 or (source_obj-0xC300)%0x40:return None
    slot=(source_obj-0xC300)//0x40
    if slot>31 or body[22:24]!=b'\x11\xfc' or body[26:28]!=b'\xed\x52':return None
    delay=int.from_bytes(body[24:26],'big')&0xFF
    if body[28:34]!=PALETTE_FIRST32_BLACK_CALL:return None
    if body[34]!=0x70 or body[36:42]!=QUICK_DIALOGUE_CALL:return None
    dialogue=body[35]
    flag=None;flag_end=None
    for n in (2,4):
        v=decode_imm_d0(body[42:42+n])
        if v is not None and body[42+n:42+n+6]==FLAG_SET_CALL:
            flag=v;flag_end=42+n+6;break
    if flag is None:return None
    pos=flag_end
    if body[pos:pos+2]!=b'\x11\xfc' or body[pos+4:pos+6]!=b'\xec\xfc':return None
    battle=int.from_bytes(body[pos+2:pos+4],'big')&0xFF;pos+=6
    if body[pos:pos+6]!=bytes.fromhex('08 f8 00 03 ec 27'):return None
    pos+=6
    if body[pos:pos+2]!=b'\x4e\x75' or pos+2!=len(body):return None
    return slot,delay,dialogue,flag,battle,source_obj


# Generated normal-event transition script opcodes.  These mirror only audited
# PSIV field-event helpers/instructions; unsupported helpers reject the routine.
EVTTR_DIALOGUE=1
EVTTR_FLAG_SET=2
EVTTR_SOUND=3
EVTTR_FADE_OUT=4
EVTTR_RAM16=5
EVTTR_MAPLOAD_CLEAR=6
EVTTR_REFRESH_MAP=7
EVTTR_FACE_OBJECT=8
EVTTR_FADE_IN=9
EVTTR_RAM8=10
EVTTR_TREE_LOAD=11
EVTTR_REMOVE_ALL_ITEM=12
EVTTR_TYPED_FLAG_SET=13
EVTTR_COPY32=14
EVTTR_COPY8=15
EVTTR_BRANCH_FLAG_CLEAR=16
EVTTR_RETURN=17
EVTTR_RAM32=18
EVTTR_PARTY_SYNC=19
EVTTR_MOVE_CAMERA=20
EVTTR_WAIT_UPDATES=21
EVTTR_BRANCH_FLAG_SET=22
EVTTR_ENSURE_SAVED_SOUND=23
EVTTR_VINT=24
EVTTR_FACE_OBJECT_OPPOSITE=25

def decode_item_immediate(body: bytes, pos: int):
    """Decode source move.b/move.w #ItemID,d0 used by inventory helpers."""
    if pos+4>len(body) or body[pos:pos+2] not in (b'\x10\x3c',b'\x30\x3c'):
        return None
    item=int.from_bytes(body[pos+2:pos+4],'big')
    return item if item<=0xFF else None


def decode_remove_all_item_loop(body: bytes, pos: int):
    """Recognize PSIV's exact FindItem -> clear slot -> compact -> repeat loop.

    $5B100 scans the packed 40-byte inventory at $F410 and returns zero with
    A0 on a matching slot.  The caller clears that byte; $5CB72 shifts the
    remaining packed bytes left.  BRA.S back to the FindItem call therefore
    removes every copy of the requested item.
    """
    item=decode_item_immediate(body,pos)
    if item is None:return None
    q=pos+4
    if body[q:q+6]!=FIND_ITEM_CALL:return None
    q+=6
    if q+12>len(body) or body[q]!=0x66:return None
    exit_target=q+2+int.from_bytes(body[q+1:q+2],'big',signed=True);q+=2
    if body[q:q+2]!=b'\x42\x10' or body[q+2:q+8]!=COMPACT_INVENTORY_CALL:return None
    q+=8
    if q+2>len(body) or body[q]!=0x60:return None
    loop_target=q+2+int.from_bytes(body[q+1:q+2],'big',signed=True);q+=2
    if loop_target!=pos or exit_target!=q:return None
    return item,q


def classify_layout_animation_event(body: bytes, source_addr: int):
    """Exact 2x2 layout-animation EventPtr template.

    The source obtains a layout pointer with GetMapLayoutChunk ($53514), then
    streams records of [duration, tl, tr, bl, br] from an inline ROM table.
    Every record redraws the plane and waits duration+1 VInts (DBF semantics),
    and the routine terminates by setting one story EventFlag.  The classifier
    accepts the complete routine including its inline frame table; no event id,
    coordinates, tiles, timing, sound or flag are invented by the port.
    """
    # move.b #sound,Sound_Index; lea Character_1,a0; move.w #x,d1;
    # move.w #y,d2; moveq #plane,d3; jsr GetMapLayoutChunk
    if len(body)<0x68:return None
    if body[0:2]!=b'\x13\xFC' or body[4:8]!=b'\xFF\xFF\x50\x0A':return None
    sound=body[3]
    if body[8:12]!=b'\x41\xF8\xC0\x00':return None
    if body[12:14]!=b'\x32\x3C' or body[16:18]!=b'\x34\x3C':return None
    x=int.from_bytes(body[14:16],'big');y=int.from_bytes(body[18:20],'big')
    if body[20]!=0x76:return None
    plane=body[21]
    if plane>1:return None
    if body[22:28]!=b'\x4E\xB9\x00\x05\x35\x14':return None
    if body[28:30]!=b'\x41\xF9':return None
    data_ptr=int.from_bytes(body[30:34],'big')
    # Exact source loop.  The short branches are part of the accepted shape so
    # a superficially similar routine with different control flow is rejected.
    loop=bytes.fromhex(
        '70 00 72 00 10 18 0c 00 00 ff 67 30 12 18 14 18 16 18 18 18 '
        '12 81 13 42 00 01 13 43 00 20 13 44 00 21 48 e7 80 e0 '
        '4e b9 00 05 40 fa 4c df 07 01 4e b9 00 04 16 a6 51 c8 ff f8 60 c4')
    if body[34:94]!=loop:return None
    if body[94]!=0x70:return None
    flag=body[95]
    if body[96:102]!=FLAG_SET_JMP:return None
    off=data_ptr-source_addr
    if off!=102 or off<0 or off>=len(body):return None
    q=off;frames=0
    while q<len(body):
        duration=body[q];q+=1
        if duration==0xFF:break
        if q+4>len(body):return None
        q+=4;frames+=1
        if frames>32:return None
    if not frames:return None
    # Source tables are word-aligned and terminate in FF,00.
    if q>=len(body) or body[q]!=0x00 or q+1!=len(body):return None
    return sound,x,y,plane,data_ptr,flag,frames



def classify_fixed_two_row_layout_event(body: bytes, source_addr: int):
    """Exact fixed two-row 2x2 layout animation used by EventPtr $42."""
    prefix=bytes.fromhex(
        '13 fc 00 e2 ff ff 50 0a 41 f8 c0 00 32 3c 00 0f '
        '34 3c 00 09 76 01 4e b9 00 05 35 14 45 d1 32 3c '
        '00 0f 34 3c 00 08 76 01 4e b9 00 05 35 14 41 f9')
    if len(body)<0x70 or body[:len(prefix)]!=prefix:return None
    q=len(prefix);data_ptr=int.from_bytes(body[q:q+4],'big');q+=4
    loop_start=q
    loop=bytes.fromhex(
        '70 00 10 18 0c 00 00 ff 67 2e 12 18 14 18 16 18 18 18 '
        '12 81 13 42 00 01 14 83 15 44 00 01 48 e7 80 e0 '
        '4e b9 00 05 40 fa 4c df 07 01 4e b9 00 04 16 a6 '
        '51 c8 ff f8')
    if body[q:q+len(loop)]!=loop:return None
    q+=len(loop)
    if body[q]!=0x60:return None
    target=q+2+int.from_bytes(body[q+1:q+2],'big',signed=True)
    if target!=loop_start:return None
    q+=2
    if body[q:q+2]!=b'\x4e\x75':return None
    q+=2
    if source_addr+q!=data_ptr:return None
    frames=0
    while q<len(body):
        duration=body[q];q+=1
        if duration==0xFF:break
        if q+4>len(body):return None
        q+=4;frames+=1
        if frames>32:return None
    if not frames or q!=len(body):return None
    return 0xE2,0x000F,0x0008,1,data_ptr,frames

def classify_dynamic_two_cell_layout_event(body: bytes, source_addr: int):
    """Exact Garuberk-style two-cell layout animation.

    The source resolves Character_1's current layout byte and the byte one
    32-pixel chunk above it through GetMapLayoutChunk using the current
    Map_Collision_Plane ($EC24). If the lower byte is the authored closed-door
    tile, it streams [duration, lower, upper] records from an inline table,
    redraws the selected plane, updates field objects and VInt-prepares for
    duration+1 frames. The optional ``moveq #0,d2`` seen in one source copy is
    semantically irrelevant because only D2.b is written; every other opcode
    and branch target is required exactly.
    """
    prefix=bytes.fromhex(
        '41 f8 c0 00 32 28 00 30 34 28 00 34 04 42 00 20 '
        'ea 49 ea 4a 76 00 16 38 ec 24 4e b9 00 05 35 14 '
        '45 d1 41 f8 c0 00 32 28 00 30 34 28 00 34 '
        'ea 49 ea 4a 76 00 16 38 ec 24 4e b9 00 05 35 14 '
        '0c 11 00 38')
    if len(body)<0x90 or body[:len(prefix)]!=prefix:return None
    q=len(prefix)
    # BNE.W -> routine RTS when the current/lower tile is not $38.
    if body[q:q+2]!=b'\x66\x00':return None
    rts_off=q+2+int.from_bytes(body[q+2:q+4],'big',signed=True);q+=4
    if rts_off+2>len(body) or body[rts_off:rts_off+2]!=b'\x4e\x75':return None
    if body[q:q+2]!=b'\x13\xfc' or body[q+4:q+8]!=b'\xff\xff\x50\x0a':return None
    sound=body[q+3];q+=8
    if body[q:q+2]!=b'\x41\xf9':return None
    data_ptr=int.from_bytes(body[q+2:q+6],'big');q+=6
    if data_ptr!=source_addr+rts_off+2:return None
    loop_start=q
    if body[q:q+4]!=b'\x70\x00\x72\x00':return None
    q+=4
    if body[q:q+2]==b'\x74\x00':q+=2
    if body[q:q+6]!=b'\x10\x18\x0c\x00\x00\xff':return None
    q+=6
    if body[q]!=0x67:return None
    end_target=q+2+int.from_bytes(body[q+1:q+2],'big',signed=True)
    if end_target!=rts_off:return None
    q+=2
    fixed=bytes.fromhex(
        '12 18 14 18 14 81 12 82 48 e7 80 e0 4a 38 ec 24 '
        '66 08 4e b9 00 05 40 98 60 06 4e b9 00 05 40 fa '
        '4e b9 00 05 49 38 4c df 07 01 4e b9 00 04 16 78 '
        '51 c8 ff d6')
    if body[q:q+len(fixed)]!=fixed:return None
    q+=len(fixed)
    if body[q]!=0x60:return None
    loop_target=q+2+int.from_bytes(body[q+1:q+2],'big',signed=True)
    if loop_target!=loop_start:return None
    q+=2
    if q!=rts_off or body[q:q+2]!=b'\x4e\x75':return None
    q+=2
    if source_addr+q!=data_ptr:return None
    frames=0
    while q<len(body):
        duration=body[q];q+=1
        if duration==0xFF:break
        if q+2>len(body):return None
        q+=2;frames+=1
        if frames>32:return None
    if not frames or q!=len(body):return None
    return sound,0x38,data_ptr,frames

def classify_garuberk_door_entered_event(body: bytes, source_addr: int):
    """Exact Garuberk Tower door-entered choreography used by EventPtrs $37/$38.

    Both source copies are 384 bytes and differ only in the authored upper open
    tile, the inline close-frame table pointer, and the close-frame tile values.
    Require the complete machine-code skeleton so unrelated long field events
    cannot be accepted merely because they contain a fade/reload/move sequence.
    """
    if len(body)!=384:return None
    normalized=bytearray(body[:368])
    normalized[91]=0;normalized[296]=0;normalized[297]=0
    expected=bytes.fromhex(
        '4e b9 00 04 22 3e 20 78 ec 34 4e b9 00 05 46 ea '
        '4e b9 00 05 ae 98 41 f8 c0 00 32 28 00 30 34 28 '
        '00 34 04 42 00 10 ea 49 ea 4a 76 00 16 38 ec 24 '
        '4e b9 00 05 35 14 45 d1 41 f8 c0 00 32 28 00 30 '
        '34 28 00 34 ea 49 ea 4a 76 00 16 38 ec 24 4e b9 '
        '00 05 35 14 12 bc 00 3c 14 bc 00 00 4e b9 00 05 '
        '40 98 4e b9 00 05 40 fa 4e b9 00 05 49 38 4e b9 '
        '00 04 16 78 4e b9 00 04 21 d4 13 fc 00 d7 ff ff '
        '50 0a 49 f8 c0 00 30 2c 00 34 06 40 00 10 39 40 '
        '00 3a 4e b9 00 04 47 d8 4e b9 00 05 49 38 4e b9 '
        '00 04 54 c0 4e b9 00 04 55 20 4e b9 00 04 55 80 '
        '4e b9 00 04 55 e0 4e b9 00 05 41 5c 4e b9 00 05 '
        '42 46 4e b9 00 04 16 a6 42 38 ef 02 49 f8 c0 00 '
        '4a 6c 00 2a 67 02 60 ba 4e b9 00 05 a8 7a 41 f8 '
        'c0 00 32 28 00 30 34 28 00 34 04 42 00 20 ea 49 '
        'ea 4a 76 00 16 38 ec 24 4e b9 00 05 35 14 45 d1 '
        '41 f8 c0 00 32 28 00 30 34 28 00 34 ea 49 ea 4a '
        '76 00 16 38 ec 24 4e b9 00 05 35 14 13 fc 00 d8 '
        'ff ff 50 0a 41 f9 00 06 00 00 70 00 72 00 74 00 '
        '10 18 0c 00 00 ff 67 36 12 18 14 18 14 81 12 82 '
        '48 e7 80 e0 4a 38 ec 24 66 08 4e b9 00 05 40 98 '
        '60 06 4e b9 00 05 40 fa 4e b9 00 05 49 38 4c df '
        '07 01 4e b9 00 04 16 78 51 c8 ff d6 60 bc 4e 75')
    if bytes(normalized)!=expected:return None
    open_upper=body[91]
    if open_upper not in (0x3E,0x34):return None
    data_ptr=int.from_bytes(body[294:298],'big')
    if data_ptr!=source_addr+368:return None
    q=368;frames=0
    while q<len(body):
        duration=body[q];q+=1
        if duration==0xFF:break
        if q+2>len(body):return None
        q+=2;frames+=1
        if frames>32:return None
    if frames!=5 or q!=len(body):return None
    if open_upper==0x3E:
        if data_ptr!=0x0006F714 or body[368:]!=bytes.fromhex('03 3e 3c 03 3d 3b 03 37 3a 03 36 39 03 35 38 ff'):return None
    else:
        if data_ptr!=0x0006F894 or body[368:]!=bytes.fromhex('03 34 3c 03 33 3b 03 32 3a 03 31 39 03 30 38 ff'):return None
    return open_upper,data_ptr,frames


def classify_consume_item_dialogue_event(body: bytes):
    """Face source NPC; remove one item; success dialogue+flag, else dialogue.

    This is the complete source template used by EventPtr $78.  It models the
    inventory helpers themselves rather than hard-coding the event's item ID.
    """
    if len(body)<48 or body[:2]!=b'\x49\xf8' or body[4:8]!=b'\x47\xf8\xc0\x00':return None
    if body[8:12]!=b'\x30\x2b\x00\x06' or body[12:16]!=b'\x08\x40\x00\x02' or body[16:22]!=FACE_OBJECT_CALL:return None
    source_obj=int.from_bytes(body[2:4],'big')
    if source_obj<0xC300 or (source_obj-0xC300)%0x40:return None
    slot=(source_obj-0xC300)//0x40
    if slot>31:return None
    pos=22;item=decode_item_immediate(body,pos)
    if item is None:return None
    pos+=4
    if body[pos:pos+6]!=FIND_ITEM_CALL:return None
    pos+=6
    if pos+2>len(body) or body[pos]!=0x66:return None
    fallback=pos+2+int.from_bytes(body[pos+1:pos+2],'big',signed=True);pos+=2
    if body[pos:pos+2]!=b'\x42\x10' or body[pos+2:pos+8]!=COMPACT_INVENTORY_CALL:return None
    pos+=8
    if pos+8>len(body) or body[pos]!=0x70 or body[pos+2:pos+8]!=DIALOGUE_CALL:return None
    success_dialogue=body[pos+1];pos+=8
    flag=None
    for n in (2,4):
        v=decode_imm_d0(body[pos:pos+n])
        if v is not None and body[pos+n:pos+n+6]==FLAG_SET_JMP:
            flag=v;pos+=n+6;break
    if flag is None or pos!=fallback:return None
    if pos+8!=len(body) or body[pos]!=0x70 or body[pos+2:pos+8]!=DIALOGUE_CALL.replace(b'\x4e\xb9',b'\x4e\xf9'):return None
    return slot,item,success_dialogue,flag,body[pos+1],source_obj



def classify_dialogue_face_resume(body: bytes):
    """Dialogue; face source object opposite Character_1; resume saved dialogue.

    Event_AiedoManSavings ($22) uses the ordinary dialogue resolver first,
    derives the opposite of Character_1's authored facing for one source field
    object, then reconstructs A0 from Saved_Dialogue_Addr and enters the common
    RunText body at $5AC6C.  The final $51ADC call reloads source field graphics
    from the current map descriptor; native GBA dialogue does not overwrite the
    resident map tiles, so the executor only needs a common field redraw there.
    """
    if len(body)!=62 or body[0]!=0x70 or body[2:8]!=DIALOGUE_CALL:return None
    dialogue=body[1]
    if body[8:10]!=b'\x49\xf8':return None
    source_obj=int.from_bytes(body[10:12],'big')
    if source_obj<0xC300 or (source_obj-0xC300)%0x40:return None
    slot=(source_obj-0xC300)//0x40
    if slot>31:return None
    # lea Character_1,a3; move.w facing_dir(a3),d0; bchg #2,d0; face object.
    if body[12:30]!=bytes.fromhex('47 f8 c0 00 30 2b 00 06 08 40 00 02 4e b9 00 05 a9 36'):return None
    # Reconstruct A0 from Saved_Dialogue_Addr and enter the shared RunText body.
    if body[30:44]!=bytes.fromhex('70 ff 30 38 ec f0 20 40 4e b9 00 05 ac 6c'):return None
    # Game_Mode_Routine=0; A0=(Map_Gfx source pointer); source field-GFX reload; RTS.
    if body[44:]!=bytes.fromhex('31 fc 00 00 ec 20 20 78 ec 2c 4e b9 00 05 1a dc 4e 75'):return None
    return slot,dialogue,source_obj

def classify_choreographed_followup_dialogue(body: bytes):
    """Exact repeated EventPtrs $6D/$6E/$6F choreography template.

    The source fixes one authored object facing, runs a timed SFX/dialogue
    sequence through loc_5A71E, then reconstructs A0 from Saved_Dialogue_Addr
    and calls the shared dialogue runner at $5AC6C.  Finally it writes one
    authored byte in the $F880 source-state block and sets one story flag.
    Every varying value is emitted as descriptor data; the timing/SFX skeleton
    must match byte-for-byte or the routine is rejected.
    """
    if len(body) not in (124,126):return None
    pos=0
    # move.b #$E1,Sound_Index
    if body[pos:pos+8]!=bytes.fromhex('13 fc 00 e1 ff ff 50 0a'):return None
    pos+=8
    # lea SourceFieldObj,a4 ; moveq #4,d0 ; Event_UpdateObjFacing
    if body[pos:pos+2]!=b'\x49\xf8':return None
    source_obj=int.from_bytes(body[pos+2:pos+4],'big');pos+=4
    if source_obj<0xC300 or (source_obj-0xC300)%0x40:return None
    slot=(source_obj-0xC300)//0x40
    if slot>31 or body[pos:pos+2]!=b'\x70\x04' or body[pos+2:pos+8]!=FACE_OBJECT_CALL:return None
    facing=4;pos+=8
    wait_call=bytes.fromhex('4e b9 00 05 a7 1e')
    def take_wait(expected):
        nonlocal pos
        if body[pos:pos+2]!=bytes((0x70,expected)) or body[pos+2:pos+8]!=wait_call:return False
        pos+=8;return True
    if not take_wait(0x27):return None
    # First authored dialogue.
    if pos+8>len(body) or body[pos]!=0x70 or body[pos+2:pos+8]!=DIALOGUE_CALL:return None
    dialogue=body[pos+1];pos+=8
    if not take_wait(0x27):return None
    # Three Grave/door-style source SFX pulses with authored waits.
    for wait in (0x27,0x13,0x3B):
        if body[pos:pos+8]!=bytes.fromhex('13 fc 00 cf ff ff 50 0a'):return None
        pos+=8
        if not take_wait(wait):return None
    # moveq #-1,d0; move.w Saved_Dialogue_Addr,d0; movea.l d0,a0;
    # jsr $5AC6C (shared RunText body on the already resolved cursor).
    if body[pos:pos+14]!=bytes.fromhex('70 ff 30 38 ec f0 20 40 4e b9 00 05 ac 6c'):return None
    pos+=14
    # lea $F880,a0 ; move.b #value,disp(a0)
    if body[pos:pos+4]!=bytes.fromhex('41 f8 f8 80') or body[pos+4:pos+6]!=b'\x11\x7c':return None
    state_value=int.from_bytes(body[pos+6:pos+8],'big')&0xFF
    disp=int.from_bytes(body[pos+8:pos+10],'big')
    state_addr=0xF880+disp
    if not (0xF880<=state_addr<=0xF8FF):return None
    pos+=10
    flag=None
    for n in (2,4):
        v=decode_imm_d0(body[pos:pos+n])
        if v is not None and body[pos+n:pos+n+6]==FLAG_SET_JMP:
            flag=v;pos+=n+6;break
    if flag is None or pos!=len(body):return None
    return slot,facing,dialogue,state_addr,state_value,flag,source_obj


def classify_transition_event_script(body: bytes):
    """Accept complete EventPtrs map-transition scripts using an audited opcode set.

    This is intentionally strict static lowering, not a 68K interpreter.  A
    routine must contain RefreshMap and every byte before/after it must match one
    of the source operations below.  This currently admits $83 and $9E while
    rejecting $97/$98 because their item/inventory helpers are outside the
    proven vocabulary.
    """
    ops=[];pos=0;saw_refresh=False;current_object_slot=None
    allowed_ram16={0xEC28,0xEC2A,0xEC48,0xEC4A,0xEC44,0xEC46}
    # These copies are the source's saved-party restoration primitive used by
    # EventPtr $6A.  Keep the opcode generic in the generated VM, but only
    # accept the audited source/destination pairs until another exact routine
    # proves a broader transition-time RAM-copy shape.
    allowed_copy32={(0xED54,0xF40A),(0xF40A,0xED54)}
    allowed_copy8={(0xED58,0xF40E),(0xF40E,0xED58)}
    while pos<len(body):
        if body[pos:pos+2]==b'\x4e\x75':
            pos+=2
            if pos!=len(body):return None
            break
        matched=False
        for n in (2,4):
            if pos+n+6<=len(body):
                imm=decode_imm_d0(body[pos:pos+n])
                call=body[pos+n:pos+n+6]
                if imm is not None and call in (DIALOGUE_CALL,DIALOGUE_CALL.replace(b'\x4e\xb9',b'\x4e\xf9'),FLAG_SET_CALL,FLAG_SET_JMP):
                    if call in (DIALOGUE_CALL,DIALOGUE_CALL.replace(b'\x4e\xb9',b'\x4e\xf9')):
                        ops.append((EVTTR_DIALOGUE,imm,0,0))
                    else:
                        ops.append((EVTTR_FLAG_SET,imm,0,0))
                    pos+=n+6;matched=True;break
        if matched:continue
        # Direct absolute-short RAM copies.  Genesis source uses these before
        # RefreshMap to restore Saved_Char_ID_Mem_1..5 into Current_Party_Slots.
        if pos+6<=len(body) and body[pos:pos+2] in (b'\x21\xf8',b'\x11\xf8'):
            src=int.from_bytes(body[pos+2:pos+4],'big');dst=int.from_bytes(body[pos+4:pos+6],'big')
            if body[pos:pos+2]==b'\x21\xf8':
                if (src,dst) not in allowed_copy32:return None
                ops.append((EVTTR_COPY32,0,dst,src))
            else:
                if (src,dst) not in allowed_copy8:return None
                ops.append((EVTTR_COPY8,0,dst,src))
            pos+=6;continue
        # Exact transition-time long immediate store used when an event temporarily
        # rewrites the five Current_Party_Slot bytes as a packed long+byte pair.
        # Keep this generic in the VM but restrict generation to the audited party
        # slot destination until another source routine proves a broader shape.
        if pos+8<=len(body) and body[pos:pos+2]==b'\x21\xfc':
            value=int.from_bytes(body[pos+2:pos+6],'big');addr=int.from_bytes(body[pos+6:pos+8],'big')
            if addr!=0xF40A:return None
            ops.append((EVTTR_RAM32,0,addr,value));pos+=8;continue
        # Event field-party rebuild ($662DA).  This consumes Current_Party_Slot_1..5
        # and refreshes Character_1..5 metadata.  Native runtime mirrors the same
        # party roster into lead/follower state through one shared primitive.
        if body[pos:pos+6]==bytes.fromhex('4e b9 00 06 62 da'):
            ops.append((EVTTR_PARTY_SYNC,0,0,0));pos+=6;continue
        # Event_MoveCamera($5AAEE): move.w #x,d0; move.w #y,d1; moveq #step,d2.
        # Pack x in addr and y in value; arg8 carries the authored per-frame step.
        if pos+16<=len(body) and body[pos:pos+2]==b'\x30\x3c' and body[pos+4:pos+6]==b'\x32\x3c' and body[pos+8]==0x74 and body[pos+10:pos+16]==bytes.fromhex('4e b9 00 05 aa ee'):
            x=int.from_bytes(body[pos+2:pos+4],'big');y=int.from_bytes(body[pos+6:pos+8],'big');step=body[pos+9]
            if not step:return None
            ops.append((EVTTR_MOVE_CAMERA,step,x,y));pos+=16;continue
        # loc_5A71E clears the field-update latch then performs D0+1 iterations of
        # Field_UpdateObjects($54938) + VInt_Prepare($4204C).  Store D0 itself so
        # runtime preserves DBF count semantics exactly.
        if pos+8<=len(body) and body[pos]==0x70 and body[pos+2:pos+8]==bytes.fromhex('4e b9 00 05 a7 1e'):
            ops.append((EVTTR_WAIT_UPDATES,body[pos+1],0,0));pos+=8;continue
        # Source field-object facing derived from Character_1: load the lead's
        # facing_dir, toggle bit 2 (opposite direction), and apply it to the
        # authored source object.  This is shared by ordinary talk events and
        # transition events such as $79; keep it as a VM primitive rather than
        # baking the event id into runtime code.
        if pos+22<=len(body) and body[pos:pos+2]==b'\x49\xf8' and body[pos+4:pos+8]==b'\x47\xf8\xc0\x00' and body[pos+8:pos+16]==bytes.fromhex('30 2b 00 06 08 40 00 02') and body[pos+16:pos+22]==FACE_OBJECT_CALL:
            source_obj=int.from_bytes(body[pos+2:pos+4],'big')
            if source_obj<0xC300 or (source_obj-0xC300)%0x40:return None
            slot=(source_obj-0xC300)//0x40
            if slot>31:return None
            ops.append((EVTTR_FACE_OBJECT_OPPOSITE,slot,0,0));current_object_slot=slot;pos+=22;continue
        # Once A4 has been loaded with an authored source object, source events
        # often issue several Event_UpdateObjFacing calls separated only by
        # update waits.  Preserve that register-lifetime pattern as repeated
        # FACE_OBJECT ops against the last statically proven A4 object slot.
        if current_object_slot is not None and pos+8<=len(body) and body[pos]==0x70 and body[pos+2:pos+8]==FACE_OBJECT_CALL:
            ops.append((EVTTR_FACE_OBJECT,current_object_slot,0,body[pos+1]));pos+=8;continue
        # Common source guard: test one story flag; when it is clear, run a
        # tail-jumped fallback dialogue and return.  When set, branch over that
        # two-op terminal path and continue the event body.  The short-branch
        # target and tail JMP must match exactly.
        guard_matched=False
        for n in (2,4):
            test_flag=decode_imm_d0(body[pos:pos+n])
            if test_flag is None or pos+n+8>len(body) or body[pos+n:pos+n+6]!=EVENT_FLAG_TEST:continue
            branch_pos=pos+n+6
            if body[branch_pos]!=0x66:continue  # BNE.s: source flag is set.
            target=branch_pos+2+int.from_bytes(body[branch_pos+1:branch_pos+2],'big',signed=True)
            fall=branch_pos+2
            if fall+8!=target or target>len(body):continue
            if body[fall]!=0x70 or body[fall+2:fall+8]!=DIALOGUE_CALL.replace(b'\x4e\xb9',b'\x4e\xf9'):continue
            ops.append((EVTTR_BRANCH_FLAG_SET,test_flag,0,2))
            ops.append((EVTTR_DIALOGUE,body[fall+1],0,0))
            ops.append((EVTTR_RETURN,0,0,0))
            pos=target;guard_matched=True;break
        if guard_matched:continue
        # Source music/sound restore idiom: if Saved_Sound_Index already equals
        # the authored id, do nothing; otherwise write the id to Sound_Index
        # and Saved_Sound_Index.  The source has an extra BEQ after loading D0
        # so id zero intentionally performs no write.
        if pos+24<=len(body) and body[pos:pos+2]==bytes.fromhex('0c 38') and body[pos+4:pos+8]==bytes.fromhex('ec ec 67 10') and body[pos+8:pos+10]==bytes.fromhex('10 3c') and body[pos+12:pos+14]==bytes.fromhex('67 0a') and body[pos+14:pos+20]==bytes.fromhex('13 c0 ff ff 50 0a') and body[pos+20:pos+24]==bytes.fromhex('11 c0 ec ec'):
            sound0=int.from_bytes(body[pos+2:pos+4],'big')&0xFF
            sound1=int.from_bytes(body[pos+10:pos+12],'big')&0xFF
            if sound0!=sound1:return None
            ops.append((EVTTR_ENSURE_SAVED_SOUND,sound0,0,0));pos+=24;continue
        # A direct VInt_Prepare call is one source frame boundary without the
        # Field_UpdateObjects call used by loc_5A71E.
        if body[pos:pos+6]==bytes.fromhex('4e b9 00 04 20 4c'):
            ops.append((EVTTR_VINT,0,0,0));pos+=6;continue
        # Source conditional terminal shape used by EventPtr $6A:
        # EventFlags_Test(test); BEQ.s else_sound; EventFlags_Set(set) via JMP;
        # else_sound: move.b #sound,Sound_Index.  Lower it to generic branch,
        # flag-set, return, and sound ops so the transition VM keeps real
        # control flow instead of baking the event identity into runtime code.
        cond_matched=False
        for n in (2,4):
            test_flag=decode_imm_d0(body[pos:pos+n])
            if test_flag is None or pos+n+8>len(body) or body[pos+n:pos+n+6]!=EVENT_FLAG_TEST:continue
            branch_pos=pos+n+6
            if body[branch_pos]!=0x67:continue  # BEQ.s: EventFlags_Test returned clear.
            disp=int.from_bytes(body[branch_pos+1:branch_pos+2],'big',signed=True)
            target=branch_pos+2+disp
            set_pos=branch_pos+2
            set_flag=None;set_end=None
            for sn in (2,4):
                v=decode_imm_d0(body[set_pos:set_pos+sn])
                if v is not None and body[set_pos+sn:set_pos+sn+6]==FLAG_SET_JMP:
                    set_flag=v;set_end=set_pos+sn+6;break
            if set_flag is None or set_end!=target or target+8>len(body):continue
            if body[target:target+2]!=b'\x13\xfc' or body[target+4:target+8]!=bytes.fromhex('ff ff 50 0a'):continue
            sound=int.from_bytes(body[target+2:target+4],'big')&0xFF
            ops.append((EVTTR_BRANCH_FLAG_CLEAR,test_flag,0,2))
            ops.append((EVTTR_FLAG_SET,set_flag,0,0))
            ops.append((EVTTR_RETURN,0,0,0))
            ops.append((EVTTR_SOUND,sound,0,0))
            pos=target+8;cond_matched=True;break
        if cond_matched:continue
        if pos+8<=len(body) and body[pos:pos+2]==b'\x13\xfc' and body[pos+4:pos+8]==bytes.fromhex('ff ff 50 0a'):
            ops.append((EVTTR_SOUND,int.from_bytes(body[pos+2:pos+4],'big')&0xFF,0,0));pos+=8;continue
        if pos+6<=len(body) and body[pos:pos+2]==b'\x11\xfc':
            addr=int.from_bytes(body[pos+4:pos+6],'big')
            if addr not in (0xECEC,0xF40E):return None
            ops.append((EVTTR_RAM8,int.from_bytes(body[pos+2:pos+4],'big')&0xFF,addr,0));pos+=6;continue
        rem=decode_remove_all_item_loop(body,pos)
        if rem is not None:
            item,newpos=rem;ops.append((EVTTR_REMOVE_ALL_ITEM,item,0,0));pos=newpos;continue
        for call,bank in ((TEMP_FLAG_SET_CALL,2),):
            matched_flag=False
            for n in (2,4):
                imm=decode_imm_d0(body[pos:pos+n])
                if imm is not None and body[pos+n:pos+n+6]==call:
                    ops.append((EVTTR_TYPED_FLAG_SET,imm,bank,0));pos+=n+6;matched_flag=True;break
            if matched_flag:break
        if matched_flag:continue
        if body[pos:pos+6] in (bytes.fromhex('4e b9 00 04 22 3e'),bytes.fromhex('4e f9 00 04 22 3e')):
            ops.append((EVTTR_FADE_OUT,0,0,0));pos+=6;continue
        if body[pos:pos+6] in (bytes.fromhex('4e b9 00 04 21 d4'),bytes.fromhex('4e f9 00 04 21 d4')):
            ops.append((EVTTR_FADE_IN,0,0,0));pos+=6;continue
        if pos+6<=len(body) and body[pos:pos+2]==b'\x31\xfc':
            value=int.from_bytes(body[pos+2:pos+4],'big');addr=int.from_bytes(body[pos+4:pos+6],'big')
            if addr not in allowed_ram16:return None
            ops.append((EVTTR_RAM16,0,addr,value));pos+=6;continue
        if pos+6<=len(body) and body[pos:pos+2]==b'\x08\xb8' and body[pos+4:pos+6]==b'\xec\x4e':
            bit=int.from_bytes(body[pos+2:pos+4],'big')
            if bit>7:return None
            ops.append((EVTTR_MAPLOAD_CLEAR,bit,0xEC4E,0));pos+=6;continue
        if body[pos:pos+6]==bytes.fromhex('4e b9 00 05 ae 98'):
            ops.append((EVTTR_REFRESH_MAP,0,0,0));pos+=6;saw_refresh=True;continue
        if pos+12<=len(body) and body[pos:pos+2]==b'\x49\xf8' and body[pos+4]==0x70 and body[pos+6:pos+12]==FACE_OBJECT_CALL:
            source_obj=int.from_bytes(body[pos+2:pos+4],'big')
            if source_obj<0xC300 or (source_obj-0xC300)%0x40:return None
            slot=(source_obj-0xC300)//0x40
            if slot>31:return None
            ops.append((EVTTR_FACE_OBJECT,slot,0,body[pos+5]));current_object_slot=slot;pos+=12;continue
        if pos+12<=len(body) and body[pos:pos+2]==b'\x20\x3c' and body[pos+6:pos+12]==DIALOGUE_TREE_LOAD:
            tree=int.from_bytes(body[pos+2:pos+6],'big')
            if tree>=0x300000:return None
            ops.append((EVTTR_TREE_LOAD,0,0,tree));pos+=12;continue
        return None
    if pos!=len(body) or not saw_refresh:return None
    return ops


def classify_flag_coord_eq_condition(body: bytes, source_addr: int):
    """RunEvent predicates made only of flag tests and exact lead X/Y tests.

    This accepts no ranges and no helper calls: after zero-to-three verified flag
    bank tests the source must LEA Character_1, compare one or both curr_x_pos
    ($30) / curr_y_pos ($34) for exact equality, branch to RunEvent_NoEvent on
    mismatch, then write Event_Index and return d7=1.
    """
    if len(body)<24 or body[-10:-8]!=b'\x31\xfc' or body[-6:-4]!=b'\xec\xa8' or body[-4:]!=b'\x7e\x01\x4e\x75':
        return None
    event_index=int.from_bytes(body[-8:-6],'big');q=body[:-10];pos=0;tests=[]
    calls=((EVENT_FLAG_TEST,0),(CHEST_FLAG_TEST,1),(TEMP_FLAG_TEST,2))
    # Optional leading flag gates.
    while pos<len(q) and q[pos:pos+4]!=b'\x49\xf8\xc0\x00':
        flag=None;nimm=0
        for n in (2,4):
            v=decode_imm_d0(q[pos:pos+n])
            if v is not None:flag=v;nimm=n;break
        if flag is None or flag>0xFF:return None
        pos+=nimm;bank=None
        for call,b in calls:
            if q[pos:pos+6]==call:bank=b;break
        if bank is None:return None
        pos+=6
        if q[pos:pos+2] not in (b'\x66\x00',b'\x67\x00') or pos+4>len(q):return None
        op=q[pos];disp=int.from_bytes(q[pos+2:pos+4],'big',signed=True)
        if source_addr+pos+2+disp!=RUN_EVENT_NOEVENT:return None
        tests.append((bank,flag,1 if op==0x67 else 0));pos+=4
        if len(tests)>3:return None
    if q[pos:pos+4]!=b'\x49\xf8\xc0\x00':return None
    pos+=4;x=0xFFFF;y=0xFFFF;seen=0
    while pos<len(q):
        if pos+10>len(q) or q[pos:pos+2]!=b'\x0c\x6c':return None
        value=int.from_bytes(q[pos+2:pos+4],'big');off=int.from_bytes(q[pos+4:pos+6],'big')
        if off==0x30:
            if x!=0xFFFF:return None
            x=value
        elif off==0x34:
            if y!=0xFFFF:return None
            y=value
        else:return None
        # Only BNE.W -> NoEvent is accepted: successful predicate means equality.
        if q[pos+6:pos+8]!=b'\x66\x00':return None
        disp=int.from_bytes(q[pos+8:pos+10],'big',signed=True)
        if source_addr+pos+8+disp!=RUN_EVENT_NOEVENT:return None
        pos+=10;seen+=1
    if not seen:return None
    return event_index,tests,x,y


def classify_flag_coord_range_condition(body: bytes, source_addr: int):
    """Flag gates plus unsigned exact/range comparisons against lead X/Y.

    Accepted Bcc.W branches all target RunEvent_NoEvent. CMP.W is unsigned for
    BHI/BLS/BCC/BCS, matching the source's positive field coordinates.
    Predicate op codes: 0 EQ, 1 LE, 2 GT, 3 LT, 4 GE.
    """
    if len(body)<24 or body[-10:-8]!=b'\x31\xfc' or body[-6:-4]!=b'\xec\xa8' or body[-4:]!=b'\x7e\x01\x4e\x75':return None
    event_index=int.from_bytes(body[-8:-6],'big');q=body[:-10];pos=0;tests=[]
    calls=((EVENT_FLAG_TEST,0),(CHEST_FLAG_TEST,1),(TEMP_FLAG_TEST,2))
    while pos<len(q) and q[pos:pos+4]!=b'\x49\xf8\xc0\x00':
        flag=None;nimm=0
        for n in (2,4):
            v=decode_imm_d0(q[pos:pos+n])
            if v is not None:flag=v;nimm=n;break
        if flag is None or flag>0xFF:return None
        pos+=nimm;bank=None
        for call,b in calls:
            if q[pos:pos+6]==call:bank=b;break
        if bank is None:return None
        pos+=6
        if q[pos:pos+2] not in (b'\x66\x00',b'\x67\x00') or pos+4>len(q):return None
        op=q[pos];disp=int.from_bytes(q[pos+2:pos+4],'big',signed=True)
        if source_addr+pos+2+disp!=RUN_EVENT_NOEVENT:return None
        tests.append((bank,flag,1 if op==0x67 else 0));pos+=4
        if len(tests)>3:return None
    if q[pos:pos+4]!=b'\x49\xf8\xc0\x00':return None
    pos+=4;comps=[];opmap={0x66:0,0x62:1,0x63:2,0x64:3,0x65:4}
    while pos<len(q):
        if pos+10>len(q) or q[pos:pos+2]!=b'\x0c\x6c':return None
        value=int.from_bytes(q[pos+2:pos+4],'big');off=int.from_bytes(q[pos+4:pos+6],'big')
        bop=q[pos+6]
        if off not in (0x30,0x34) or q[pos+7]!=0 or bop not in opmap:return None
        disp=int.from_bytes(q[pos+8:pos+10],'big',signed=True)
        if source_addr+pos+8+disp!=RUN_EVENT_NOEVENT:return None
        comps.append((0 if off==0x30 else 1,opmap[bop],value));pos+=10
        if len(comps)>4:return None
    if not comps or all(op==0 for _,op,_ in comps):return None
    return event_index,tests,comps

def run_event_target(data: bytes, idx: int) -> int:
    a=RUN_EVENTS+idx*4
    if data[a:a+2] != b'\x60\x00':
        raise SystemExit(f'unexpected RunEventsJmpTbl opcode at {idx:#x}')
    disp=int.from_bytes(data[a+2:a+4], 'big', signed=True)
    return a+2+disp

def classify_flag_condition(body: bytes):
    # move.w #flag,d0; jsr EventFlags_Test; beq/bne NoEvent;
    # move.w #event,(Event_Index).w; moveq #1,d7; rts
    if len(body)!=24 or body[:2]!=b'\x30\x3c' or body[4:10]!=EVENT_FLAG_TEST:
        return None
    if body[10:12] not in (b'\x66\x00', b'\x67\x00'):
        return None
    if body[14:16]!=b'\x31\xfc' or body[18:20]!=b'\xec\xa8' or body[20:]!=b'\x7e\x01\x4e\x75':
        return None
    flag=int.from_bytes(body[2:4], 'big')
    event_index=int.from_bytes(body[16:18], 'big')
    if flag>0xFF:
        return None
    # BNE NoEvent means trigger while clear; BEQ NoEvent means require set.
    require_set=1 if body[10:12]==b'\x67\x00' else 0
    return flag,event_index,require_set


def classify_flag_chain_condition(body: bytes, source_addr: int):
    """Pure RunEvents flag-gate chains (1-3 story/chest/temp tests).

    Each source test loads a flag into D0, calls one of the three verified flag
    bank Test entrypoints, and branches to RunEvent_NoEvent ($57620) when the
    required state is not met.  A successful chain writes Event_Index and
    returns d7=1.  No coordinates, object state or helper side effects are
    accepted by this classifier.
    """
    tail_len=10
    if len(body)<tail_len or body[-10:-8]!=b'\x31\xfc' or body[-6:-4]!=b'\xec\xa8' or body[-4:]!=b'\x7e\x01\x4e\x75':
        return None
    event_index=int.from_bytes(body[-8:-6],'big')
    q=body[:-tail_len];pos=0;tests=[]
    calls=((EVENT_FLAG_TEST,0),(CHEST_FLAG_TEST,1),(TEMP_FLAG_TEST,2))
    while pos<len(q):
        flag=None;nimm=0
        for n in (2,4):
            v=decode_imm_d0(q[pos:pos+n])
            if v is not None:flag=v;nimm=n;break
        if flag is None or flag>0xFF:return None
        pos+=nimm;bank=None
        for call,b in calls:
            if q[pos:pos+6]==call:bank=b;break
        if bank is None:return None
        pos+=6
        if q[pos:pos+2] not in (b'\x66\x00',b'\x67\x00') or pos+4>len(q):return None
        op=q[pos];disp=int.from_bytes(q[pos+2:pos+4],'big',signed=True)
        # 68000 Bcc.w displacement is relative to the extension-word address.
        if source_addr+pos+2+disp != RUN_EVENT_NOEVENT:return None
        require_set=1 if op==0x67 else 0
        tests.append((bank,flag,require_set));pos+=4
        if len(tests)>3:return None
    if not tests:return None
    return event_index,tests

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--rom',required=True,type=Path)
    ap.add_argument('--output',type=Path,default=Path('gba/generated_simple_events_v0982.h'))
    a=ap.parse_args()
    data=a.rom.read_bytes(); sha=hashlib.sha256(data).hexdigest()
    if len(data)!=0x300000 or sha!=EXPECTED:
        raise SystemExit(f'bad PSIV source {len(data):#x} {sha}')
    ptrs=[read_ptr(data,i) for i in range(EVENT_COUNT)]
    # The byte immediately after the final pointer is code, proving the table's
    # source boundary; this also prevents accidentally treating code as pointers.
    if int.from_bytes(data[EVENT_PTRS+EVENT_COUNT*4:EVENT_PTRS+EVENT_COUNT*4+4],'big') != 0x48E7FFF8:
        raise SystemExit('unexpected EventPtrs end marker')

    # EventPtrs are laid out monotonically in this pinned ROM, so every entry but
    # the last is bounded by the next source pointer.  $A0 has no following table
    # entry; do *not* use the EventPtrs table address as its end (that is earlier
    # than the routine).  The final source routine is an exact dialogue+flag tail
    # ending in EventFlags_Set, so derive its boundary from that audited terminal
    # jump.  This keeps the generator table-driven instead of hard-coding $A0's
    # dialogue or flag values.
    last_p=ptrs[-1]
    last_term=data.find(FLAG_SET_JMP,last_p,min(len(data),last_p+0x80))
    if last_term<0:
        raise SystemExit('could not locate terminal EventFlags_Set for EventPtrs[$A0]')
    last_end=last_term+len(FLAG_SET_JMP)
    def event_end(i: int) -> int:
        return ptrs[i+1] if i+1<EVENT_COUNT else last_end

    noop_rows=[]
    rows=[]
    for i,p in enumerate(ptrs):
        end=event_end(i)
        if not (0 <= p < len(data)):
            raise SystemExit(f'bad EventPtrs[{i:#x}]={p:#x}')
        if i+1<EVENT_COUNT and end < p:
            raise SystemExit(f'nonmonotonic EventPtrs at {i:#x}')
        body=data[p:end]
        if classify_noop(body):
            noop_rows.append((i,p,end-p))
        c=classify(body)
        if c:
            rows.append((i,c[0],c[1],p,end-p))
    face_rows=[]
    for i,p in enumerate(ptrs):
        end=event_end(i)
        c=classify_face_dialogue_flag(data[p:end])
        if c:
            face_rows.append((i,c[0],c[1],c[2],c[3],p,end-p))

    battle_rows=[]
    temp_clear_rows=[]
    two_flag_rows=[]
    recovery_rows=[]
    conditional_dialogue_rows=[]
    dialogue_camera_rows=[]
    tree_dialogue_rows=[]
    object_dialogue_patch_rows=[]
    quick_battle_rows=[]
    guarded_quick_rows=[]
    extended_battle_rows=[]
    party_formation_battle_rows=[]
    inventory_reward_rows=[]
    money_transaction_rows=[]
    coord_object_dialogue_rows=[]
    paired_object_dialogue_rows=[]
    rng_dialogue_rows=[]
    dialogue_successor_flag_rows=[]
    choreographed_followup_rows=[]
    dialogue_face_resume_rows=[]
    transition_script_rows=[]
    transition_script_ops=[]
    consume_item_rows=[]
    layout_animation_rows=[]
    dynamic_two_cell_layout_rows=[]
    fixed_two_row_layout_rows=[]
    garuberk_door_entered_rows=[]
    palette_alarm_rows=[]
    palette_black_battle_rows=[]
    for i,p in enumerate(ptrs):
        end=event_end(i)
        body=data[p:end]
        c=classify_simple_battle_event(body)
        if c:
            battle_rows.append((i,c[0],c[1],c[2],c[3],c[4],p,end-p))
        c=classify_temp_flag_clear(body)
        if c is not None:temp_clear_rows.append((i,c,p,end-p))
        c=classify_dialogue_two_flags(body)
        if c:two_flag_rows.append((i,c[0],c[1],c[2],p,end-p))
        c=classify_dialogue_recovery(body)
        if c is not None:recovery_rows.append((i,c,p,end-p))
        c=classify_dialogue_camera_flag(body)
        if c:dialogue_camera_rows.append((i,c[0],c[1],p,end-p))
        c=classify_conditional_dialogue_flag(body)
        if c:conditional_dialogue_rows.append((i,c[0],c[1],c[2],p,end-p))
        c=classify_tree_dialogue_flag(body)
        if c:tree_dialogue_rows.append((i,c[0],c[1],c[2],p,end-p))
        c=classify_object_dialogue_patch(body)
        if c:object_dialogue_patch_rows.append((i,*c,p,end-p))
        c=classify_quick_dialogue_battle_event(body)
        if c:quick_battle_rows.append((i,*c,p,end-p))
        c=classify_guarded_quick_battle_or_dialogue(body)
        if c:guarded_quick_rows.append((i,*c,p,end-p))
        c=classify_extended_battle_event(body)
        if c:extended_battle_rows.append((i,*c,p,end-p))
        c=classify_party_formation_battle_event(body)
        if c:party_formation_battle_rows.append((i,*c,p,end-p))
        c=classify_inventory_reward_event(body)
        if c:inventory_reward_rows.append((i,*c,p,end-p))
        c=classify_money_dialogue_transaction(body)
        if c:money_transaction_rows.append((i,*c,p,end-p))
        c=classify_coord_object_conditional_dialogue(body,p)
        if c:coord_object_dialogue_rows.append((i,*c,p,end-p))
        c=classify_paired_object_dialogue_state(body)
        if c:paired_object_dialogue_rows.append((i,*c,p,end-p))
        c=classify_rng_dialogue_event(body)
        if c:rng_dialogue_rows.append((i,*c,p,end-p))
        c=classify_dialogue_successor_flag_event(body)
        if c:dialogue_successor_flag_rows.append((i,*c,p,end-p))
        c=classify_choreographed_followup_dialogue(body)
        if c:choreographed_followup_rows.append((i,*c,p,end-p))
        c=classify_dialogue_face_resume(body)
        if c:dialogue_face_resume_rows.append((i,*c,p,end-p))
        c=classify_consume_item_dialogue_event(body)
        if c:consume_item_rows.append((i,*c,p,end-p))
        c=classify_layout_animation_event(body,p)
        if c:layout_animation_rows.append((i,*c,p,end-p))
        c=classify_dynamic_two_cell_layout_event(body,p)
        if c:dynamic_two_cell_layout_rows.append((i,*c,p,end-p))
        c=classify_fixed_two_row_layout_event(body,p)
        if c:fixed_two_row_layout_rows.append((i,*c,p,end-p))
        c=classify_garuberk_door_entered_event(body,p)
        if c:garuberk_door_entered_rows.append((i,*c,p,end-p))
        c=classify_palette_alarm_dialogue_event(body)
        if c:palette_alarm_rows.append((i,*c,p,end-p))
        c=classify_palette_black_battle_event(body)
        if c:palette_black_battle_rows.append((i,*c,p,end-p))
        c=classify_transition_event_script(body)
        if c:
            first=len(transition_script_ops)
            transition_script_ops.extend(c)
            transition_script_rows.append((i,first,len(c),p,end-p))

    run_targets=[run_event_target(data,i) for i in range(RUN_EVENT_COUNT)]
    uniq=sorted(set(run_targets));next_target={t:(uniq[n+1] if n+1<len(uniq) else t+512) for n,t in enumerate(uniq)}
    conditions=[]
    flag_chain_conditions=[]
    flag_coord_eq_conditions=[]
    flag_coord_range_conditions=[]
    for i,t in enumerate(run_targets):
        body=data[t:next_target[t]]
        c=classify_flag_condition(body)
        if c:
            conditions.append((i,c[0],c[1],c[2],t))
        fc=classify_flag_chain_condition(body,t)
        if fc:
            event_index,tests=fc
            # Keep the original compact table for its exact one-story-flag
            # subset; emit every mixed-bank or multi-test predicate here.
            if len(tests)>1 or tests[0][0]!=0 or c is None:
                flag_chain_conditions.append((i,event_index,tests,t))
        cc=classify_flag_coord_eq_condition(body,t)
        if cc:
            flag_coord_eq_conditions.append((i,cc[0],cc[1],cc[2],cc[3],t))
        rc=classify_flag_coord_range_condition(body,t)
        if rc:
            flag_coord_range_conditions.append((i,rc[0],rc[1],rc[2],t))

    out=[]
    out.append('/* Auto-generated from verified PSIV US ROM by generate_simple_event_descriptors.py. */')
    out.append('#ifndef PS4_GENERATED_SIMPLE_EVENTS_V0982_H')
    out.append('#define PS4_GENERATED_SIMPLE_EVENTS_V0982_H')
    out.append('struct Ps4NoopEventDescriptor { uint8_t event_index; };')
    out.append(f'static const struct Ps4NoopEventDescriptor ps4_noop_event_descriptors[{len(noop_rows)}]={{')
    for idx,p,size in noop_rows:
        out.append(f'    {{0x{idx:02X}u}}, /* ${p:06X}, {size} bytes */')
    out.append('};')
    out.append(f'#define PS4_NOOP_EVENT_DESCRIPTOR_COUNT {len(noop_rows)}u')
    out.append('struct Ps4SimpleEventDescriptor { uint8_t event_index; uint8_t dialogue; uint8_t flag; };')
    out.append(f'static const struct Ps4SimpleEventDescriptor ps4_simple_event_descriptors[{len(rows)}]={{')
    for idx,dialogue,flag,p,size in rows:
        out.append(f'    {{0x{idx:02X}u,0x{dialogue:02X}u,0x{flag:02X}u}}, /* ${p:06X}, {size} bytes */')
    out.append('};')
    out.append(f'#define PS4_SIMPLE_EVENT_DESCRIPTOR_COUNT {len(rows)}u')
    out.append('struct Ps4FacingDialogueFlagDescriptor { uint8_t event_index; uint8_t object_slot; uint8_t dialogue; uint8_t flag; };')
    out.append(f'static const struct Ps4FacingDialogueFlagDescriptor ps4_facing_dialogue_flag_descriptors[{len(face_rows)}]={{')
    for idx,slot,dialogue,flag,source_obj,p,size in face_rows:
        out.append(f'    {{0x{idx:02X}u,{slot}u,0x{dialogue:02X}u,0x{flag:02X}u}}, /* source obj ${source_obj:04X}, event ${p:06X} */')
    out.append('};')
    out.append(f'#define PS4_FACING_DIALOGUE_FLAG_DESCRIPTOR_COUNT {len(face_rows)}u')
    out.append('struct Ps4SimpleBattleEventDescriptor { uint8_t event_index; uint8_t dialogue; uint8_t flag; uint8_t battle_index; uint8_t set_mapload_bit; uint8_t return_one; };')
    out.append(f'static const struct Ps4SimpleBattleEventDescriptor ps4_simple_battle_event_descriptors[{len(battle_rows)}]={{')
    for idx,dialogue,flag,battle,mapload,return_one,p,size in battle_rows:
        out.append(f'    {{0x{idx:02X}u,0x{dialogue:02X}u,0x{flag:02X}u,0x{battle:02X}u,{mapload}u,{return_one}u}}, /* ${p:06X}, {size} bytes */')
    out.append('};')
    out.append(f'#define PS4_SIMPLE_BATTLE_EVENT_DESCRIPTOR_COUNT {len(battle_rows)}u')
    out.append('struct Ps4TempFlagClearDescriptor { uint8_t event_index; uint8_t flag; };')
    out.append(f'static const struct Ps4TempFlagClearDescriptor ps4_temp_flag_clear_descriptors[{len(temp_clear_rows)}]={{')
    for idx,flag,p,size in temp_clear_rows:
        out.append(f'    {{0x{idx:02X}u,0x{flag:02X}u}}, /* ${p:06X}, {size} bytes */')
    out.append('};')
    out.append(f'#define PS4_TEMP_FLAG_CLEAR_DESCRIPTOR_COUNT {len(temp_clear_rows)}u')
    out.append('struct Ps4DialogueTwoFlagsDescriptor { uint8_t event_index; uint8_t dialogue; uint8_t first_flag; uint8_t second_flag; };')
    out.append(f'static const struct Ps4DialogueTwoFlagsDescriptor ps4_dialogue_two_flags_descriptors[{len(two_flag_rows)}]={{')
    for idx,dialogue,f1,f2,p,size in two_flag_rows:
        out.append(f'    {{0x{idx:02X}u,0x{dialogue:02X}u,0x{f1:02X}u,0x{f2:02X}u}}, /* ${p:06X}, {size} bytes */')
    out.append('};')
    out.append(f'#define PS4_DIALOGUE_TWO_FLAGS_DESCRIPTOR_COUNT {len(two_flag_rows)}u')
    out.append('struct Ps4DialogueRecoveryDescriptor { uint8_t event_index; uint8_t dialogue; };')
    out.append(f'static const struct Ps4DialogueRecoveryDescriptor ps4_dialogue_recovery_descriptors[{len(recovery_rows)}]={{')
    for idx,dialogue,p,size in recovery_rows:
        out.append(f'    {{0x{idx:02X}u,0x{dialogue:02X}u}}, /* ${p:06X}, {size} bytes */')
    out.append('};')
    out.append(f'#define PS4_DIALOGUE_RECOVERY_DESCRIPTOR_COUNT {len(recovery_rows)}u')
    out.append('struct Ps4DialogueCameraFlagDescriptor { uint8_t event_index; uint8_t dialogue; uint8_t flag; };')
    out.append(f'static const struct Ps4DialogueCameraFlagDescriptor ps4_dialogue_camera_flag_descriptors[{len(dialogue_camera_rows)}]={{')
    for idx,dialogue,flag,p,size in dialogue_camera_rows:
        out.append(f'    {{0x{idx:02X}u,0x{dialogue:02X}u,0x{flag:02X}u}}, /* ${p:06X}, {size} bytes */')
    out.append('};')
    out.append(f'#define PS4_DIALOGUE_CAMERA_FLAG_DESCRIPTOR_COUNT {len(dialogue_camera_rows)}u')
    out.append('struct Ps4ConditionalDialogueFlagDescriptor { uint8_t event_index; uint8_t flag; uint8_t first_dialogue; uint8_t repeat_dialogue; };')
    out.append(f'static const struct Ps4ConditionalDialogueFlagDescriptor ps4_conditional_dialogue_flag_descriptors[{len(conditional_dialogue_rows)}]={{')
    for idx,flag,first,repeat,p,size in conditional_dialogue_rows:
        out.append(f'    {{0x{idx:02X}u,0x{flag:02X}u,0x{first:02X}u,0x{repeat:02X}u}}, /* ${p:06X}, {size} bytes */')
    out.append('};')
    out.append(f'#define PS4_CONDITIONAL_DIALOGUE_FLAG_DESCRIPTOR_COUNT {len(conditional_dialogue_rows)}u')
    out.append('struct Ps4TreeDialogueFlagDescriptor { uint8_t event_index; uint32_t dialogue_tree; uint8_t dialogue; uint8_t flag; };')
    out.append(f'static const struct Ps4TreeDialogueFlagDescriptor ps4_tree_dialogue_flag_descriptors[{len(tree_dialogue_rows)}]={{')
    for idx,tree,dialogue,flag,p,size in tree_dialogue_rows:
        out.append(f'    {{0x{idx:02X}u,0x{tree:08X}u,0x{dialogue:02X}u,0x{flag:02X}u}}, /* ${p:06X}, {size} bytes */')
    out.append('};')
    out.append(f'#define PS4_TREE_DIALOGUE_FLAG_DESCRIPTOR_COUNT {len(tree_dialogue_rows)}u')
    out.append('struct Ps4ObjectDialoguePatchDescriptor { uint8_t event_index; uint32_t pre_tree; uint8_t dialogue; uint8_t start_slot; uint8_t count; uint8_t object_dialogue; uint32_t post_tree; uint8_t flag; };')
    out.append(f'static const struct Ps4ObjectDialoguePatchDescriptor ps4_object_dialogue_patch_descriptors[{len(object_dialogue_patch_rows)}]={{')
    for idx,pre_tree,dialogue,start_slot,count,obj_dialogue,post_tree,flag,p,size in object_dialogue_patch_rows:
        out.append(f'    {{0x{idx:02X}u,0x{pre_tree:08X}u,0x{dialogue:02X}u,{start_slot}u,{count}u,0x{obj_dialogue:02X}u,0x{post_tree:08X}u,0x{flag:02X}u}}, /* ${p:06X}, {size} bytes */')
    out.append('};')
    out.append(f'#define PS4_OBJECT_DIALOGUE_PATCH_DESCRIPTOR_COUNT {len(object_dialogue_patch_rows)}u')
    out.append('struct Ps4QuickDialogueBattleEventDescriptor { uint8_t event_index; uint8_t dialogue; uint8_t flag; uint8_t battle_index; uint8_t mapload_bit; uint8_t return_one; uint8_t saved_sound; };')
    out.append(f'static const struct Ps4QuickDialogueBattleEventDescriptor ps4_quick_dialogue_battle_event_descriptors[{len(quick_battle_rows)}]={{')
    for idx,dialogue,flag,battle,mapbit,return_one,saved_sound,p,size in quick_battle_rows:
        out.append(f'    {{0x{idx:02X}u,0x{dialogue:02X}u,0x{flag:02X}u,0x{battle:02X}u,0x{mapbit:02X}u,{return_one}u,0x{saved_sound:02X}u}}, /* ${p:06X}, {size} bytes */')
    out.append('};')
    out.append(f'#define PS4_QUICK_DIALOGUE_BATTLE_EVENT_DESCRIPTOR_COUNT {len(quick_battle_rows)}u')
    out.append('struct Ps4GuardedQuickBattleDescriptor { uint8_t event_index; uint8_t guard_flag; uint8_t fallback_dialogue; uint8_t quick_dialogue; uint8_t flag; uint8_t battle_index; uint8_t mapload_bit; };')
    out.append(f'static const struct Ps4GuardedQuickBattleDescriptor ps4_guarded_quick_battle_descriptors[{len(guarded_quick_rows)}]={{')
    for idx,guard,fallback,quick,flag,battle,mapbit,p,size in guarded_quick_rows:
        out.append(f'    {{0x{idx:02X}u,0x{guard:02X}u,0x{fallback:02X}u,0x{quick:02X}u,0x{flag:02X}u,0x{battle:02X}u,0x{mapbit:02X}u}}, /* ${p:06X}, {size} bytes */')
    out.append('};')
    out.append(f'#define PS4_GUARDED_QUICK_BATTLE_DESCRIPTOR_COUNT {len(guarded_quick_rows)}u')
    out.append('struct Ps4ExtendedBattleEventDescriptor { uint8_t event_index; uint8_t dialogue; uint8_t flag; uint8_t battle_index; uint8_t set_mapload_bit; uint8_t return_one; uint8_t guard_flag; uint8_t saved_sound; };')
    out.append(f'static const struct Ps4ExtendedBattleEventDescriptor ps4_extended_battle_event_descriptors[{len(extended_battle_rows)}]={{')
    for idx,dialogue,flag,battle,mapload,return_one,guard,saved_sound,p,size in extended_battle_rows:
        out.append(f'    {{0x{idx:02X}u,0x{dialogue:02X}u,0x{flag:02X}u,0x{battle:02X}u,{mapload}u,{return_one}u,0x{guard:02X}u,0x{saved_sound:02X}u}}, /* ${p:06X}, {size} bytes */')
    out.append('};')
    out.append(f'#define PS4_EXTENDED_BATTLE_EVENT_DESCRIPTOR_COUNT {len(extended_battle_rows)}u')
    out.append('struct Ps4PartyFormationBattleDescriptor { uint8_t event_index; uint8_t dialogue; uint8_t flag; uint8_t item; uint8_t battle_index; uint8_t mapload_bit; uint8_t source_facing; uint16_t x0,y0,x1,y1,x2,y2,x3,y3,x4,y4; };')
    out.append(f'static const struct Ps4PartyFormationBattleDescriptor ps4_party_formation_battle_descriptors[{len(party_formation_battle_rows)}]={{')
    for idx,dialogue,flag,item,battle,mapbit,facing,coords,p,size in party_formation_battle_rows:
        xy=','.join(f'0x{x:04X}u,0x{y:04X}u' for x,y in coords)
        out.append(f'    {{0x{idx:02X}u,0x{dialogue:02X}u,0x{flag:02X}u,0x{item:02X}u,0x{battle:02X}u,{mapbit}u,0x{facing:02X}u,{xy}}}, /* ${p:06X}, {size} bytes */')
    out.append('};')
    out.append(f'#define PS4_PARTY_FORMATION_BATTLE_DESCRIPTOR_COUNT {len(party_formation_battle_rows)}u')
    out.append('struct Ps4InventoryRewardDescriptor { uint8_t event_index; uint8_t item; uint8_t success_dialogue; uint8_t full_dialogue; uint8_t flag; };')
    out.append(f'static const struct Ps4InventoryRewardDescriptor ps4_inventory_reward_descriptors[{len(inventory_reward_rows)}]={{')
    for idx,item,success_dialogue,full_dialogue,flag,p,size in inventory_reward_rows:
        out.append(f'    {{0x{idx:02X}u,0x{item:02X}u,0x{success_dialogue:02X}u,0x{full_dialogue:02X}u,0x{flag:02X}u}}, /* ${p:06X}, {size} bytes */')
    out.append('};')
    out.append(f'#define PS4_INVENTORY_REWARD_DESCRIPTOR_COUNT {len(inventory_reward_rows)}u')
    out.append('struct Ps4MoneyDialogueTransactionDescriptor { uint8_t event_index; uint8_t object_slot; uint8_t dialogue; uint8_t money_flag; uint8_t success_flag; uint8_t patch_dialogue; uint8_t flag_when_affordable; uint32_t cost; };')
    out.append(f'static const struct Ps4MoneyDialogueTransactionDescriptor ps4_money_dialogue_transaction_descriptors[{len(money_transaction_rows)}]={{')
    for idx,slot,dialogue,insufficient,success,cost,patch_dialogue,flag_when_affordable,source_obj,p,size in money_transaction_rows:
        out.append(f'    {{0x{idx:02X}u,{slot}u,0x{dialogue:02X}u,0x{insufficient:02X}u,0x{success:02X}u,0x{patch_dialogue:02X}u,{flag_when_affordable}u,0x{cost:08X}u}}, /* source obj ${source_obj:04X}, event ${p:06X}, {size} bytes */')
    out.append('};')
    out.append(f'#define PS4_MONEY_DIALOGUE_TRANSACTION_DESCRIPTOR_COUNT {len(money_transaction_rows)}u')
    out.append('struct Ps4CoordObjectConditionalDialogueDescriptor { uint8_t event_index; uint8_t flag; uint8_t clear_dialogue; uint8_t set_dialogue; uint16_t x0,y0; uint8_t slot0; uint16_t x1,y1; uint8_t slot1; uint16_t x2,y2; uint8_t slot2; uint16_t x3,y3; uint8_t slot3; };')
    out.append(f'static const struct Ps4CoordObjectConditionalDialogueDescriptor ps4_coord_object_conditional_dialogue_descriptors[{len(coord_object_dialogue_rows)}]={{')
    for idx,flag,clear_dialogue,set_dialogue,table_ptr,entries,p,size in coord_object_dialogue_rows:
        vals=[]
        for x,y,slot,obj in entries: vals.extend((f'0x{x:04X}u',f'0x{y:04X}u',f'{slot}u'))
        out.append(f'    {{0x{idx:02X}u,0x{flag:02X}u,0x{clear_dialogue:02X}u,0x{set_dialogue:02X}u,'+','.join(vals)+f'}}, /* table ${table_ptr:06X}, event ${p:06X}, {size} bytes */')
    out.append('};')
    out.append(f'#define PS4_COORD_OBJECT_CONDITIONAL_DIALOGUE_DESCRIPTOR_COUNT {len(coord_object_dialogue_rows)}u')
    out.append('struct Ps4PairedObjectDialogueStateDescriptor { uint8_t event_index; uint8_t slot0,slot1; uint8_t primary_flag,secondary_flag; uint8_t base_dialogue,primary_dialogue,secondary_dialogue; uint8_t temp_flag; };')
    out.append(f'static const struct Ps4PairedObjectDialogueStateDescriptor ps4_paired_object_dialogue_state_descriptors[{len(paired_object_dialogue_rows)}]={{')
    for idx,slot0,slot1,primary,secondary,base_dialogue,primary_dialogue,secondary_dialogue,temp_flag,obj0,obj1,p,size in paired_object_dialogue_rows:
        out.append(f'    {{0x{idx:02X}u,{slot0}u,{slot1}u,0x{primary:02X}u,0x{secondary:02X}u,0x{base_dialogue:02X}u,0x{primary_dialogue:02X}u,0x{secondary_dialogue:02X}u,0x{temp_flag:02X}u}}, /* objs ${obj0:04X}/${obj1:04X}, event ${p:06X}, {size} bytes */')
    out.append('};')
    out.append(f'#define PS4_PAIRED_OBJECT_DIALOGUE_STATE_DESCRIPTOR_COUNT {len(paired_object_dialogue_rows)}u')
    out.append('struct Ps4RngDialogueDescriptor { uint8_t event_index; uint8_t object_slot; uint8_t flag; uint8_t zero_dialogue,one_dialogue,other_dialogue,flagged_dialogue; };')
    out.append(f'static const struct Ps4RngDialogueDescriptor ps4_rng_dialogue_descriptors[{len(rng_dialogue_rows)}]={{')
    for idx,slot,flag,dzero,done,dother,flagged,obj,p,size in rng_dialogue_rows:
        out.append(f'    {{0x{idx:02X}u,{slot}u,0x{flag:02X}u,0x{dzero:02X}u,0x{done:02X}u,0x{dother:02X}u,0x{flagged:02X}u}}, /* obj ${obj:04X}, event ${p:06X}, {size} bytes */')
    out.append('};')
    out.append(f'#define PS4_RNG_DIALOGUE_DESCRIPTOR_COUNT {len(rng_dialogue_rows)}u')
    out.append('struct Ps4DialogueSuccessorFlagDescriptor { uint8_t event_index; uint8_t flag; uint8_t first_dialogue; uint8_t repeat_dialogue; uint8_t successor_marker; };')
    out.append(f'static const struct Ps4DialogueSuccessorFlagDescriptor ps4_dialogue_successor_flag_descriptors[{len(dialogue_successor_flag_rows)}]={{')
    for idx,flag,first,repeat,marker,p,size in dialogue_successor_flag_rows:
        out.append(f'    {{0x{idx:02X}u,0x{flag:02X}u,0x{first:02X}u,0x{repeat:02X}u,0x{marker:02X}u}}, /* ${p:06X}, {size} bytes */')
    out.append('};')
    out.append(f'#define PS4_DIALOGUE_SUCCESSOR_FLAG_DESCRIPTOR_COUNT {len(dialogue_successor_flag_rows)}u')
    out.append('struct Ps4DialogueFaceResumeDescriptor { uint8_t event_index; uint8_t object_slot; uint8_t dialogue; };')
    out.append(f'static const struct Ps4DialogueFaceResumeDescriptor ps4_dialogue_face_resume_descriptors[{len(dialogue_face_resume_rows)}]={{')
    for idx,slot,dialogue,source_obj,p,size in dialogue_face_resume_rows:
        out.append(f'    {{0x{idx:02X}u,{slot}u,0x{dialogue:02X}u}}, /* source obj ${source_obj:04X}, event ${p:06X}, {size} bytes */')
    out.append('};')
    out.append(f'#define PS4_DIALOGUE_FACE_RESUME_DESCRIPTOR_COUNT {len(dialogue_face_resume_rows)}u')
    out.append('struct Ps4ChoreographedFollowupDialogueDescriptor { uint8_t event_index; uint8_t object_slot; uint8_t source_facing; uint8_t dialogue; uint16_t state_addr; uint8_t state_value; uint8_t flag; };')
    out.append(f'static const struct Ps4ChoreographedFollowupDialogueDescriptor ps4_choreographed_followup_dialogue_descriptors[{len(choreographed_followup_rows)}]={{')
    for idx,slot,facing,dialogue,state_addr,state_value,flag,source_obj,p,size in choreographed_followup_rows:
        out.append(f'    {{0x{idx:02X}u,{slot}u,0x{facing:02X}u,0x{dialogue:02X}u,0x{state_addr:04X}u,0x{state_value:02X}u,0x{flag:02X}u}}, /* source obj ${source_obj:04X}, event ${p:06X}, {size} bytes */')
    out.append('};')
    out.append(f'#define PS4_CHOREOGRAPHED_FOLLOWUP_DIALOGUE_DESCRIPTOR_COUNT {len(choreographed_followup_rows)}u')
    out.append('struct Ps4ConsumeItemDialogueDescriptor { uint8_t event_index; uint8_t object_slot; uint8_t item; uint8_t success_dialogue; uint8_t flag; uint8_t fallback_dialogue; };')
    out.append(f'static const struct Ps4ConsumeItemDialogueDescriptor ps4_consume_item_dialogue_descriptors[{len(consume_item_rows)}]={{')
    for idx,slot,item,success_dialogue,flag,fallback_dialogue,source_obj,p,size in consume_item_rows:
        out.append(f'    {{0x{idx:02X}u,{slot}u,0x{item:02X}u,0x{success_dialogue:02X}u,0x{flag:02X}u,0x{fallback_dialogue:02X}u}}, /* source obj ${source_obj:04X}, event ${p:06X}, {size} bytes */')
    out.append('};')
    out.append(f'#define PS4_CONSUME_ITEM_DIALOGUE_DESCRIPTOR_COUNT {len(consume_item_rows)}u')
    out.append('struct Ps4LayoutAnimationDescriptor { uint8_t event_index; uint8_t sound; uint16_t x_chunk,y_chunk; uint8_t plane; uint32_t frames_addr; uint8_t flag; uint8_t frame_count; };')
    out.append(f'static const struct Ps4LayoutAnimationDescriptor ps4_layout_animation_descriptors[{len(layout_animation_rows)}]={{')
    for idx,sound,x,y,plane,data_ptr,flag,frames,p,size in layout_animation_rows:
        out.append(f'    {{0x{idx:02X}u,0x{sound:02X}u,0x{x:04X}u,0x{y:04X}u,{plane}u,0x{data_ptr:08X}u,0x{flag:02X}u,{frames}u}}, /* ${p:06X}, {size} bytes */')
    out.append('};')
    out.append(f'#define PS4_LAYOUT_ANIMATION_DESCRIPTOR_COUNT {len(layout_animation_rows)}u')
    out.append('struct Ps4DynamicTwoCellLayoutDescriptor { uint8_t event_index,sound,expected_lower; uint32_t frames_addr; uint8_t frame_count; };')
    out.append(f'static const struct Ps4DynamicTwoCellLayoutDescriptor ps4_dynamic_two_cell_layout_descriptors[{len(dynamic_two_cell_layout_rows)}]={{')
    for idx,sound,expected_lower,data_ptr,frames,p,size in dynamic_two_cell_layout_rows:
        out.append(f'    {{0x{idx:02X}u,0x{sound:02X}u,0x{expected_lower:02X}u,0x{data_ptr:08X}u,{frames}u}}, /* ${p:06X}, {size} bytes */')
    out.append('};')
    out.append(f'#define PS4_DYNAMIC_TWO_CELL_LAYOUT_DESCRIPTOR_COUNT {len(dynamic_two_cell_layout_rows)}u')
    out.append('struct Ps4FixedTwoRowLayoutDescriptor { uint8_t event_index,sound; uint16_t x_chunk,y_chunk; uint8_t plane; uint32_t frames_addr; uint8_t frame_count; };')
    out.append(f'static const struct Ps4FixedTwoRowLayoutDescriptor ps4_fixed_two_row_layout_descriptors[{len(fixed_two_row_layout_rows)}]={{')
    for idx,sound,x,y,plane,data_ptr,frames,p,size in fixed_two_row_layout_rows:
        out.append(f'    {{0x{idx:02X}u,0x{sound:02X}u,0x{x:04X}u,0x{y:04X}u,{plane}u,0x{data_ptr:08X}u,{frames}u}}, /* ${p:06X}, {size} bytes */')
    out.append('};')
    out.append(f'#define PS4_FIXED_TWO_ROW_LAYOUT_DESCRIPTOR_COUNT {len(fixed_two_row_layout_rows)}u')
    out.append('struct Ps4GaruberkDoorEnteredDescriptor { uint8_t event_index; uint8_t open_upper; uint32_t close_frames_addr; uint8_t frame_count; };')
    out.append(f'static const struct Ps4GaruberkDoorEnteredDescriptor ps4_garuberk_door_entered_descriptors[{len(garuberk_door_entered_rows)}]={{')
    for idx,open_upper,data_ptr,frames,p,size in garuberk_door_entered_rows:
        out.append(f'    {{0x{idx:02X}u,0x{open_upper:02X}u,0x{data_ptr:08X}u,{frames}u}}, /* ${p:06X}, {size} bytes */')
    out.append('};')
    out.append(f'#define PS4_GARUBERK_DOOR_ENTERED_DESCRIPTOR_COUNT {len(garuberk_door_entered_rows)}u')
    out.append('struct Ps4PaletteAlarmDialogueDescriptor { uint8_t event_index,sound,delay_down,delay_restore,dialogue,temp_flag; };')
    out.append(f'static const struct Ps4PaletteAlarmDialogueDescriptor ps4_palette_alarm_dialogue_descriptors[{len(palette_alarm_rows)}]={{')
    for idx,sound,delay_down,delay_restore,dialogue,flag,p,size in palette_alarm_rows:
        out.append(f'    {{0x{idx:02X}u,0x{sound:02X}u,{delay_down}u,{delay_restore}u,0x{dialogue:02X}u,0x{flag:02X}u}}, /* ${p:06X}, {size} bytes */')
    out.append('};')
    out.append(f'#define PS4_PALETTE_ALARM_DIALOGUE_DESCRIPTOR_COUNT {len(palette_alarm_rows)}u')
    out.append('struct Ps4PaletteBlackBattleDescriptor { uint8_t event_index,object_slot,delay,dialogue,flag,battle_index; };')
    out.append(f'static const struct Ps4PaletteBlackBattleDescriptor ps4_palette_black_battle_descriptors[{len(palette_black_battle_rows)}]={{')
    for idx,slot,delay,dialogue,flag,battle,source_obj,p,size in palette_black_battle_rows:
        out.append(f'    {{0x{idx:02X}u,{slot}u,{delay}u,0x{dialogue:02X}u,0x{flag:02X}u,0x{battle:02X}u}}, /* source obj ${source_obj:04X}, event ${p:06X}, {size} bytes */')
    out.append('};')
    out.append(f'#define PS4_PALETTE_BLACK_BATTLE_DESCRIPTOR_COUNT {len(palette_black_battle_rows)}u')
    out.append('#define PS4_EVTTR_DIALOGUE 1u')
    out.append('#define PS4_EVTTR_FLAG_SET 2u')
    out.append('#define PS4_EVTTR_SOUND 3u')
    out.append('#define PS4_EVTTR_FADE_OUT 4u')
    out.append('#define PS4_EVTTR_RAM16 5u')
    out.append('#define PS4_EVTTR_MAPLOAD_CLEAR 6u')
    out.append('#define PS4_EVTTR_REFRESH_MAP 7u')
    out.append('#define PS4_EVTTR_FACE_OBJECT 8u')
    out.append('#define PS4_EVTTR_FADE_IN 9u')
    out.append('#define PS4_EVTTR_RAM8 10u')
    out.append('#define PS4_EVTTR_TREE_LOAD 11u')
    out.append('#define PS4_EVTTR_REMOVE_ALL_ITEM 12u')
    out.append('#define PS4_EVTTR_TYPED_FLAG_SET 13u')
    out.append('#define PS4_EVTTR_COPY32 14u')
    out.append('#define PS4_EVTTR_COPY8 15u')
    out.append('#define PS4_EVTTR_BRANCH_FLAG_CLEAR 16u')
    out.append('#define PS4_EVTTR_RETURN 17u')
    out.append('#define PS4_EVTTR_RAM32 18u')
    out.append('#define PS4_EVTTR_PARTY_SYNC 19u')
    out.append('#define PS4_EVTTR_MOVE_CAMERA 20u')
    out.append('#define PS4_EVTTR_WAIT_UPDATES 21u')
    out.append('#define PS4_EVTTR_BRANCH_FLAG_SET 22u')
    out.append('#define PS4_EVTTR_ENSURE_SAVED_SOUND 23u')
    out.append('#define PS4_EVTTR_VINT 24u')
    out.append('#define PS4_EVTTR_FACE_OBJECT_OPPOSITE 25u')
    out.append('struct Ps4TransitionEventOp { uint8_t opcode; uint8_t arg8; uint16_t addr; uint32_t value; };')
    out.append(f'static const struct Ps4TransitionEventOp ps4_transition_event_ops[{len(transition_script_ops)}]={{')
    for opcode,arg8,addr,value in transition_script_ops:
        out.append(f'    {{{opcode}u,0x{arg8:02X}u,0x{addr:04X}u,0x{value:08X}u}},')
    out.append('};')
    out.append('struct Ps4TransitionEventScript { uint8_t event_index; uint16_t first_op; uint8_t op_count; };')
    out.append(f'static const struct Ps4TransitionEventScript ps4_transition_event_scripts[{len(transition_script_rows)}]={{')
    for idx,first,count,p,size in transition_script_rows:
        out.append(f'    {{0x{idx:02X}u,{first}u,{count}u}}, /* ${p:06X}, {size} bytes */')
    out.append('};')
    out.append(f'#define PS4_TRANSITION_EVENT_SCRIPT_COUNT {len(transition_script_rows)}u')
    out.append('struct Ps4SimpleRunEventCondition { uint8_t run_id; uint8_t flag; uint16_t event_index; uint8_t require_set; };')
    out.append(f'static const struct Ps4SimpleRunEventCondition ps4_simple_run_event_conditions[{len(conditions)}]={{')
    for rid,flag,event_index,require_set,t in conditions:
        out.append(f'    {{0x{rid:02X}u,0x{flag:02X}u,0x{event_index:04X}u,{require_set}u}}, /* ${t:06X} */')
    out.append('};')
    out.append(f'#define PS4_SIMPLE_RUN_EVENT_CONDITION_COUNT {len(conditions)}u')
    out.append('struct Ps4RunEventFlagChainCondition { uint8_t run_id; uint16_t event_index; uint8_t count; uint8_t bank0,flag0,require0,bank1,flag1,require1,bank2,flag2,require2; };')
    out.append(f'static const struct Ps4RunEventFlagChainCondition ps4_run_event_flag_chain_conditions[{len(flag_chain_conditions)}]={{')
    for rid,event_index,tests,t in flag_chain_conditions:
        flat=[]
        for n in range(3):
            flat.extend(tests[n] if n<len(tests) else (0,0,0))
        out.append(f'    {{0x{rid:02X}u,0x{event_index:04X}u,{len(tests)}u,' + ','.join(f'{v}u' if k%3 in (0,2) else f'0x{v:02X}u' for k,v in enumerate(flat)) + f'}}, /* ${t:06X} */')
    out.append('};')
    out.append(f'#define PS4_RUN_EVENT_FLAG_CHAIN_CONDITION_COUNT {len(flag_chain_conditions)}u')
    out.append('struct Ps4RunEventFlagCoordEqCondition { uint8_t run_id; uint16_t event_index; uint8_t count; uint8_t bank0,flag0,require0,bank1,flag1,require1,bank2,flag2,require2; uint16_t x,y; };')
    out.append(f'static const struct Ps4RunEventFlagCoordEqCondition ps4_run_event_flag_coord_eq_conditions[{len(flag_coord_eq_conditions)}]={{')
    for rid,event_index,tests,x,y,t in flag_coord_eq_conditions:
        flat=[]
        for n in range(3): flat.extend(tests[n] if n<len(tests) else (0,0,0))
        vals=','.join(f'{v}u' if k%3 in (0,2) else f'0x{v:02X}u' for k,v in enumerate(flat))
        out.append(f'    {{0x{rid:02X}u,0x{event_index:04X}u,{len(tests)}u,{vals},0x{x:04X}u,0x{y:04X}u}}, /* ${t:06X} */')
    out.append('};')
    out.append(f'#define PS4_RUN_EVENT_FLAG_COORD_EQ_CONDITION_COUNT {len(flag_coord_eq_conditions)}u')
    out.append('struct Ps4RunEventFlagCoordRangeCondition { uint8_t run_id; uint16_t event_index; uint8_t flag_count; uint8_t bank0,flag0,require0,bank1,flag1,require1,bank2,flag2,require2; uint8_t cmp_count; uint8_t axis0,op0; uint16_t value0; uint8_t axis1,op1; uint16_t value1; uint8_t axis2,op2; uint16_t value2; uint8_t axis3,op3; uint16_t value3; };')
    out.append(f'static const struct Ps4RunEventFlagCoordRangeCondition ps4_run_event_flag_coord_range_conditions[{len(flag_coord_range_conditions)}]={{')
    for rid,event_index,tests,comps,t in flag_coord_range_conditions:
        flat=[]
        for n in range(3):flat.extend(tests[n] if n<len(tests) else (0,0,0))
        fvals=','.join(f'{v}u' if k%3 in (0,2) else f'0x{v:02X}u' for k,v in enumerate(flat))
        cvals=[]
        for n in range(4):
            axis,op,value=comps[n] if n<len(comps) else (0,0,0)
            cvals.extend((f'{axis}u',f'{op}u',f'0x{value:04X}u'))
        out.append(f'    {{0x{rid:02X}u,0x{event_index:04X}u,{len(tests)}u,{fvals},{len(comps)}u,'+','.join(cvals)+f'}}, /* ${t:06X} */')
    out.append('};')
    out.append(f'#define PS4_RUN_EVENT_FLAG_COORD_RANGE_CONDITION_COUNT {len(flag_coord_range_conditions)}u')
    # Bind the four immutable condition classes once at build time.  Keep one
    # compact word per RunEventsJmpTbl ID: high byte class, low byte record.
    # The source classes are currently disjoint; fail generation if that ever
    # changes so runtime precedence cannot silently change.
    bound=[0xFFFF]*RUN_EVENT_COUNT
    for kind,arr in ((1,conditions),(2,flag_chain_conditions),(3,flag_coord_eq_conditions),(4,flag_coord_range_conditions)):
        for idx,row in enumerate(arr):
            rid=row[0]
            if bound[rid]!=0xFFFF:raise SystemExit(f'duplicate generated RunEvent condition id {rid:02X}')
            if idx>255:raise SystemExit('RunEvent condition index exceeds compact binding')
            bound[rid]=(kind<<8)|idx
    out.append('static const uint16_t ps4_run_event_condition_binding[128]={')
    for i in range(0,len(bound),16):out.append('    '+','.join(('0xFFFFu' if v==0xFFFF else f'0x{v:04X}u') for v in bound[i:i+16])+',')
    out.append('};')
    out.append(f'#define PS4_EVENT_PTRS_SOURCE_COUNT 0x{EVENT_COUNT:02X}u')
    out.append('#endif')
    a.output.parent.mkdir(parents=True,exist_ok=True)
    a.output.write_text('\n'.join(out)+'\n')
    print(f'PASS generated {len(rows)} exact dialogue+flag EventPtrs descriptors -> {a.output}')
    print('noop event indices:', ' '.join(f'{r[0]:02X}' for r in noop_rows))
    print('event indices:', ' '.join(f'{r[0]:02X}' for r in rows))
    print('facing-dialogue-flag indices:', ' '.join(f'{r[0]:02X}' for r in face_rows))
    print('simple battle-event indices:', ' '.join(f'{r[0]:02X}' for r in battle_rows))
    print('temp-clear indices:', ' '.join(f'{r[0]:02X}' for r in temp_clear_rows))
    print('dialogue-two-flags indices:', ' '.join(f'{r[0]:02X}' for r in two_flag_rows))
    print('dialogue-recovery indices:', ' '.join(f'{r[0]:02X}' for r in recovery_rows))
    print('conditional-dialogue-flag indices:', ' '.join(f'{r[0]:02X}' for r in conditional_dialogue_rows))
    print('tree-dialogue-flag indices:', ' '.join(f'{r[0]:02X}' for r in tree_dialogue_rows))
    print('object-dialogue-patch indices:', ' '.join(f'{r[0]:02X}' for r in object_dialogue_patch_rows))
    print('quick-dialogue battle indices:', ' '.join(f'{r[0]:02X}' for r in quick_battle_rows))
    print('guarded quick-battle indices:', ' '.join(f'{r[0]:02X}' for r in guarded_quick_rows))
    print('extended battle-event indices:', ' '.join(f'{r[0]:02X}' for r in extended_battle_rows))
    print('money-dialogue transaction indices:', ' '.join(f'{r[0]:02X}' for r in money_transaction_rows))
    print('coord-object conditional-dialogue indices:', ' '.join(f'{r[0]:02X}' for r in coord_object_dialogue_rows))
    print('paired-object dialogue-state indices:', ' '.join(f'{r[0]:02X}' for r in paired_object_dialogue_rows))
    print('rng-dialogue event indices:', ' '.join(f'{r[0]:02X}' for r in rng_dialogue_rows))
    print('dialogue-successor-flag indices:', ' '.join(f'{r[0]:02X}' for r in dialogue_successor_flag_rows))
    print('choreographed-followup dialogue indices:', ' '.join(f'{r[0]:02X}' for r in choreographed_followup_rows))
    print('dialogue-face-resume indices:', ' '.join(f'{r[0]:02X}' for r in dialogue_face_resume_rows))
    print('layout-animation event indices:', ' '.join(f'{r[0]:02X}' for r in layout_animation_rows))
    print('dynamic-two-cell layout event indices:', ' '.join(f'{r[0]:02X}' for r in dynamic_two_cell_layout_rows))
    print('fixed-two-row layout event indices:', ' '.join(f'{r[0]:02X}' for r in fixed_two_row_layout_rows))
    print('Garuberk door-entered event indices:', ' '.join(f'{r[0]:02X}' for r in garuberk_door_entered_rows))
    print('palette-alarm dialogue indices:', ' '.join(f'{r[0]:02X}' for r in palette_alarm_rows))
    print('palette-black battle indices:', ' '.join(f'{r[0]:02X}' for r in palette_black_battle_rows))
    print('transition-script event indices:', ' '.join(f'{r[0]:02X}' for r in transition_script_rows))
    print('flag-only RunEvent ids:', ' '.join(f'{r[0]:02X}' for r in conditions))
    print('flag-chain RunEvent ids:', ' '.join(f'{r[0]:02X}' for r in flag_chain_conditions))
    print('flag+coord-eq RunEvent ids:', ' '.join(f'{r[0]:02X}' for r in flag_coord_eq_conditions))
    print('flag+coord-range RunEvent ids:', ' '.join(f'{r[0]:02X}' for r in flag_coord_range_conditions))
if __name__=='__main__': main()
