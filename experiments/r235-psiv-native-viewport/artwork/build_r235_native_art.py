#!/usr/bin/env python3
"""Build a guarded, experimental original-Genesis-art R235 test ROM.

No Sega ROM/assets are embedded. The operator provides archives previously made
from their own game and source files. This experiment changes: manga IDs 0..9,
39 small portraits, and optionally preserves Stage4 native field camera.

python build_r235_native_art.py --r235 R235.gba --stage4 STAGE4.gba \
  --r175 PSC_R175_CUMULATIVE_HANDOFF.zip --r203 PSC_R203_COMPLETE.zip \
  --r234 PSC_R234_SPACE_AUDIO_FIX_HANDOFF.zip --out EXPERIMENT.gba

NOT a complete conversion. The selected x=48 viewport loses ~2.8% of nonblack
source pixels in sampled manga panels; cutscene composition remains unverified.
"""
import argparse
import hashlib
import json
import struct
import sys
import tempfile
import zipfile
import zlib
from pathlib import Path

import numpy as np

PARENT_SHA = '4caf23ae8b030c16a32658fdc95085330de6c6dd4a51498f0e2b9c17c7d89308'
GENESIS_SHA = '511f35cc11f88316f8b8940e28ab298bd75a4da193672a80172884d6eb913b6a'
STAGE4_SHA = 'f3a80bad91b6d3f1809bf1e09ea09f887e204428aeb55247bf67426caf03f89d'
TEST_SHA = 'c95c2830de0cfe6bb271edfbfe6b8ea26f32a960dffd3e9818c390713529c954'
BANK = 0x091A10CC
FREESTART = 0x0922B000
FREEEND = 0x09240000
PORTRAITS = 0xE627C4
CROP_X, CROP_Y = 48, 0


def sha(data):
    return hashlib.sha256(data).hexdigest()


def imported_sources(a, b, tmp):
    with zipfile.ZipFile(a) as z:
        for f in ('r196/build_r196.py', 'r196/md_compression.py', 'r196/projection.py'):
            target = tmp/f
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(z.read(f))
    with zipfile.ZipFile(b) as z:
        target = tmp/'portraits.py'
        target.write_bytes(z.read('source/src/portraits.py'))
    sys.path.insert(0, str(tmp/'r196'))
    sys.path.insert(0, str(tmp))
    import build_r196 as R
    import portraits as P
    from md_compression import nemesis_decompress
    return R, P, nemesis_decompress


def source_palette(image, panel, R):
    stream, length, po, mask, valid = struct.unpack_from('<IHHBBxx', image, R.PANEL_DESC + 12*panel)
    assert valid
    pal = np.zeros(64, dtype=np.int32)
    pos = R.PANEL_BANK + R.PANEL_PALETTES + po*2
    for bank in range(4):
        if mask & (1 << bank):
            pal[bank*16:bank*16+16] = struct.unpack_from('<16H', image, pos)
            pos += 32
    return pal


