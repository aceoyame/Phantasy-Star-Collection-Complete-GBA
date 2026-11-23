#!/usr/bin/env python3
"""Host-only self-tests; no game ROM data or emulator core required."""
import importlib.util
from pathlib import Path
import struct
import unittest

SRC = Path(__file__).with_name('trace_stage17_obj_uploads.py')
spec = importlib.util.spec_from_file_location('trace_stage17', SRC)
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)


class TestStage17(unittest.TestCase):
    def test_reused_obj_slot_is_not_a_pose_identity(self):
        before = bytes(m.OBJ_SIZE)
        first = bytearray(before)
        first[448*32:448*32+32] = bytes(range(32))
        update1 = m.changed_tiles(before, bytes(first))
        self.assertEqual([v['tile_index'] for v in update1], [448])
        self.assertEqual(update1[0]['changed_bytes'], 31)
        second = bytearray(first)
        second[448*32:448*32+32] = bytes([0xff]*32)
        update2 = m.changed_tiles(bytes(first), bytes(second))
        self.assertEqual([v['tile_index'] for v in update2], [448])
        self.assertNotEqual(update2[0]['before_sha256'], update2[0]['after_sha256'])

    def test_native_1d_4bpp_and_8bpp_sprite_geometry(self):
        oam = bytearray(m.OAM_SIZE)
        for i in range(128):
            struct.pack_into('<4H', oam, i*8, 0x0200, 0, 0, 0)
        # Original-size GBA 16x32 4bpp sprite, tile 448; 2D vertical shape.
        struct.pack_into('<4H', oam, 0, 0x8000 | 10, 0x8000 | 20, 448 | (2 << 12), 0)
        # 16x16 8bpp sprite, tile 480, using 64 bytes per 8x8 tile.
        struct.pack_into('<4H', oam, 8, 0x2000 | 22, 0x4000 | 45, 480, 0)
        vram = bytearray(m.OBJ_SIZE)
        vram[448*32:448*32+256] = bytes(range(256))
        vram[480*32:480*32+256] = bytes(range(256))
        sprites = m.decode_oam(bytes(oam), bytes(vram), bytes(m.PAL_SIZE), True)
        self.assertEqual([(s['tile_index'], s['dims'], s['bpp']) for s in sprites],
                         [(448, [16,32], 4), (480, [16,16], 8)])

    def test_route_rejects_unsafe_lengths(self):
        with self.assertRaises(ValueError):
            m.parse_route('right:-1')
        with self.assertRaises(ValueError):
            m.parse_route('right:20001')
        self.assertEqual(m.parse_route('idle:1,right:3'), [('idle',1), ('right',3)])


if __name__ == '__main__':
    unittest.main()
