#!/usr/bin/env python3
"""Instruction-level OAM publication trace for the isolated R235 viewport experiment.

Required private inputs: SHA-pinned accepted R235 or Stage14 ROM, SHA-pinned
Academy after-principal mGBA state and user-owned headless mGBA probe.
No cartridge edits, RAM pokes, or copyrighted data embedded in this source.

Example:
 python trace_stage20_oam_writer.py --rom ACCEPTED.gba --variant accepted \
   --state academy_settled.state --toolkit headless_toolkit \
   --output PRIVATE_stage20_writer_trace.json

ARM PC in this mGBA bridge is prefetched: actual store instruction is
PC-4 (ARM), or PC-2 (Thumb). Record both to avoid incorrect source addresses.
"""
import argparse
import ctypes
import hashlib
import json
import os
from pathlib import Path
import sys

ROM_HASHES = {
    'accepted': '4caf23ae8b030c16a32658fdc95085330de6c6dd4a51498f0e2b9c17c7d89308',
    'stage14': 'c1ed9db09505fe2d17322efa9f7c4915e7a4cd57bdf2a3dc6f24d0f8f1d15609',
}
STATE_HASH = '3a18bcce1f725baaeac2114f20edda5aca4aa4e482464aba6f15ee21b0454ae3'
# Identical original controller tape used in the Stage17/Stage18 Academy tests.
ROUTE = (('', 25), ('RIGHT', 20), ('', 16), ('LEFT', 18),
         ('UP', 12), ('DOWN', 12), ('', 55))
SLOTS = (0, 2, 40)
WINDOWS = (
    (0x03002900, 0x09EEE9B0),
    (0x03002A80, 0x09EEEB30),
    (0x03002AC0, 0x09EEEB70),
    (0x03002CC0, 0x09EEED70),
    (0x03002DC0, 0x09EEEE70),
)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def verify(path, digest, size=None):
    p = Path(path)
    if not p.is_file() or (size is not None and p.stat().st_size != size):
        raise ValueError('Missing/incorrect-size private input: ' + str(p))
    observed = sha(p.read_bytes())
    if observed != digest:
        raise ValueError('SHA256 mismatch for ' + str(p) + ': ' + observed)
    return p


def tracked_oam(game):
    return tuple(game.rd32(0x07000000 + 8 * slot + 4 * word)
                 for slot in SLOTS for word in (0, 1))


def main():
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--rom', required=True)
    p.add_argument('--variant', required=True, choices=tuple(ROM_HASHES))
    p.add_argument('--state', required=True)
    p.add_argument('--toolkit', required=True)
    p.add_argument('--output', required=True)
    args = p.parse_args()
    rom = verify(args.rom, ROM_HASHES[args.variant], 33554432)
    state = verify(args.state, STATE_HASH, 397312)
    probe = Path(args.toolkit) / 'qa' / 'probe'
    if not (probe / 'mgba2.py').is_file() or not (probe / 'probe.so').is_file():
        raise FileNotFoundError('mGBA toolkit qa/probe not staged')
    os.environ['CP08_PROBE_LIB'] = str((probe / 'probe.so').resolve())
    sys.path.insert(0, str(probe.resolve()))
    from mgba2 import GBA, L
    L.mg_steps_exact.argtypes = [ctypes.c_uint32]
    L.mg_cpu_flags.restype = ctypes.c_uint32

    game = GBA(rom)
    game.load(state)
    if game.frame() != 16445 or (game.rd32(0x02024068) & 255) != 18:
        raise ValueError('Wrong Academy state/frame')
    for key, frames in ROUTE:
        for _ in range(frames):
            if game.frame() >= 16570:
                break
            game.run(1, key)
        if game.frame() >= 16570:
            break
    if game.frame() != 16570 or (game.rd32(0x02024068) & 255) != 17:
        raise AssertionError('Wrong map-transition input tape')

    # Prove the live IWRAM graphics writer is the copied ROM code in this ROM.
    rom_bytes = rom.read_bytes()
    origins = []
    for iwram_addr, rom_addr in WINDOWS:
        b = game.mem(iwram_addr, 256)
        off = rom_addr - 0x08000000
        if b != rom_bytes[off:off + len(b)] or rom_bytes.find(b, off + 1) >= 0:
            raise AssertionError('IWRAM source window failed unique ROM match')
        origins.append({'iwram': hex(iwram_addr), 'rom': hex(rom_addr),
                        'size': len(b), 'sha256': sha(b)})

    previous = tracked_oam(game)
    entries = []
    steps = 0
    frame_edges = []
    last_frame = game.frame()
    while game.frame() < 16573 and steps < 350000:
        before_pc = game.reg(15)
        before_flags = L.mg_cpu_flags()
        before_frame = game.frame()
        before_cycles = game.cycles()
        L.mg_steps_exact(1)
        steps += 1
        after = tracked_oam(game)
        if after != previous:
            thumb = bool(before_flags & 0x20)
            insn_pc = before_pc - (2 if thumb else 4)
            opcode = game.rd16(insn_pc) if thumb else game.rd32(insn_pc)
            changes = [
                {'slot': SLOTS[i // 2], 'word_index': i % 2,
                 'before': hex(a), 'after': hex(b)}
                for i, (a, b) in enumerate(zip(previous, after)) if a != b
            ]
            entries.append({
                'instruction_step': steps, 'before_frame': before_frame,
                'after_frame': game.frame(), 'cycles_before': before_cycles,
                'pc_prefetched': hex(before_pc), 'instruction_address': hex(insn_pc),
                'opcode': hex(opcode), 'cpsr': hex(before_flags),
                'changed_oam': changes,
                'registers_after': [hex(game.reg(i)) for i in range(16)],
            })
            if len(entries) > 100:
                raise AssertionError('Unexpected OAM churn; cap exceeded')
        previous = after
        if game.frame() != last_frame:
            frame_edges.append({'frame': game.frame(), 'instruction_step': steps})
            last_frame = game.frame()
    if game.frame() != 16573:
        raise AssertionError('Could not complete three real mGBA frames')
    if len(entries) < 10:
        raise AssertionError('Observed no expected sprite-publication writes')
    result = {
        'status': 'LIVE_MGBA_INSTRUCTION_STEP_READ_ONLY',
        'variant': args.variant, 'rom_sha256': ROM_HASHES[args.variant],
        'rom_size_bytes': rom.stat().st_size, 'state_sha256': STATE_HASH,
        'start_frame': 16570, 'end_frame': game.frame(), 'cpu_steps': steps,
        'frame_edges': frame_edges, 'oam_writes': entries,
        'rom_backed_iwram_windows': origins,
        'limitations': [
            'CPU PC from bridge is prefetched; actual instruction_address is adjusted.',
            'Only OAM slots 0, 2 and 40 tracked; does not identify all sprite writers.',
            'CPU step records writes but not original Genesis pose source.',
            'No ROM modifications, two-core acceptance, save rewrite, or physical Micro test.',
        ],
    }
    Path(args.output).write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({'status': result['status'], 'variant': args.variant,
                      'steps': steps, 'oam_write_events': len(entries),
                      'frames': frame_edges}, indent=2))


if __name__ == '__main__':
    main()
