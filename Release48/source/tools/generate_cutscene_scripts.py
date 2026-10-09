#!/usr/bin/env python3
"""Statically lower exact simple PSIV FieldRoutine_Cutscene bodies to typed ops.

This generator does not execute 68000 instructions on GBA.  It accepts only a
small audited instruction/helper vocabulary and rejects a cutscene immediately
when it encounters an opcode or helper with semantics outside that vocabulary.
The resulting table is therefore safe to extend as more PSIV helpers are
translated without turning the ARM runtime into an emulator.
"""
from __future__ import annotations
import argparse, hashlib
from pathlib import Path

EXPECTED='511f35cc11f88316f8b8940e28ab298bd75a4da193672a80172884d6eb913b6a'
CUT_PTRS=0x5A580
CUT_COUNT=0x22
CUT_TABLE_END=0x5A608
CUT_PREP=0x05A658
CUT_DIALOGUE=0x05ADF8
CUT_REFRESH_MAP=0x05AE98
PAL_FADE_IN=0x0421D4
EVENT_FLAG_SET=0x057666

OP_SOUND=1
OP_DIALOGUE=2
OP_FLAG_SET=3
OP_RAM8=4
OP_RAM16=5
OP_ADD32=6
OP_REFRESH_MAP=7
OP_FADE_IN=8
OP_PREP=9


def ptrs(data:bytes):
    p=[int.from_bytes(data[CUT_PTRS+i*4:CUT_PTRS+i*4+4],'big') for i in range(CUT_COUNT)]
    if data[CUT_TABLE_END:CUT_TABLE_END+4] != bytes.fromhex('13 fc 00 fe'):
        raise SystemExit('unexpected FieldRoutine_Cutscene table end marker')
    return p


def emit(op,a=0,addr=0,value=0):
    return (op,a,addr,value)


def parse(body:bytes):
    """Return exact typed ops or None if any instruction/helper is unsupported."""
    i=0; d0=None; out=[]
    while i < len(body):
        # move.b #imm,$abs.l -- currently only Sound_Index is accepted.
        if body[i:i+2]==b'\x13\xfc' and i+8<=len(body):
            imm=int.from_bytes(body[i+2:i+4],'big')&0xFF; addr=int.from_bytes(body[i+4:i+8],'big')
            if addr != 0xFFFF500A: return None
            out.append(emit(OP_SOUND,imm));i+=8;continue
        # move.b #imm,$abs.w
        if body[i:i+2]==b'\x11\xfc' and i+6<=len(body):
            imm=int.from_bytes(body[i+2:i+4],'big')&0xFF;addr=int.from_bytes(body[i+4:i+6],'big')
            out.append(emit(OP_RAM8,imm,addr));i+=6;continue
        # move.w #imm,$abs.w
        if body[i:i+2]==b'\x31\xfc' and i+6<=len(body):
            imm=int.from_bytes(body[i+2:i+4],'big');addr=int.from_bytes(body[i+4:i+6],'big')
            out.append(emit(OP_RAM16,0,addr,imm));i+=6;continue
        # move.w d0,$abs.w -- only if D0 is statically known.
        if body[i:i+2]==b'\x31\xc0' and i+4<=len(body):
            if d0 is None:return None
            addr=int.from_bytes(body[i+2:i+4],'big');out.append(emit(OP_RAM16,0,addr,d0&0xFFFF));i+=4;continue
        # clr.b $abs.w
        if body[i:i+2]==b'\x42\x38' and i+4<=len(body):
            addr=int.from_bytes(body[i+2:i+4],'big');out.append(emit(OP_RAM8,0,addr));i+=4;continue
        # addi.l #imm,$abs.w
        if body[i:i+2]==b'\x06\xb8' and i+8<=len(body):
            val=int.from_bytes(body[i+2:i+6],'big');addr=int.from_bytes(body[i+6:i+8],'big')
            out.append(emit(OP_ADD32,0,addr,val));i+=8;continue
        # moveq #imm,d0
        if i+2<=len(body) and body[i]==0x70:
            v=body[i+1];d0=v-256 if v&0x80 else v;i+=2;continue
        # jsr/jmp absolute long
        if body[i:i+2] in (b'\x4e\xb9',b'\x4e\xf9') and i+6<=len(body):
            is_jmp=body[i+1]==0xF9;target=int.from_bytes(body[i+2:i+6],'big')
            if target==CUT_PREP:out.append(emit(OP_PREP))
            elif target==PAL_FADE_IN:out.append(emit(OP_FADE_IN))
            elif target==CUT_DIALOGUE:
                if d0 is None:return None
                out.append(emit(OP_DIALOGUE,d0&0xFF))
            elif target==CUT_REFRESH_MAP:out.append(emit(OP_REFRESH_MAP))
            elif target==EVENT_FLAG_SET:
                if d0 is None:return None
                out.append(emit(OP_FLAG_SET,d0&0xFF))
            else:return None
            i+=6
            if is_jmp:
                # Tail JMP acts as return. Only accept if source body ends here.
                if i != len(body):return None
                break
            continue
        if body[i:i+2]==b'\x4e\x75':
            if i+2!=len(body):return None
            i+=2;break
        return None
    return out