def portrait_dest(x, y):
    if y < 32:
        tile = (y//8)*4+x//8 if x < 32 else 16+y//8
    else:
        tile = 20+x//8 if x < 32 else 24
    return tile*32 + (y%8)*4+(x%8)//2, (x & 1)*4


def build_art(image, genesis, R, P, nemesis):
    allocation = FREESTART - 0x08000000
    assert image[allocation:FREEEND-0x08000000] == bytes(FREEEND - FREESTART), 'nonfree ROM'
    details = []
    for panel in range(10):
        raw, bounds = R.decode_panel(genesis, panel >> 6, panel & 63,
                                      source_palette(image, panel, R))
        x0, y0, x1, y1 = bounds
        left = max(x0, CROP_X)
        right = min(x1, CROP_X+240)
        top = max(y0, CROP_Y)
        bottom = min(y1, CROP_Y+160)
        spans = [((y-CROP_Y)*240 + (left-CROP_X),
                  raw[y, left:right].astype('uint8').tobytes())
                 for y in range(top, bottom) if left < right]
        encoded = P.encode(spans)
        expected = bytearray(38400)
        for pos, payload in spans:
            assert len(payload) <= 255
            expected[pos:pos+len(payload)] = payload
        assert P.decode(encoded, bytearray(38400)) == expected
        assert 0 < len(encoded) <= 65535
        assert allocation + len(encoded) <= FREEEND-0x08000000
        image[allocation:allocation+len(encoded)] = encoded
        struct.pack_into('<IH', image, R.PANEL_DESC+panel*12,
                         0x08000000+allocation-BANK, len(encoded))
        details.append(dict(id=panel, compressed_bytes=len(encoded), bounds=list(bounds)))
        allocation += len(encoded)
    portraits = []
    for i in range(1, 40):
        pointer = struct.unpack_from('>I', genesis, 0x6A4B0+i*4)[0]
        tiles = nemesis(genesis[pointer:])
        assert len(tiles) >= 36*32
        base = PORTRAITS + (i-1)*800
        count = 0
        for y in range(2, 38):
            for x in range(2, 38):
                sx, sy = x+4, y+4
                idx = ((sy//8)*6 + sx//8)*32+(sy%8)*4+(sx%8)//2
                byte = tiles[idx]
                value = (byte & 15) if sx & 1 else (byte >> 4)
                off, shift = portrait_dest(x, y)
                before = image[base+off] >> shift & 15
                count += before != value
                image[base+off] = (image[base+off] & ~(15 << shift)) | (value << shift)
        portraits.append(dict(id=i, changed_pixels=count))
    return details, portraits, allocation


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('r235', 'r175', 'r203', 'r234', 'out'):
        p.add_argument('--'+name, type=Path, required=True)
    p.add_argument('--stage4', type=Path)
    a = p.parse_args()
    original = a.r235.read_bytes()
    assert len(original) == 33554432 and sha(original) == PARENT_SHA
    with zipfile.ZipFile(a.r175) as z:
        genesis = z.read('reference/US_PSIV_fe236442.bin')
    assert len(genesis) == 3145728 and sha(genesis) == GENESIS_SHA
    with tempfile.TemporaryDirectory() as folder:
        R, P, nemesis = imported_sources(a.r203, a.r234, Path(folder))
        image = bytearray(original)
        manga, portraits, end = build_art(image, genesis, R, P, nemesis)
    allowed = [(R.PANEL_DESC, R.PANEL_DESC+120),
               (FREESTART-0x08000000, end),
               (PORTRAITS, PORTRAITS+39*800)]
    changed = np.flatnonzero(np.frombuffer(original, dtype='uint8') != np.frombuffer(image,dtype='uint8'))
    assert all(any(lo <= int(i) < hi for lo,hi in allowed) for i in changed)
    if a.stage4:
        field = a.stage4.read_bytes()
        assert len(field) == len(original) and sha(field) == STAGE4_SHA
        field_changes = np.flatnonzero(np.frombuffer(original,dtype='uint8') != np.frombuffer(field,dtype='uint8'))
        assert set(changed).isdisjoint(map(int,field_changes))
        result = bytearray(field)
        for lo,hi in allowed:result[lo:hi] = image[lo:hi]
        assert sha(result) == TEST_SHA, 'candidate differs from already tested experimental image'
    else:
        result = image
    assert result[:0xC0] == original[:0xC0]
    a.out.write_bytes(result)
    summary = dict(parent_sha=PARENT_SHA, genesis_sha=GENESIS_SHA,
                   result_sha=sha(result), result_crc32=f'{zlib.crc32(result):08x}',
                   rom_bytes=len(result), manga_panels=manga, portraits=portraits,
                   crop=[CROP_X,CROP_Y,240,160], stage4_included=bool(a.stage4),
                   status='TEST ROM ONLY. In-game cutscene/portrait QA and full playthrough not yet verified')
    a.out.with_suffix('.manifest.json').write_text(json.dumps(summary, indent=2))
    print(json.dumps({k:summary[k] for k in ('result_sha','result_crc32','rom_bytes','crop','stage4_included')},indent=2))


if __name__ == '__main__':main()
