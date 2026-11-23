#!/usr/bin/env python3
"""Stage14: source-faithful static tiles for Zoran Bult, Gicefalgue and Xe-A-Thoul.

No commercial game content embedded. Supply your Stage12 GBA cartridge, original
US Genesis PSIV ROM, and r198/reference_tools + r199 original source utilities.
Do not upload input/output ROMs to public GitHub. This is a test cartridge.
"""
from pathlib import Path
from hashlib import sha256
import argparse, struct, zlib, json, sys

STAGE12_SHA = '5ff6d48606576e19ee46d14d7d8892b9835d4161afebde74f781caf76d38ff02'
GENESIS_SHA = '511f35cc11f88316f8b8940e28ab298bd75a4da193672a80172884d6eb913b6a'
STAGE14_SHA = 'c1ed9db09505fe2d17322efa9f7c4915e7a4cd57bdf2a3dc6f24d0f8f1d15609'
TARGETS = {0x0A: 'Zoran Bult', 0x0B: 'Gicefalgue', 0x7B: 'Xe-A-Thoul x3'}

def digest(data):
    return sha256(data).hexdigest()

def gba_color(w):
    return sum((((w >> src) & 7) * 255 // 7 >> 3) << dst
               for src, dst in ((1,0), (5,5), (9,10)))

def build(stage12, genesis, tools):
    assert len(stage12) == 33554432 and digest(stage12) == STAGE12_SHA
    assert len(genesis) == 3145728 and digest(genesis) == GENESIS_SHA
    sys.path[:0] = [str(tools/'r198'/'reference_tools'), str(tools/'r199')]
    from md_compression import nemesis_decompress, enigma_decompress
    from enemy_tiles import animated_tiles, NATIVE, BODY, MAP, PALETTE
    output = bytearray(stage12)
    permitted = []
    records = []
    for eid, name in TARGETS.items():
        _, anim_ptr, bp, mp, halfwidth, height = struct.unpack_from(
            '>IIII2B', genesis, 0x27F3AE + eid*20 + 2)
        original_body = nemesis_decompress(genesis[bp:])
        original_map = enigma_decompress(genesis[mp:])
        _, _, bptr, blen, mptr, cells, width, height_gba = struct.unpack_from(
            '<8I', stage12, NATIVE + eid*32)
        assert len(original_body) == blen and width == 2*halfwidth and height_gba == height
        assert cells == width * height
        assert tuple(original_map[:cells]) == struct.unpack_from(
            f'<{cells}H', stage12, MAP + mptr)
        pal = (0, 0xEEE, 0,
               *struct.unpack_from('>11H', genesis, 0x27FFA2 + eid*22), 0, 0)
        assert tuple(map(gba_color, pal)) == struct.unpack_from(
            '<16H', stage12, PALETTE + eid*32)
        protected = animated_tiles(genesis, eid)
        changed_bytes = 0
        changed_tiles = 0
        for tile in range(blen//32):
            at = BODY + bptr + tile*32
            if tile in protected:
                assert output[at:at+32] == stage12[at:at+32]
                continue
            correct = original_body[tile*32:(tile+1)*32]
            changed = sum(a != b for a,b in zip(stage12[at:at+32], correct))
            if changed:
                output[at:at+32] = correct
                changed_bytes += changed
                changed_tiles += 1
            permitted.append((at,at+32))
        records.append(dict(enemy=f'{eid:02X}',name=name,
                            pixel_size=[width*8,height*8],
                            changed_bytes=changed_bytes,
                            changed_static_tiles=changed_tiles,
                            protected_animation_tiles=len(protected)))
    changed_offsets = [i for i,(a,b) in enumerate(zip(stage12,output)) if a != b]
    assert all(any(a<=i<b for a,b in permitted) for i in changed_offsets)
    assert output[:0xC0] == stage12[:0xC0]
    assert len(changed_offsets) == 583 and digest(output) == STAGE14_SHA
    return bytes(output), dict(parent_sha256=STAGE12_SHA,
                              genesis_sha256=GENESIS_SHA,
                              candidate_sha256=digest(output),
                              crc32=f'{zlib.crc32(output):08x}',
                              rom_bytes=len(output),
                              changed_bytes=len(changed_offsets),
                              enemies=records,
                              status='Experimental. Three-core battle and field QA only; no full playthrough.')

if __name__ == '__main__':
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--stage12',type=Path,required=True)
    ap.add_argument('--genesis',type=Path,required=True)
    ap.add_argument('--tools',type=Path,required=True,
                    help='Extracted R203 scripts root containing r198 and r199')
    ap.add_argument('--output',type=Path,required=True)
    args=ap.parse_args()
    rom,meta=build(args.stage12.read_bytes(),args.genesis.read_bytes(),args.tools)
    args.output.write_bytes(rom)
    args.output.with_suffix('.manifest.json').write_text(json.dumps(meta,indent=2)+'\n')
    print(json.dumps(meta,indent=2))