def main():
    ap=argparse.ArgumentParser();ap.add_argument('--rom',required=True,type=Path);ap.add_argument('--output',type=Path,default=Path('gba/generated_cutscene_scripts_v0982.h'));a=ap.parse_args()
    data=a.rom.read_bytes();sha=hashlib.sha256(data).hexdigest()
    if len(data)!=0x300000 or sha!=EXPECTED:raise SystemExit(f'bad PSIV source {len(data):#x} {sha}')
    p=ptrs(data);scripts=[];ops=[]
    # Index 0 points to Event_NoEvent, outside the cutscene body region.  It is an
    # intentional no-op; real source cutscenes begin at index 1.
    for idx in range(1,CUT_COUNT):
        start=p[idx];end=p[idx+1] if idx+1<CUT_COUNT else None
        if end is None:continue  # final body boundary is not inferred; reject until explicitly sourced.
        if not (0<=start<end<=len(data)):raise SystemExit(f'bad cutscene range {idx:02X} {start:X}-{end:X}')
        q=parse(data[start:end])
        if q is not None:
            first=len(ops);ops.extend(q);scripts.append((idx,first,len(q),start,end-start))
    out=[]
    out += ['/* Auto-generated from verified PSIV US ROM by generate_cutscene_scripts.py. */','#ifndef PS4_GENERATED_CUTSCENE_SCRIPTS_V0982_H','#define PS4_GENERATED_CUTSCENE_SCRIPTS_V0982_H']
    out += [f'#define PS4_CUTOP_SOUND {OP_SOUND}u',f'#define PS4_CUTOP_DIALOGUE {OP_DIALOGUE}u',f'#define PS4_CUTOP_FLAG_SET {OP_FLAG_SET}u',f'#define PS4_CUTOP_RAM8 {OP_RAM8}u',f'#define PS4_CUTOP_RAM16 {OP_RAM16}u',f'#define PS4_CUTOP_ADD32 {OP_ADD32}u',f'#define PS4_CUTOP_REFRESH_MAP {OP_REFRESH_MAP}u',f'#define PS4_CUTOP_FADE_IN {OP_FADE_IN}u',f'#define PS4_CUTOP_PREP {OP_PREP}u']
    out.append('struct Ps4CutsceneOp { uint8_t opcode; uint8_t arg8; uint16_t addr; uint32_t value; };')
    out.append(f'static const struct Ps4CutsceneOp ps4_cutscene_ops[{len(ops)}]={{')
    for op,a8,addr,val in ops:out.append(f'    {{{op}u,0x{a8:02X}u,0x{addr:04X}u,0x{val:08X}u}},')
    out.append('};')
    out.append('struct Ps4CutsceneScript { uint8_t cutscene_index; uint16_t first_op; uint8_t op_count; };')
    out.append(f'static const struct Ps4CutsceneScript ps4_cutscene_scripts[{len(scripts)}]={{')
    for idx,first,count,start,size in scripts:out.append(f'    {{0x{idx:02X}u,{first}u,{count}u}}, /* ${start:06X}, {size} bytes */')
    out.append('};')
    out.append(f'#define PS4_CUTSCENE_SCRIPT_COUNT {len(scripts)}u')
    out.append(f'#define PS4_CUTSCENE_SOURCE_COUNT 0x{CUT_COUNT:02X}u')
    out.append('#endif')
    a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text('\n'.join(out)+'\n')
    print(f'PASS generated {len(scripts)} exact cutscene scripts / {len(ops)} ops -> {a.output}')
    print('cutscene indices:', ' '.join(f'{s[0]:02X}' for s in scripts))

if __name__=='__main__':main()
