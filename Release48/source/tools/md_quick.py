#!/usr/bin/env python3
"""Developer loop: rebuild the Genesis runtime (+ menu) and patch them into an existing CP08-layout ROM.
  python3 tools/md_quick.py IN.gba OUT.gba [--debug]"""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import gbabuild
src, dst = sys.argv[1], sys.argv[2]
dbg = '--debug' in sys.argv
rom = bytearray(open(src, 'rb').read())
md, elf = gbabuild.build_md(debug=dbg)
assert len(md) <= 0x730000 - 0x6e2000, len(md)
rom[0x6e2000:0x730000] = bytes(0x730000 - 0x6e2000); rom[0x6e2000:0x6e2000 + len(md)] = md
menu, _ = gbabuild.build_menu()
assert len(menu) <= 0x7f00 - 0x5000
rom[0x5000:0x7f00] = b'\xff' * (0x7f00 - 0x5000); rom[0x5000:0x5000 + len(menu)] = menu
open(dst, 'wb').write(rom)
print('md', len(md), 'iwram_end', hex(gbabuild.sym(elf, '__hot_end')), 'menu', len(menu))
