#!/usr/bin/env python3
"""Stage20 real-core opening/field/menu regression on SHA-pinned owner-supplied ROMs.

Nothing is patched, skipped, resampled, or uploaded to GitHub. This route is
independent of the Academy map17 transition; it must not be called acceptance.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import sys
import zlib

import numpy as np

STAGE12 = '5ff6d48606576e19ee46d14d7d8892b9835d4161afebde74f781caf76d38ff02'
STAGE14 = 'c1ed9db09505fe2d17322efa9f7c4915e7a4cd57bdf2a3dc6f24d0f8f1d15609'
CHECKS = [
    ('title', '', 0), ('newgame', 'START', 40), ('idle', '', 70),
    ('confirm', 'A', 20), ('after1', '', 120), ('confirm2', 'A', 20),
    ('after2', '', 120), ('confirm3', 'A', 20), ('field', '', 120),
    ('east', 'RIGHT', 25), ('west', 'LEFT', 25), ('north', 'UP', 25),
    ('south', 'DOWN', 25), ('menu', 'START', 30), ('close', 'B', 20),
]


def verified(path, digest):
    path = Path(path)
    if path.stat().st_size != 33554432:
        raise ValueError('Expected exact 32 MiB ROM: ' + str(path))
    actual = hashlib.sha256(path.read_bytes()).hexdigest()
    if actual != digest:
        raise ValueError('ROM SHA-256 mismatch: ' + actual)
    return path


def run():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--stage12', type=Path, required=True)
    p.add_argument('--stage14', type=Path, required=True)
    p.add_argument('--toolkit', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    args = p.parse_args()
    roms = (('stage12', verified(args.stage12, STAGE12)),
            ('stage14', verified(args.stage14, STAGE14)))
    qa = args.toolkit / 'qa'
    for relative in ('probe/mgba2.py', 'probe/probe.so',
                     'tests/vbam86.py', 'tests/ps4_intro_route.py',
                     'vbam/libretro/vbam_libretro.so',
                     'vbam/delta/vbam_libretro.so'):
        if not (qa / relative).is_file():
            raise FileNotFoundError(qa / relative)
    os.environ['CP08_PROBE_LIB'] = str((qa / 'probe/probe.so').resolve())
    sys.path[:0] = [str(qa / 'probe'), str(qa / 'tests')]
    from mgba2 import GBA
    from vbam86 import VBA
    from ps4_intro_route import start_ps4

    report = {}
    for core in ('mgba', 'vbam', 'delta'):
        if core != 'mgba':
            os.environ['PSC_VBAM_CORE'] = str(
                qa / ('vbam/delta/vbam_libretro.so' if core == 'delta'
                      else 'vbam/libretro/vbam_libretro.so'))
        runs = {}
        for label, rom in roms:
            game = GBA(rom) if core == 'mgba' else VBA(rom)
            start_ps4(game)
            game.run(3000)
            records = []
            for name, keys, frames in CHECKS:
                if frames:
                    game.run(frames, keys)
                shot = np.asarray(game.shot()).copy()
                records.append({
                    'checkpoint': name, 'frame': game.frame(),
                    'map_id': game.rd32(0x02024068) & 255,
                    'player_xy': [game.rd32(0x020240e4),
                                  game.rd32(0x020240e8)],
                    'framebuffer_crc32': f'{zlib.crc32(shot.tobytes()):08x}',
                })
            if hasattr(game, 'close'):
                game.close()
            runs[label] = records
        if runs['stage12'] != runs['stage14']:
            raise AssertionError('Live scene or framebuffer divergence: ' + core)
        report[core] = {'result': 'PASS', 'checkpoints': len(CHECKS),
                        'records': runs['stage14']}
    output = {
        'status': 'LIVE_THREE_CORE_REGRESSION_PASS',
        'rom_sha256': {'stage12': STAGE12, 'stage14': STAGE14},
        'route': 'start_ps4 + 3000 idle frames + CHECKS',
        'core_results': report,
        'limitations': [
            'Same-core parent/candidate parity only; CRCs are not compared across cores.',
            'Opening map94, not the Academy map18-to-17 source-blocker route.',
            'Not true Genesis parity, complete dialogue, FIN or GBA Micro acceptance.',
        ],
    }
    args.output.write_text(json.dumps(output, indent=2) + '\n')
    print(json.dumps({k: v['checkpoints'] for k, v in report.items()}, indent=2))


if __name__ == '__main__':
    run()
