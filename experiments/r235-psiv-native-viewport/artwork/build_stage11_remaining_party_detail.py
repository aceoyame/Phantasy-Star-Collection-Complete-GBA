#!/usr/bin/env python3
"""Stage11: source-guided interior detail for the eight other party characters.

Requires licensed Stage10 ROM and party PNGs generated from the SHA-verified
original Genesis PSIV ROM with R198 generate_party_battle_frames.py.
No game binaries or commercial assets are included in this source file.
"""
from pathlib import Path
import argparse, hashlib, json, zlib
from collections import defaultdict
from struct import unpack_from
import numpy as np
from PIL import Image

PARENT_SHA = 'd08d6c88b5c6f6dee2ae15925864e5c98d58b113859d063476ac86ea97161d56'
GENESIS_SHA = '511f35cc11f88316f8b8940e28ab298bd75a4da193672a80172884d6eb913b6a'
EXPECTED_SHA = '820892b5fa2c145a249876baba90144152d7ff1520fb865e43d2103bc2bb80b2'
PALETTE = 0xCD415C
POSE_BYTES = 36*34
PARTY = {'alys':0xE5105C, 'hahn':0xE52D0C, 'gryz':0xE55818,
         'rika':0xE56670, 'demi':0xE587F0, 'wren':0xE59FD8,
         'raja':0xE5B7C0, 'seth':0xE5F120}

def sha(data):
    return hashlib.sha256(data).hexdigest()

def source_15(rgb):
    return sum((int(c) >> 3) << shift for c, shift in zip(rgb, (0, 5, 10)))

def build(stage10:Path, source_dir:Path, destination:Path):
    original = stage10.read_bytes()
    assert len(original) == 33554432 and sha(original) == PARENT_SHA, 'Stage10 hash mismatch'
    manifest = json.loads((source_dir/'manifest.json').read_text())
    assert manifest['source_rom_sha256'] == GENESIS_SHA, 'wrong Genesis reference'
    palette = unpack_from('<256H', original, PALETTE)
    valid_indices = [*range(1,80), *range(85,96), *range(112,192)]
    slots = defaultdict(list)
    for i in valid_indices:
        slots[palette[i]].append(i)
    result = bytearray(original)
    permitted = []
    details = []
    for name, addr in PARTY.items():
        item = manifest['characters'][name]
        sheet = Image.open(source_dir/item['pose_file']).convert('RGB')
        for frame, pose in enumerate(item['poses'][:2]):
            offset = addr + frame*POSE_BYTES
            original_frame = np.frombuffer(
                original[offset:offset+POSE_BYTES], dtype=np.uint8).reshape(34,36).copy()
            genesis_frame = np.asarray(
                sheet.crop((frame*64+8, 8, frame*64+56, 56))
                .resize((36,34), Image.Resampling.NEAREST))
            updated = original_frame.copy()
            count = 0
            for y in range(1,33):
                for x in range(1,35):
                    # Keep every silhouette and its 1px margin exactly as R235.
                    if not original_frame[y,x] or not np.all(
                        original_frame[y-1:y+2,x-1:x+2] != 0):
                        continue
                    matches = slots.get(source_15(genesis_frame[y,x]))
                    if not matches:
                        continue
                    replacement = (original_frame[y,x] if original_frame[y,x] in matches
                                   else matches[0])
                    if updated[y,x] != replacement:
                        updated[y,x] = replacement
                        count += 1
            assert np.array_equal(original_frame == 0, updated == 0)
            assert np.array_equal(original_frame[0], updated[0])
            assert np.array_equal(original_frame[-1], updated[-1])
            assert np.array_equal(original_frame[:,0], updated[:,0])
            assert np.array_equal(original_frame[:,-1], updated[:,-1])
            result[offset:offset+POSE_BYTES] = updated.tobytes()
            permitted.append((offset, offset+POSE_BYTES))
            details.append(dict(character=name,pose=pose,changed_pixels=count,
                                rom_offset=hex(offset)))
    changes = np.flatnonzero(np.frombuffer(original,dtype=np.uint8)
                            != np.frombuffer(result,dtype=np.uint8))
    assert len(changes) == 802
    assert all(any(start <= int(pos) < end for start,end in permitted) for pos in changes)
    assert result[:0xC0] == original[:0xC0]
    assert sha(result) == EXPECTED_SHA, 'source/binary differs from tested candidate'
    destination.write_bytes(result)
    record = dict(parent_sha256=PARENT_SHA, candidate_sha256=sha(result),
                  crc32=f'{zlib.crc32(result):08x}', rom_size=len(result),
                  altered_rom_bytes=len(changes), poses=details,
                  status='experimental: source art tested through Chaz-slot in-engine fixture; full party formation/attack/cast remains untested')
    destination.with_suffix('.manifest.json').write_text(json.dumps(record,indent=2)+'\n')
    return record

if __name__ == '__main__':
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('stage10_rom',type=Path)
    ap.add_argument('genesis_party_frames_dir',type=Path)
    ap.add_argument('output',type=Path)
    args = ap.parse_args()
    print(json.dumps(build(args.stage10_rom,args.genesis_party_frames_dir,args.output),indent=2))
