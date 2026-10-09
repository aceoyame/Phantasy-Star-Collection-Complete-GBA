#!/usr/bin/env python3
"""Boot isolated diagnostic copies. Slow guest execution is an open issue.

Does not alter the playable ROM, certify commercial games, or require old states.
"""
from pathlib import Path
import json, struct, sys, zlib

P = Path(__file__).resolve().parents[1]
R = P.parents[1]
sys.path.insert(0, str(R / 'source/tools_host/r46_mgba'))
from mgba import GBA

O = P / 'build'
O.mkdir(exist_ok=True)
base = (R / 'binaries/RUN_THIS_RELEASE48_PSC.gba').read_bytes()
blob = (O / 'psc8.bin').read_bytes()
assert len(base) == 0x2000000
assert 0 < len(blob) <= 0x738000 - 0x6CA500
assert not any(base[0x6CA4EC:0x738000]), 'PSII padding no longer available'
assert set(base[0x7F2800:0x7FA800]) <= {0}, 'Diagnostic cartridge slot occupied'
rows = []
for name, mode in [('GG', 1), ('FM', 2), ('PSI', 2)]:
    b = bytearray(base)
    b[0x6CA500:0x6CA500 + len(blob)] = blob
    if name == 'PSI':
        cart = (R / 'additional_roms' / 'Phantasy_Star_1_Retranslated_(English_v2.6.1).sms').read_bytes()
        assert len(cart) == 524288
        # Use a verified-empty high ROM slot in this isolated diagnostic image.
        b[0x1800000:0x1880000] = cart
        addr = 0x09800000
    else:
        cart = (P / 'tests' / ('GG_SELFTEST.gg' if name == 'GG' else 'FM_SELFTEST.sms')).read_bytes()
        assert len(cart) == 32768
        b[0x7F2800:0x7FA800] = cart
        addr = 0x087F2800
    rom = O / ('diagnostic_' + name + '.gba')
    rom.write_bytes(b)
    g = GBA(rom, entry=0x086CA500)
    g.put(0x0203FF00, struct.pack('<IIII', addr, len(cart), mode, 0))
    g.run(900)
    g.shot(O / ('diagnostic_' + name + '.png'))
    v = struct.unpack('<14I', g.mem(0x0203E000, 56))
    assert v[0] == 0x50534338 and v[1] > 0, (name, v)
    if name in ('FM','PSI'):
        assert v[3] > 0 and v[9] == 1, v
    if name == 'GG':
        assert v[4] > 0 and v[10] == 16, v
        im = g.shot()
        assert im.getpixel((40, 8))[0] > 200 and im.getpixel((199, 151))[0] > 200
        assert im.getpixel((0, 0)) == (0, 0, 0)
    row = dict(name=name, display_frames=900, guest_frames=v[1], guest_pc=v[2],
               fm_ports=v[3], gg_ports=v[4], audio_blocks=v[5], late_blocks=v[6],
               z80_cycles=v[7], fm_control=v[9], stereo=v[10])
    rows.append(row)
    print(json.dumps(row), flush=True)
report = dict(scope='Isolated PSC8 runtime using RAM configuration; validates real PSI FM writes and GG hardware path. Performance certification remains separate.',
              base_rom_crc32=f'{zlib.crc32(base):08x}', module_bytes=len(blob), cases=rows)
(O / 'runtime_results.json').write_text(json.dumps(report, indent=2) + '\n')
print('PASS diagnostic execution; timing/audio limitations remain')
