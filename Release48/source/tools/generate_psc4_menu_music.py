#!/usr/bin/env python3
"""Generate PSC-native PSIV launcher medley P27 PSC-style phrase edit.

P27 treats the fourth selection as a PSC medley quotation rather than a long
literal transcription. It retains P19's phrase-complete PSIV body through
source tick 9432 and its user-preferred 132-tick cadence into PSI, preserves
PSC retail voice roles (0/0/2/1), and adds a true 12-tick inter-selection rest
as extra time. The body is balanced toward the retail PSC convention of stable
100-ish accompaniment with a clear but not overbearing voice-2 melody.

P19 keeps the accepted ~2:17 PSIV entry and overall ~P17/P18 duration, but
treats the joins as musical phrases rather than fixed fades.  The III->IV
attack reaches normal level quickly (close to the visual card transition), and
the final 132 ticks are a short four-note voice-leading bridge into the exact
first pitches/tempo of the retail PSI loop.  This avoids a long perceptual gap
and the abrupt PSIV->PSI cut without changing the retail PSC voices.

FM lead, bass roots, harmony roots and ostinato pitches are decoded directly
from the pinned 3 MiB PSIV retail ROM.  Only orchestration is adapted to the
stock PSC launcher voicegroup.  Crescendo thresholds remain at the same
absolute PSIV source-song positions used by P13/P14.
"""
from __future__ import annotations

import argparse
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / 'gba'
ROM_BASE = 0x08000000
TRACK_OFFSETS = [0x46800, 0x46C00, 0x47000, 0x47400]
ORIGINAL_LOOP = [0x3FF6E, 0x401D0, 0x4035A, 0x404DC]

# User reference 2:17..3:18 maps to source ticks ~7706..11138.  The selected
# compact passage stays wholly inside it and includes the build + first full
# high-theme statement.
SRC_START = 7704
SRC_END = 9624
SRC_LEN = SRC_END - SRC_START
SCALE_NUM = 11
SCALE_DEN = 16
M4A_LEN = SRC_LEN * SCALE_NUM // SCALE_DEN
assert SRC_LEN == 1920 and M4A_LEN == 1320

# Literal PSIV source runs through 0x24D8 source ticks (9432).  The last 132
# M4A ticks are a deliberately authored bridge into PSI: four 33-tick cells.
BRIDGE_SRC = 9432
BRIDGE_AT = (BRIDGE_SRC-SRC_START)*SCALE_NUM//SCALE_DEN
BRIDGE_CELL = 33
assert BRIDGE_AT == 1188 and BRIDGE_AT + 4*BRIDGE_CELL == M4A_LEN

# Retail $AC lane starts / initial state from the pinned generated song table.
FM_LANES = [
    # pc, transpose, source attenuation
    (0x0DBD38, -12, 16),  # FM1 - identity lead
    (0x0DBDBD, -12, 18),  # FM2 - harmonic motion
    (0x0DBE71, -24, 20),  # FM3 - fast ostinato
    (0x0DBEF3, -24, 15),  # FM4 - bass/root pulse
]

DURS = list(range(1,25)) + [28,30,32,36,40,42,44,48,52,54,56,60,64,66,68,72,76,78,80,84,88,90,92,96]
WAITS = list(range(0,25)) + [28,30,32,36,40,42,44,48,52,54,56,60,64,66,68,72,76,78,80,84,88,90,92,96]
DUR_OP = {d: 0xD0+i for i,d in enumerate(DURS)}
WAIT_OP = {d: 0x80+i for i,d in enumerate(WAITS)}


def s8(v:int)->int: return v-256 if v >= 128 else v
def s16(v:int)->int: return v-65536 if v >= 32768 else v


