#!/usr/bin/env python3
"""Static channel/voice audit for the retail PSC PSII/PSIII native audio engines.

This does not compare against original Genesis music data. It proves only what
Release 48's retained GBA-native players can schedule/mix concurrently.
"""
from __future__ import annotations
import argparse, json, zlib
from pathlib import Path

GAMES = {
    "PSII": {
        "audio_region": (0x394000, 0x397000),
        "mixer_counter": 0x395172,  # movs r2,#5 in six-line summation loop
        "scheduler_cmp": 0x395204,  # cmp r4,#5 / ble
        "scheduler_stride_halfword": 0x395200, # adds r5,#0x40
        "init_cmp": 0x395340,       # cmp r3,#5 / ble
        "record_stride": 0x40,
        "mixer_rom": 0x08395530,
        "phase_rom": 0x083955B8,
    },
    "PSIII": {
        "audio_region": (0x082000, 0x084000),
        "mixer_counter": 0x082CAA,  # movs r2,#5 in six-line summation loop
        "scheduler_cmp": 0x082D10,  # cmp r4,#5 / ble
        "scheduler_stride_halfword": 0x082D0C, # adds r5,#0x58
        "init_cmp": 0x082EBA,       # cmp r3,#5 / ble
        "record_stride": 0x58,
        "voice_base": 0x0201F430,
        "mixer_rom": 0x08083140,
        "phase_rom": 0x080831C8,
    },
}

def h16(buf: bytes, off: int) -> int:
    return int.from_bytes(buf[off:off+2], "little")

def find_cmp5_ble(buf: bytes, start: int, end: int):
    hits=[]
    for off in range(start, end-3, 2):
        a=h16(buf,off); b=h16(buf,off+2)
        if (a & 0xF8FF) == 0x2805 and (b & 0xFF00) == 0xDD00:
            hits.append({
                "file_offset": f"0x{off:X}",
                "cmp_halfword": f"0x{a:04X}",
                "register": (a >> 8) & 7,
                "branch_halfword": f"0x{b:04X}",
                "meaning": "loop while counter <= 5 (six iterations from zero)",
            })
    return hits

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("rom")
    ap.add_argument("-o","--out",required=True)
    a=ap.parse_args()
    p=Path(a.rom); rom=p.read_bytes()
    assert len(rom) >= 8*1024*1024
    report={
        "rom": p.name,
        "crc32": f"{zlib.crc32(rom)&0xffffffff:08x}",
        "scope": "Static audit of retained PSC-native PSII/PSIII audio scheduler and mixer loops; not a Genesis source-channel comparison.",
        "conclusion": "Both retained native engines have six concurrent scheduled/mixed voice slots. This is a hard player-side ceiling in these paths; waveform smoothing cannot restore source parts that were omitted before or during conversion.",
        "caution": "Six native slots alone does not identify which original FM/PSG/DAC part is absent, nor prove that every song used more than six source parts. Source-sequence comparison is required before adding/reassigning channels.",
        "games": {}
    }
    for name,g in GAMES.items():
        s,e=g["audio_region"]
        mixer=h16(rom,g["mixer_counter"])
        sched=h16(rom,g["scheduler_cmp"]); schedbr=h16(rom,g["scheduler_cmp"]+2)
        init=h16(rom,g["init_cmp"]); initbr=h16(rom,g["init_cmp"]+2)
        stride=h16(rom,g["scheduler_stride_halfword"])
        # expected encodings: MOVS r2,#5 = 0x2205; CMP rN,#5 = 0x2N05;
        # conditional LE = 0xDDxx. Stride opcode is documented separately.
        assert mixer == 0x2205, (name,hex(g["mixer_counter"]),hex(mixer))
        assert (sched & 0xF8FF)==0x2805 and (schedbr & 0xFF00)==0xDD00
        assert (init & 0xF8FF)==0x2805 and (initbr & 0xFF00)==0xDD00
        report["games"][name]={
            "native_mixer_rom": f"0x{g['mixer_rom']:08X}",
            "native_phase_rom": f"0x{g['phase_rom']:08X}",
            "record_stride_bytes": g["record_stride"],
            "voice_base_iwram_or_ewram": (f"0x{g['voice_base']:08X}" if "voice_base" in g else None),
            "mixer_sum_loop": {
                "counter_file_offset": f"0x{g['mixer_counter']:X}",
                "counter_halfword": f"0x{mixer:04X}",
                "decoded": "MOVS r2,#5; loop decrements through 5,4,3,2,1,0 => six mixed scratch lines",
            },
            "scheduler_loop": {
                "compare_file_offset": f"0x{g['scheduler_cmp']:X}",
                "compare_halfword": f"0x{sched:04X}",
                "branch_halfword": f"0x{schedbr:04X}",
                "decoded": "counter starts at 0, increments by 1, continues while <=5 => six scheduled slots",
                "record_stride_observed_halfword": f"0x{stride:04X}",
            },
            "initializer_loop": {
                "compare_file_offset": f"0x{g['init_cmp']:X}",
                "compare_halfword": f"0x{init:04X}",
                "branch_halfword": f"0x{initbr:04X}",
                "decoded": "initialization loop also terminates at slot 5",
            },
            "all_cmp_imm5_le_loops_in_audio_region": find_cmp5_ble(rom,s,e),
            "concurrent_native_voice_slots": 6,
        }
    Path(a.out).write_text(json.dumps(report,indent=2)+"\n")
    print(json.dumps({"out":a.out,"crc32":report["crc32"],"slots":{"PSII":6,"PSIII":6}},indent=2))

if __name__=="__main__": main()
