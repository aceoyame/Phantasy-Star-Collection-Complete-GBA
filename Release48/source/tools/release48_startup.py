"""Skip the four publisher/developer logos, preserving normal PSC init/title.

The existing warm-start branch at 0x284 already skips the intro. Replace the
conditional branch that bypasses it with a NOP. Reclaim only the four adjacent
logo payloads (graphics, maps, palettes), all referenced from the skipped block.
The GBA boot header, PSC title/cards/music, native games and saves stay intact.
The pristine shell remains in donors/ for rollback and byte guards.
"""
from pathlib import Path
import struct
LOGO_START=0x4170
LOGO_END=0x9CAC
LOGO_LITERALS={0x32C:0x08004170,0x330:0x080068EA,0x334:0x08006988,
 0x350:0x080069A8,0x354:0x0800779C,0x358:0x080079F4,
 0x36C:0x08007BF4,0x370:0x08008E80,0x374:0x080090D8,
 0x4B4:0x080092D8,0x4B8:0x08009854,0x4BC:0x08009AAC}
def apply(shell):
 b=bytearray(shell);assert len(b)==0x800000
 assert b[0x280:0x286]==bytes.fromhex('002b00d0aae0'),'unexpected startup branch'
 for off,pointer in LOGO_LITERALS.items():assert struct.unpack_from('<I',b,off)[0]==pointer
 # Native warm-start destination, with all low-level initialization retained.
 assert b[0x3DC:0x3E0]==bytes.fromhex('02f0c2fb')
 b[0x282:0x284]=bytes.fromhex('c046')
 b[LOGO_START:LOGO_END]=b'\xff'*(LOGO_END-LOGO_START)
 return bytes(b)