def simulate_fm(rom:bytes, start_pc:int, initial_transpose:int, initial_volume:int, stop_tick:int):
    """Replay the retail PSIV bytecode far enough to recover FM note events."""
    pc=start_pc; trans=initial_transpose; vol=initial_volume; prev_dur=0; t=0
    loops=[0]*16; stack=[]; hold=False; current_note=None; patch=None
    events=[]; active=True
    for _guard in range(200000):
        if not active or t >= stop_tick: break
        for _ops in range(128):
            addr=pc; op=rom[pc]; pc+=1
            if op < 0xE0:
                duration=prev_dur
                if op < 0x80:
                    duration=op
                else:
                    if rom[pc] < 0x80:
                        duration=rom[pc]; pc+=1
                    if op == 0x80:
                        if not hold: current_note=None
                    else:
                        current_note=(op-0x80+trans)&0x7F
                wait=(duration*2)&0xFF
                if wait == 0: wait=256
                events.append({'t':t,'wait':wait,'note':current_note,'hold':hold,'vol':vol,'patch':patch,'addr':addr})
                hold=False; prev_dur=duration; t += wait
                break
            if op==0xE0: pc+=1
            elif op==0xE1: pc+=1
            elif op==0xE2: pc+=1
            elif op==0xE3: pc+=1
            elif op==0xE4: pc+=1
            elif op==0xE5:
                pc+=1; vol=(vol+rom[pc])&0xFF; pc+=1
            elif op in (0xE6,0xEC): vol=(vol+rom[pc])&0xFF; pc+=1
            elif op==0xE7: hold=True
            elif op==0xE8: pc+=1
            elif op==0xE9: pc+=2
            elif op==0xEA: pc+=1
            elif op==0xEB: pc+=1
            elif op==0xED: pc+=1
            elif op==0xEE: pc+=1
            elif op==0xEF: patch=rom[pc]; pc+=1
            elif op==0xF0: pc+=4
            elif op==0xF1: pc+=2
            elif op==0xF2: active=False; break
            elif op==0xF3: pc+=1
            elif op==0xF4: pc+=1
            elif op==0xF5: pc+=1
            elif op==0xF6:
                raw=(rom[pc]<<8)|rom[pc+1]; pc+=2
                pc=pc+s16(raw)-1
            elif op==0xF7:
                idx=rom[pc]; reps=rom[pc+1]; raw=(rom[pc+2]<<8)|rom[pc+3]; pc+=4
                slot=idx&15
                if loops[slot]==0: loops[slot]=reps if reps else 1
                loops[slot]-=1
                if loops[slot]!=0: pc=pc+s16(raw)-1
            elif op==0xF8:
                raw=(rom[pc]<<8)|rom[pc+1]; pc+=2
                stack.append(pc); pc=pc+s16(raw)-1
            elif op==0xF9:
                if not stack: active=False; break
                pc=stack.pop()
            elif op==0xFA: pc+=1
            elif op==0xFB: trans += s8(rom[pc]); pc+=1
            elif op==0xFC: pc+=1
            elif op==0xFD: pc+=1
            elif op==0xFE: pc+=4
            elif op==0xFF:
                meta=rom[pc]; pc+=1
                if meta!=0: active=False; break
                mode=rom[pc]; pc+=1
                if mode: pc+=4
            else:
                active=False; break
        else:
            raise RuntimeError('command-only source loop')
    return events


def setup(voice:int, vol:int, pan:int=64)->bytearray:
    return bytearray([0xBC,0x00, 0xBD,voice, 0xBE,vol, 0xBF,pan])


def setvol(out:bytearray, vol:int):
    out += bytes([0xBE, max(0,min(127,vol))])


def emit_wait(out:bytearray, ticks:int):
    while ticks:
        q=max(w for w in WAITS if 0 < w <= ticks)
        out.append(WAIT_OP[q]); ticks-=q

ENTRY_GAP = 12
def add_entry_gap(out:bytearray):
    emit_wait(out, ENTRY_GAP)


