#!/usr/bin/env python3
"""Read-only Stage17 GBA OBJ/OAM upload timeline for a locally supplied PSC ROM.

Requires the user's private headless mGBA toolkit (qa/probe/mgba2.py and probe.so).
Logs actual between-frame VRAM/OAM mutations and actor/sprite associations. A
byte difference proves an update occurred, NOT which CPU instruction, DMA event,
or compressed Genesis source supplied it. Never maps a tile_index to a pose.
No commercial art, ROMs, or save states are embedded in this program.

Example:
 python trace_stage17_obj_uploads.py --rom PSC_STAGE14.gba \
   --sha256 c1ed9db09505fe2d17322efa9f7c4915e7a4cd57bdf2a3dc6f24d0f8f1d15609 \
   --state academy.state --toolkit ./PSC_HEADLESS_EMULATOR_TOOLKIT_2026-11-16 \
   --expected-map 18 --route idle:3,right:24,left:24,up:24,down:24 \
   --output private_obj_trace.json
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

OBJ_BASE, OBJ_SIZE = 0x06010000, 0x8000
OAM_BASE, OAM_SIZE = 0x07000000, 0x400
PAL_BASE, PAL_SIZE = 0x05000200, 0x200
MAP_ID_ADDR = 0x02024068
ACTOR0_ADDR = 0x02026DAC  # R86 lineage reference, not a stable OAM slot map.
SHAPES = (
    ((8, 8), (16, 16), (32, 32), (64, 64)),
    ((16, 8), (32, 8), (32, 16), (64, 32)),
    ((8, 16), (8, 32), (16, 32), (32, 64)),
)


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def screenshot_crc(g) -> str:
    # Native 240x160 RGBA; no rescaling or inferred scene alignment.
    from mgba2 import L
    ptr = L.mg_pixels()
    return f"{zlib.crc32(ctypes.string_at(ptr, 240 * 160 * 4)):08x}"


def decode_oam(data: bytes, vram: bytes, palette: bytes, one_dimensional: bool) -> list[dict]:
    if len(data) != OAM_SIZE or len(vram) != OBJ_SIZE or len(palette) != PAL_SIZE:
        raise ValueError('Incomplete OAM, OBJ VRAM or sprite palette')
    import struct
    found = []
    for slot in range(128):
        a0, a1, a2, _ = struct.unpack_from('<4H', data, slot * 8)
        affine = bool(a0 & 0x100)
        if not affine and (a0 & 0x200):
            continue  # Disabled non-affine entry, not an animated character.
        shape = (a0 >> 14) & 3
        if shape == 3:
            continue
        w, h = SHAPES[shape][(a1 >> 14) & 3]
        x, y = a1 & 0x1FF, a0 & 0xFF
        if x >= 240:
            x -= 512
        if y >= 160:
            y -= 256
        bounds_w = w * (2 if affine and (a0 & 0x200) else 1)
        bounds_h = h * (2 if affine and (a0 & 0x200) else 1)
        if x >= 240 or y >= 160 or x + bounds_w <= 0 or y + bounds_h <= 0:
            continue
        depth = 8 if a0 & 0x2000 else 4
        tile = a2 & 0x3FF
        palette_bank = None if depth == 8 else (a2 >> 12) & 15
        nbytes = (w * h * depth) // 8
        # Attribute 2 tile number always has 32-byte units, including 8bpp.
        # In 2D mapping, rows have 32 tile indices; no invented pose identity.
        tile_bytes = bytearray()
        tiles_per_row = w // 8
        bytes_per_tile = 64 if depth == 8 else 32
        for ty in range(h // 8):
            for tx in range(tiles_per_row):
                addr = (tile * 32 + (ty * tiles_per_row + tx) * bytes_per_tile) if one_dimensional else (tile * 32 + ty * 1024 + tx * bytes_per_tile)
                if addr + bytes_per_tile > OBJ_SIZE:
                    tile_bytes = bytearray()
                    break
                tile_bytes += vram[addr:addr + bytes_per_tile]
            if not tile_bytes:
                break
        if len(tile_bytes) != nbytes:
            continue  # Invalid/out-of-range graphics, do not fabricate hash.
        pbytes = palette if depth == 8 else palette[palette_bank * 32:(palette_bank + 1) * 32]
        found.append({
            'slot': slot, 'xy': [x, y], 'dims': [w, h],
            'affine': affine, 'horizontal_flip': bool(a1 & 0x1000) if not affine else None,
            'vertical_flip': bool(a1 & 0x2000) if not affine else None,
            'tile_index': tile, 'palette_bank': palette_bank, 'bpp': depth,
            'raw_sha256': sha(tile_bytes), 'palette_sha256': sha(pbytes),
        })
    return found


def changed_tiles(before: bytes, after: bytes) -> list[dict]:
    if len(before) != OBJ_SIZE or len(after) != OBJ_SIZE:
        raise ValueError('OBJ VRAM must be 32768 bytes')
    out = []
    for i in range(0, OBJ_SIZE, 32):
        old, new = before[i:i + 32], after[i:i + 32]
        if old != new:
            out.append({'tile_index': i // 32,
                        'obj_vram_bus_address': f'0x{OBJ_BASE + i:08x}',
                        'changed_bytes': sum(a != b for a, b in zip(old, new)),
                        'before_sha256': sha(old), 'after_sha256': sha(new)})
    return out


def parse_route(value: str) -> list[tuple[str, int]]:
    accepted = {'idle', 'right', 'left', 'up', 'down', 'a', 'b', 'start', 'select'}
    route = []
    for segment in value.split(','):
        key, sep, count = segment.strip().partition(':')
        key = key.lower()
        if not sep or key not in accepted or not count.isdecimal() or not 1 <= int(count) <= 3000:
            raise ValueError(f'Invalid route segment: {segment!r}')
        route.append((key, int(count)))
    if sum(n for _, n in route) > 20000:
        raise ValueError('Refusing more than 20000 frames')
    return route


def snapshot(g, expected_map):
    dispcnt = g.rd16(0x04000000)
    one_d = bool(dispcnt & 0x40)
    vram = g.mem(OBJ_BASE, OBJ_SIZE)
    oam = g.mem(OAM_BASE, OAM_SIZE)
    pal = g.mem(PAL_BASE, PAL_SIZE)
    mid = g.rd32(MAP_ID_ADDR) & 0xFF
    if expected_map is not None and mid != expected_map:
        raise RuntimeError(f'Unexpected map transition: expected {expected_map}, got {mid}')
    return {'frame': g.frame(), 'map_id': mid, 'dispcnt': f'0x{dispcnt:04x}',
            'obj_1d_mapping': one_d, 'player_world_xy': [g.rd32(0x020240e4), g.rd32(0x020240e8)],
            'actor0_raw_hex': g.mem(ACTOR0_ADDR, 32).hex(),
            'screenshot_crc32': screenshot_crc(g),
            'obj_vram_sha256': sha(vram), 'oam_sha256': sha(oam),
            'obj_palette_sha256': sha(pal),
            'sprites': decode_oam(oam, vram, pal, one_d),
            '_vram': vram, '_oam': oam}


def run(args):
    rom = Path(args.rom)
    if not rom.is_file() or not args.sha256 or len(args.sha256) != 64:
        raise ValueError('A real input ROM and its exact 64-digit SHA256 are mandatory')
    digest = sha(rom.read_bytes())
    if digest != args.sha256.lower():
        raise ValueError(f'ROM SHA mismatch: observed {digest}; required {args.sha256}')
    if rom.stat().st_size != 32 * 1024 * 1024 and not args.synthetic_test:
        raise ValueError('PSC ROM must be exactly 32 MiB')
    probe = Path(args.toolkit) / 'qa' / 'probe'
    if not (probe / 'mgba2.py').is_file() or not (probe / 'probe.so').is_file():
        raise ValueError('Missing user-owned headless mGBA toolkit qa/probe')
    os.environ['CP08_PROBE_LIB'] = str((probe / 'probe.so').resolve())
    sys.path.insert(0, str(probe.resolve()))
    from mgba2 import GBA, L
    L.mg_video_watch.argtypes = [ctypes.c_int]
    L.mg_video_count.argtypes = [ctypes.c_uint]
    L.mg_video_count.restype = ctypes.c_uint64
    g = GBA(rom)
    if args.state:
        if not Path(args.state).is_file():
            raise FileNotFoundError(args.state)
        g.load(Path(args.state))
    prev = snapshot(g, args.expected_map)
    prev_public = {k: v for k, v in prev.items() if not k.startswith('_')}
    events = []
    phase = 0
    for key, count in parse_route(args.route):
        phase += 1
        for index in range(count):
            L.mg_video_watch(1)
            g.run(1, '' if key == 'idle' else key.upper())
            visible_cpu_obj_writes = int(L.mg_video_count(8))
            L.mg_video_watch(0)
            cur = snapshot(g, args.expected_map)
            vchanges = changed_tiles(prev['_vram'], cur['_vram'])
            oam_changed = cur['_oam'] != prev['_oam']
            pal_changed = cur['obj_palette_sha256'] != prev['obj_palette_sha256']
            if vchanges or oam_changed or pal_changed:
                relevant = {k: v for k, v in cur.items() if not k.startswith('_')}
                relevant.update({
                    'phase': phase, 'key': key, 'phase_frame': index + 1,
                    'changed_32byte_tile_slots': vchanges,
                    'oam_changed': oam_changed, 'palette_changed': pal_changed,
                    'visible_scanline_cpu_obj_stores': visible_cpu_obj_writes,
                })
                events.append(relevant)
            prev = cur
    result = {
        'type': 'stage17_readonly_obj_upload_timeline',
        'status': 'EMULATOR_EXECUTED' if not args.synthetic_test else 'SYNTHETIC_EMULATOR_SELF_TEST',
        'rom_sha256': digest, 'rom_size_bytes': rom.stat().st_size,
        'state_path_supplied': bool(args.state), 'route': args.route,
        'initial': prev_public, 'emulated_frames': sum(n for _, n in parse_route(args.route)),
        'mutation_frames': len(events), 'events': events,
        'source_provenance': 'UNKNOWN. CPU vs DMA uploader and original compressed sprite pointer NOT proved.',
        'limitations': ['Frame snapshots detect changed VRAM/OAM bytes, not all redundant stores.',
                        'visible_scanline_cpu_obj_stores excludes VBlank and DMA.',
                        'OAM slot and OBJ tile_index are temporary allocations, not character/Genesis pose IDs.',
                        'No automatic Genesis scene or actor-to-source matching.'],
    }
    Path(args.output).write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({'output': str(args.output), 'rom_sha256': digest,
                      'frames': result['emulated_frames'], 'mutation_frames': len(events)}, indent=2))
    return result


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--rom', required=True)
    p.add_argument('--sha256', required=True)
    p.add_argument('--toolkit', required=True)
    p.add_argument('--state')
    p.add_argument('--expected-map', type=int)
    p.add_argument('--route', default='idle:2,right:20,left:20,up:20,down:20')
    p.add_argument('--output', required=True)
    p.add_argument('--synthetic-test', action='store_true', help='Explicit tiny original test ROM only; never accepted PSC')
    run(p.parse_args())


if __name__ == '__main__':
    main()
