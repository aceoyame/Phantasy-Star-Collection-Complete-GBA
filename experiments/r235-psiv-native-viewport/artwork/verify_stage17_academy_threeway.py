#!/usr/bin/env python3
"""Read-only three-cartridge, frame-exact PSC Academy BG/OBJ validation.

Requires real user-supplied ROMs, private Academy state and headless mGBA toolkit.
No ROM, commercial image or save-state data is embedded in this source.

python verify_stage17_academy_threeway.py \
  --accepted accepted_R235.gba --stage12 stage12.gba --stage14 stage14.gba \
  --state academy_map18.state --toolkit PSC_HEADLESS_EMULATOR_TOOLKIT_2026-11-16 \
  --output private_stage17_verification.json

Never modifies a cartridge and rejects any ROM hash that differs from the
privately verified exact baseline. A match is scoped to the specified route.
"""
from __future__ import annotations
import argparse
import ctypes
import hashlib
import json
import os
from pathlib import Path
import sys
import zlib

EXPECTED = {
    'accepted': '4caf23ae8b030c16a32658fdc95085330de6c6dd4a51498f0e2b9c17c7d89308',
    'stage12': '5ff6d48606576e19ee46d14d7d8892b9835d4161afebde74f781caf76d38ff02',
    'stage14': 'c1ed9db09505fe2d17322efa9f7c4915e7a4cd57bdf2a3dc6f24d0f8f1d15609',
}
ROUTE = (('idle', 25), ('right', 20), ('idle', 16), ('left', 18),
         ('up', 12), ('down', 12), ('idle', 55))


def verify_rom(path, expected):
    p = Path(path)
    if not p.is_file() or p.stat().st_size != 33554432:
        raise ValueError('Expected real 32 MiB cartridge: ' + str(p))
    actual = hashlib.sha256(p.read_bytes()).hexdigest()
    if actual != expected:
        raise ValueError('ROM SHA mismatch for ' + str(p) + ': ' + actual)
    return p


def run(args):
    roms = {key: verify_rom(getattr(args, key), sha)
            for key, sha in EXPECTED.items()}
    state = Path(args.state)
    if not state.is_file():
        raise FileNotFoundError(state)
    probe = Path(args.toolkit) / 'qa' / 'probe'
    for name in ('mgba2.py', 'probe.so'):
        if not (probe / name).is_file():
            raise FileNotFoundError(probe / name)
    os.environ['CP08_PROBE_LIB'] = str((probe / 'probe.so').resolve())
    sys.path.insert(0, str(probe.resolve()))
    from mgba2 import GBA, L
    import numpy as np
    data = {}
    for key, rom in roms.items():
        g = GBA(rom)
        g.load(state)
        if (g.rd32(0x02024068) & 0xff) != 18:
            raise ValueError('Not an Academy map18 savestate')
        records = []
        for button, count in ROUTE:
            for _ in range(count):
                g.run(1, '' if button == 'idle' else button.upper())
                rgba = ctypes.string_at(L.mg_pixels(), 240 * 160 * 4)
                records.append({
                    'frame': g.frame(),
                    'map_id': g.rd32(0x02024068) & 0xff,
                    'player_xy': (g.rd32(0x020240e4), g.rd32(0x020240e8)),
                    'framebuffer_crc32': f'{zlib.crc32(rgba):08x}',
                    'obj_sha256': hashlib.sha256(g.mem(0x06010000, 0x8000)).hexdigest(),
                    'palette_sha256': hashlib.sha256(g.mem(0x05000200, 0x200)).hexdigest(),
                    '_rgba': np.frombuffer(rgba, dtype=np.uint8).reshape(160, 240, 4),
                })
        data[key] = records

    accepted, stage12, stage14 = (data[x] for x in ('accepted', 'stage12', 'stage14'))
    if not len(accepted) == len(stage12) == len(stage14) == 158:
        raise AssertionError('Frame-count mismatch')
    fields = ('frame', 'map_id', 'player_xy', 'framebuffer_crc32',
              'obj_sha256', 'palette_sha256')
    stage_diff = {k: sum(x[k] != y[k] for x, y in zip(stage12, stage14))
                  for k in fields}
    accepted_diff = {k: sum(x[k] != y[k] for x, y in zip(accepted, stage14))
                     for k in fields}
    if any(stage_diff.values()):
        raise AssertionError('Stage12 / Stage14 live frame divergence: ' +
                             repr(stage_diff))
    if any(accepted_diff[k] for k in ('frame', 'map_id', 'player_xy',
                                     'obj_sha256', 'palette_sha256')):
        raise AssertionError('Accepted / Stage14 state or sprite memory divergence')

    classified = []
    for x, y in zip(accepted, stage14):
        full = bool(np.array_equal(x['_rgba'], y['_rgba']))
        cropped = bool(np.array_equal(x['_rgba'][8:, 16:],
                                      y['_rgba'][:152, :224]))
        overlap = float(np.mean(np.all(
            x['_rgba'][8:, 16:] == y['_rgba'][:152, :224], axis=2)))
        kind = ('original_identical' if full else 'shifted_exact' if cropped
                else 'transition_or_warmup')
        classified.append({'frame': x['frame'], 'map_id': x['map_id'],
                           'class': kind, 'shifted_overlap': round(overlap, 6)})
    segments = []
    for item in classified:
        if not segments or segments[-1]['class'] != item['class']:
            segments.append({'from': item['frame'], 'to': item['frame'],
                             'class': item['class']})
        else:
            segments[-1]['to'] = item['frame']
    transitions = [accepted[i]['frame'] for i in range(1, len(accepted))
                   if accepted[i]['map_id'] != accepted[i-1]['map_id']]
    result = {
        'status': 'LIVE_MGBA_ACADEMY_EXECUTED_NO_ROM_PATCH',
        'roms_sha256': EXPECTED, 'rom_size_bytes': 33554432,
        'state_sha256': hashlib.sha256(state.read_bytes()).hexdigest(),
        'route': ROUTE, 'executed_frames_per_rom': len(accepted),
        'first_frame': accepted[0]['frame'], 'last_frame': accepted[-1]['frame'],
        'map_transition_frames': transitions,
        'stage12_stage14_full_frame_diff_counts': stage_diff,
        'accepted_stage14_full_frame_diff_counts': accepted_diff,
        'screen_classification_runs': segments,
        'transient_frames': [x for x in classified
                             if x['class'] == 'transition_or_warmup'],
        'limitations': [
            'mGBA only: VBA-M/Delta not run on this exact state',
            'One Academy route: not whole-game, save/load or physical hardware acceptance',
            'A shifted overlap proves crop equivalence, NOT original Genesis pose provenance',
            'Never upload real user ROM, screenshots, states or extracted commercial art to public GitHub',
        ],
    }
    Path(args.output).write_text(json.dumps(result, indent=2) + '\n')
    return result


def main():
    p = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    for name in ('accepted', 'stage12', 'stage14', 'state', 'toolkit', 'output'):
        p.add_argument('--' + name, required=True)
    result = run(p.parse_args())
    print(json.dumps({k: result[k] for k in (
        'status', 'executed_frames_per_rom', 'map_transition_frames',
        'stage12_stage14_full_frame_diff_counts',
        'accepted_stage14_full_frame_diff_counts',
        'screen_classification_runs')}, indent=2))


if __name__ == '__main__':
    main()