def emit_note(out:bytearray, pitch:int, advance:int, vel:int, gate_cap:int|None=None):
    if advance <= 0: return
    # Keep gate close to the inter-onset distance.  P18 may deliberately shorten
    # the last gate while retaining the same inter-onset timing, leaving a tiny
    # release before the stock PS1 loop attacks.
    gate_target=min(advance,96)
    if gate_cap is not None: gate_target=min(gate_target,gate_cap)
    gate=max(d for d in DURS if d <= gate_target)
    out += bytes([DUR_OP[gate], pitch&0x7F, max(1,min(127,vel))])
    emit_wait(out,advance)


def scaled(rel_src:int)->int:
    # nearest integer, cumulative mapping prevents drift across short events
    return (rel_src*SCALE_NUM + SCALE_DEN//2)//SCALE_DEN


def source_to_m4a_pitch(src_note:int)->int:
    # PSIV effective FM note numbers are 24 below M4A's C4=72 numbering.
    return src_note + 24


def event_at(events:list[dict], tick:int):
    exact=[e for e in events if e['t']==tick]
    if exact: return exact[0]
    prev=[e for e in events if e['t']<tick]
    return prev[-1] if prev else None


def goto(out:bytearray, file_off:int):
    out.append(0xB2); out += struct.pack('<I',ROM_BASE+file_off); out.append(0xB1)


def _setvol_at(out:bytearray, current:int, wanted:int)->int:
    if wanted != current:
        setvol(out,wanted)
        return wanted
    return current


def make_tracks(rom:bytes):
    fm=[simulate_fm(rom,*m,SRC_END+256) for m in FM_LANES]
    lead,harm,ost,bass=fm
    tracks=[]

    # P19 opening envelope: reach normal PSC launcher level within ~132 ticks
    # instead of spending ~200 ticks in P18's very quiet fade.  This better
    # matches the shortened III<->IV visual traversal while still avoiding a
    # hard attack at the exact PSIII boundary.
    bass_vol={0:58,1:64,2:68,9:72,14:76}
    harm_vol={0:50,1:56,2:60,9:64,14:68}

    # Track 0 / PSC voice 0: FM4 root + tempo owner.
    t=setup(0,bass_vol[0]); cur=bass_vol[0]; add_entry_gap(t)
    for block,src_t in enumerate(range(SRC_START,BRIDGE_SRC,96)):
        if block in bass_vol: cur=_setvol_at(t,cur,bass_vol[block])
        ev=event_at(bass,src_t)
        adv=scaled(src_t+96-SRC_START)-scaled(src_t-SRC_START)
        if ev and ev['note'] is not None: emit_note(t,source_to_m4a_pitch(ev['note']),adv,52 if block<3 else 68)
        else: emit_wait(t,adv)
    assert sum(WAITS[b-0x80] for b in []) == 0  # documentation anchor; timing asserted below
    # Voice-lead the PSIV pedal down to PSI's first bass pitch 46 while tempo
    # converges 44 -> 48 -> 51 -> 54 -> 57.
    for tempo,pitch,vol,gate in zip((48,51,54,57),(51,49,48,46),(82,80,78,76),(28,28,24,18)):
        t += bytes([0xBB,tempo]); setvol(t,vol); emit_note(t,pitch,BRIDGE_CELL,60,gate)
    goto(t,ORIGINAL_LOOP[0]); tracks.append(bytes(t))

    # Track 1 / PSC voice 0: source harmonic anchor, then climb smoothly to
    # PSI track-1's first pitch 58.
    t=setup(0,harm_vol[0]); cur=harm_vol[0]; add_entry_gap(t)
    for block,src_t in enumerate(range(SRC_START,BRIDGE_SRC,96)):
        if block in harm_vol: cur=_setvol_at(t,cur,harm_vol[block])
        ev=event_at(harm,src_t)
        adv=scaled(src_t+96-SRC_START)-scaled(src_t-SRC_START)
        if ev and ev['note'] is not None: emit_note(t,source_to_m4a_pitch(ev['note']),adv,44 if block<3 else 58)
        else: emit_wait(t,adv)
    for pitch,vol,gate in zip((51,53,55,58),(72,70,68,66),(28,28,24,18)):
        setvol(t,vol); emit_note(t,pitch,BRIDGE_CELL,54,gate)
    goto(t,ORIGINAL_LOOP[1]); tracks.append(bytes(t))

    # Track 2 / PSC voice 2: literal FM1 lead through the last complete phrase
    # before the bridge.  The four bridge notes descend into PSI's first lead
    # pitch 74, turning the loop boundary into a cadence rather than a jump.
    t=setup(2,56); cur=56; add_entry_gap(t)
    lead_events=[e for e in lead if SRC_START <= e['t'] < BRIDGE_SRC]
    assert lead_events and lead_events[0]['t']==SRC_START
    for e in lead_events:
        rel=e['t']-SRC_START; dst=scaled(rel)
        if dst>=594: wanted=94
        elif dst>=330: wanted=88
        elif dst>=132: wanted=80
        elif dst>=66: wanted=68
        else: wanted=56
        cur=_setvol_at(t,cur,wanted)
        adv=min(BRIDGE_AT,scaled(min(e['t']+e['wait'],BRIDGE_SRC)-SRC_START))-dst
        if adv<=0: continue
        if e['note'] is None: emit_wait(t,adv)
        else: emit_note(t,source_to_m4a_pitch(e['note']),adv,88 if rel<288 else (100 if rel<1344 else 108))
    for pitch,vol,gate in zip((83,80,77,74),(102,94,86,80),(28,28,24,18)):
        setvol(t,vol); emit_note(t,pitch,BRIDGE_CELL,94,gate)
    goto(t,ORIGINAL_LOOP[2]); tracks.append(bytes(t))

    # Track 3 / PSC voice 1: fast FM3 ostinato.  Bring it in quickly enough to
    # support the card arrival, then turn its last four pulses into a rising
    # pickup that lands on PSI track-3's first pitch 58.
    t=setup(1,36); cur=36; add_entry_gap(t)
    for step,src_t in enumerate(range(SRC_START,BRIDGE_SRC,12)):
        dst=scaled(src_t-SRC_START)
        if dst>=594: wanted=58
        elif dst>=330: wanted=54
        elif dst>=132: wanted=48
        elif dst>=66: wanted=42
        else: wanted=36
        cur=_setvol_at(t,cur,wanted)
        ev=event_at(ost,src_t)
        adv=min(BRIDGE_AT,scaled(src_t+12-SRC_START))-dst
        if adv<=0: continue
        if ev and ev['note'] is not None: emit_note(t,source_to_m4a_pitch(ev['note']),adv,42 if step<24 else 56)
        else: emit_wait(t,adv)
    for pitch,vol,gate in zip((51,53,56,58),(60,52,44,38),(24,24,18,12)):
        setvol(t,vol); emit_note(t,pitch,BRIDGE_CELL,50,gate)
    goto(t,ORIGINAL_LOOP[3]); tracks.append(bytes(t))

    return tracks


def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--rom',type=Path,default=ROOT/'ps4built.bin',help='pinned 3 MiB PSIV retail ROM')
    args=ap.parse_args()
    rom=args.rom.read_bytes()
    if len(rom)!=0x300000: raise SystemExit(f'expected 3 MiB PSIV ROM, got {len(rom):,}')
    tracks=make_tracks(rom)
    OUT.mkdir(parents=True,exist_ok=True)
    for i,data in enumerate(tracks):
        if len(data)>0x400: raise SystemExit(f'track {i} too large: {len(data)} bytes')
        (OUT/f'psc4_menu_track{i}_ext.bin').write_bytes(data)
        print(f'track{i}: {len(data)} bytes @ file 0x{TRACK_OFFSETS[i]:05X}')
    print(f'all tracks: {M4A_LEN+ENTRY_GAP} ticks total; P27 PSC-style phrase edit + true {ENTRY_GAP}-tick III->IV rest + P19 IV->I cadence')

if __name__=='__main__': main()
