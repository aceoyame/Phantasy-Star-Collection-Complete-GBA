#!/usr/bin/env python3
"""CP09 builder (CP08 + Genesis adventure sound): patches the CP07 safe ROM (CRC32 5f7a9e5f) in place.

  * adds Nei Adventure as the 10th Additional Content entry (4 KiB page-deduplicated, uncompressed,
    pages appended to the unused ROM tail; pages identical to ones already in the ROM are shared),
  * replaces the Genesis runtime blob (new 68000 core, renderer, debug menu) at 0x086e2000,
  * replaces the Additional Content menu blob (10 entries, START = debug launch on Genesis entries).

  * replaces the PSC8 engine (SMS/GG/YM2413) at 0x086ca500 with the rewritten one in source/psc8
    (assembly Z80 core, block audio, rhythm section); built by source/psc8/build.py.

  * CP09: the Genesis runtime plays sound (the games' own Z80 driver + baked FM snapshot tables, additional/mdsnd.c).  The
    instrument bank (additional/gsnd/bake/bank_raw.bin, packed by mkbank.py) is spread over the free ROM gaps:
    index -> ROM tail (.rom2, with the AOT tables), table data -> PSC8 slack, PSII-debug slack, Genesis-runtime slack.

Everything else (PSI/II/III/IV payloads, PSI retranslation, SRAM layout, launcher hook, card art)
is byte-identical to CP07; `check_cp09.py` verifies that.

  python3 build_cp09.py --base ../binaries/RUN_THIS_R48_CP07_SAFE_HANDOFF.gba --nei "Nei Adventure.bin" --out CP08.gba
"""
from pathlib import Path
import argparse, json, os, re, struct, subprocess, sys, zlib

R = Path(__file__).resolve().parent
A = R / 'additional'
O = A / 'build'
O.mkdir(exist_ok=True)

ap = argparse.ArgumentParser()
ap.add_argument('--base', type=Path, required=True, help='CP07 safe ROM (CRC32 5f7a9e5f)')
ap.add_argument('--nei', type=Path, required=True, help='Nei Adventure.bin (256 KiB, CRC32 3d9ad465)')
ap.add_argument('--out', type=Path, required=True)
ap.add_argument('--pack-json', type=Path, default=R.parent / 'build_artifacts/ADDITIONAL_CONTENT_PACK.json')
ap.add_argument('--report', type=Path, default=R.parent / 'build_artifacts/CP09_BUILD_REPORT.json')
ap.add_argument('--opt', default='-Os')
ap.add_argument('--hot-limit', default='0x03007980')
ap.add_argument('--debug-info', action='store_true', help='add -g to the runtime objects (profiling)')
a = ap.parse_args()

CP07_CRC = 0x5f7a9e5f
NEI_CRC = 0x3d9ad465
base = a.base.resolve().read_bytes()
assert len(base) == 0x2000000 and zlib.crc32(base) == CP07_CRC, 'base must be the CP07 safe ROM'
rom = bytearray(base)
nei = a.nei.resolve().read_bytes()
assert len(nei) == 262144 and zlib.crc32(nei) == NEI_CRC, 'unexpected Nei Adventure.bin'

import gbabuild
from gbabuild import build, sym, HOT_FILES
CC, LD, OC = gbabuild.CC, gbabuild.LD, gbabuild.OC
F = gbabuild.flags(a.opt, a.debug_info)


# ---------------------------------------------------------------- cartridge pool / Nei
pack = json.loads(a.pack_json.read_text())
carts = {c['title']: c for c in pack['cartridges']}
assert len(carts) == 9

FREE_LO, FREE_HI = 0x1fd5650, 0x2000000
assert all(b == 0xff for b in base[FREE_LO:FREE_HI]), 'tail region is not erased in the base'

pool = {}                                              # page bytes -> ptr|1
for title, c in carts.items():
    if c['flags'] != 4:
        continue
    off = c['table'] - 0x8000000
    tbl = struct.unpack_from('<%dI' % (c['size'] // 4096), base, off)
    for p in tbl:
        assert p & 1
        o = (p & ~1) - 0x8000000
        pool.setdefault(bytes(base[o:o + 4096]), p)

cursor = FREE_LO
allocs = []


def alloc(data, label):
    global cursor
    start = (cursor + 3) & ~3
    assert start + len(data) <= FREE_HI, 'ROM tail exhausted: ' + label
    assert all(b == 0xff for b in rom[start:start + len(data)])
    rom[start:start + len(data)] = data
    cursor = start + len(data)
    allocs.append({'label': label, 'offset': start, 'bytes': len(data)})
    return 0x08000000 + start


nei_pages = []
new_pages = shared_pages = 0
for i in range(0, len(nei), 4096):
    raw = nei[i:i + 4096]
    if raw in pool:
        shared_pages += 1
    else:
        pool[raw] = alloc(raw, 'Nei page') | 1
        new_pages += 1
    nei_pages.append(pool[raw])
nei_table = alloc(struct.pack('<%dI' % len(nei_pages), *nei_pages), 'Nei Adventure page table')
assert nei_table & 3 == 0 and all(p & 1 for p in nei_pages) and all(((p & ~1) & 1) == 0 for p in nei_pages)

# ---------------------------------------------------------------- catalog.h
entries = [dict(title=t, table=c['table'], size=c['size'], flags=c['flags']) for t, c in carts.items()]
order = ['Phantasy Star Adventure', 'Phantasy Star Gaiden', 'Anne Adventure', 'Rudger Adventure', 'Kinds Adventure',
         'Shilka Adventure', 'Amia Adventure', 'Huey Adventure', 'Eusis Adventure']
entries = [next(e for e in entries if e['title'] == t) for t in order]
for e in entries:
    if e['title'] == 'Phantasy Star Gaiden':
        assert e['flags'] == 1
        e['flags'] = 1 | 0x10        # GG + persistent cartridge SRAM (first 4 KiB) at GBA SRAM 0x0E006000 (see psc8/psc8.c)
entries.append(dict(title='Nei Adventure', table=nei_table, size=len(nei), flags=4))
# menu background pointer is baked into the CP07 menu; read it back from the existing catalog.h
m = re.search(r'menu_background \(\(const unsigned char\*\)(\d+)u\)', (A / 'orig/catalog.h').read_text())
bgaddr = int(m.group(1))
(A / 'catalog.h').write_text(
    'typedef struct{const char*title;uint32_t table,size,flags;} Cart;\n#define CATALOG_COUNT %du\nstatic const Cart catalog[CATALOG_COUNT]={\n' % len(entries)
    + ''.join('{"%s",%du,%du,%du},\n' % (e['title'], e['table'], e['size'], e['flags']) for e in entries)
    + '};\n#define menu_background ((const unsigned char*)%du)\n' % bgaddr)

# ---------------------------------------------------------------- engines
def put(off, data, lo, hi, what):
    assert lo <= off and off + len(data) <= hi, (what, hex(off), len(data), hex(hi))
    rom[off:off + len(data)] = data


# Genesis runtime: replace old blob (17,620 B) + zero padding up to the PSII end-of-payload at 0x738000.
assert not any(base[0x6e64d4:0x738000]) and base[0x6e2000:0x6e2004] != b'\0\0\0\0'
# Statically recompiled 68000 code (additional/aot): region functions in the blob, lookup tables in the ROM tail (.rom2)
AOT_FILES = [A / 'aot/generated/aot_code.c', A / 'aot/generated/aot_tabs.c']
rom2_addr = 0x08000000 + ((cursor + 3) & ~3)

# Sound bank (additional/gsnd): one contiguous blob at BANK_BASE = [index: zones + table directory][table data].  It lives in the
# zero padding between the PSIV entry stub (ends 0x4766D) and the PSIII image (0x80000): 24 randomised 9,000-frame emulator runs
# of the collection, PSII, PSIII and PSIV never read that range (tools/rom_read_trace.py), and it separates two images
# by alignment only.  The runtime blob itself stays inside its slot (0x6e2000..0x730000).
BANK_RAW = A / 'gsnd/bake/bank_raw.bin'
BANK_C = A / 'gsnd/gsnd_bank.c'
BANK_BASE, BANK_LIMIT = 0x048000, 0x07c000
ZAOT_BASE = 0x060000          # translated Z80 sound driver (z80aot.py), after the sound tables, inside the same padding
MD_LO, MD_HI = 0x6e2000, 0x730000
PSC8_LO, PSC8_HI = 0x6ca500, 0x6e2000
PS2D_BASE, PS2D_END = 0x730000, 0x738000
assert not any(base[BANK_BASE:BANK_LIMIT]), 'sound bank area is not empty in the base'


def mkbank(table_base, tail_bytes):
    cmd = [sys.executable, '-I', str(A / 'gsnd/bake/mkbank.py'), str(BANK_RAW), str(BANK_C), '--tol', '16', '--sndidx',
           '--regions', '0x%x:%d' % (0x08000000 + table_base, tail_bytes)]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode:
        sys.exit('mkbank: ' + r.stdout + r.stderr)
    return r.stdout.strip().splitlines()[-1]


subprocess.run([sys.executable, str(R / 'psc8/build.py')], check=True, stdout=subprocess.DEVNULL)
psc8 = (R / 'psc8/build/psc8.bin').read_bytes()
psc8_hot_end = sym(R / 'psc8/build/psc8.elf', '__hot_end')
# ---------------------------------------------------------------- PSII boot-time debug module (hold L+R at launch)
PS2D = R / 'ps2dbg'
PS2_HOOK_SITE = 0x390414                                # PSII main: `bl 0x0839488C`, last init call before the main loop
assert base[PS2_HOOK_SITE:PS2_HOOK_SITE + 4] == bytes.fromhex('04f03afa'), 'unexpected PSII main'
assert not any(base[PS2D_BASE:PS2D_END]), 'PSII debug region is not empty in the base'
# title-screen warp hooks (statically translated PSII code; see ps2dbg/ps2warp.h).  Each BL replaces two Thumb instructions.
PS2_WARP_A = 0x39F2FC     # title START -> new-game intro: `movs r3,#0 ; ldr r0,=controls_locked`
PS2_WARP_B = 0x39F376     # end of MoveToOpeningScreen:    `ldr r3,=0x03004000 ; ldr r2,[r3,#0x3C]`
assert base[PS2_WARP_A:PS2_WARP_A + 4] == bytes.fromhex('0023' '0d48'), 'unexpected PSII title START code'
assert base[PS2_WARP_B:PS2_WARP_B + 4] == bytes.fromhex('0a4b' 'da6b'), 'unexpected PSII opening-screen code'


def build_ps2dbg():
    obj = []
    for f in (PS2D / 'thunk.s', PS2D / 'ps2dbg.c'):
        o = O / ('ps2dbg_' + f.name + '.o')
        flags = ['--target=arm-none-eabi', '-mcpu=arm7tdmi', '-mthumb', a.opt, '-ffreestanding', '-fno-builtin', '-fno-unwind-tables',
                 '-fno-stack-protector', '-I' + str(PS2D)]
        subprocess.run([CC, *flags, '-c', str(f), '-o', str(o)], check=True)
        obj.append(str(o))
    ld = O / 'ps2dbg.ld'
    ld.write_text(f'''ENTRY(ps2dbg_thunk)
SECTIONS {{ . = {hex(0x08000000 + PS2D_BASE)}; .rom : {{ *(.start) *(.text*) *(.rodata*) }}
 .bss 0x0203C040 (NOLOAD) : {{ __bss_start = .; *(.bss*) *(COMMON) __bss_end = ALIGN(4); }}
 /DISCARD/ : {{ *(.ARM.exidx*) *(.ARM.extab*) *(.comment*) *(.eh_frame*) *(.data*) }}
 ASSERT(__bss_end < 0x0203F000, "PS2 debug bss overflow") }}''')
    elf = O / 'ps2dbg.elf'
    subprocess.run([LD, '-T', str(ld), '-Map=' + str(O / 'ps2dbg.map'), *obj, '-o', str(elf)], check=True)
    out = O / 'ps2dbg.bin'
    subprocess.run([OC, '-O', 'binary', '--only-section=.rom', str(elf), str(out)], check=True)
    return out.read_bytes(), elf



ps2d, ps2d_elf = build_ps2dbg()
mkbank(BANK_BASE + 0x8000, BANK_LIMIT - BANK_BASE - 0x8000)               # provisional table base: learn the index size
idx_bytes = (A / 'gsnd/gsnd_bank_idx_size.txt').read_text().split()
IDX_SIZE = int(idx_bytes[0])
TAB_BASE = (BANK_BASE + IDX_SIZE + 15) & ~15
bank_info = mkbank(TAB_BASE, ZAOT_BASE - TAB_BASE)
assert int((A / 'gsnd/gsnd_bank_idx_size.txt').read_text().split()[0]) == IDX_SIZE
AOT_EXTRA_BASE = (PSC8_LO + len(psc8) + 3) & ~3
md, md_elf = gbabuild.build_md(a.hot_limit, a.opt, a.debug_info, aot=AOT_FILES, rom2=rom2_addr, sndidx=0x08000000 + BANK_BASE, zaot=0x08000000 + ZAOT_BASE, aotextra=0x08000000 + AOT_EXTRA_BASE)
rom[MD_LO:PS2D_END] = bytes(PS2D_END - MD_LO)
put(MD_LO, md, MD_LO, MD_HI, 'md runtime')
md_rom2 = (gbabuild.O / 'md_rom2.bin').read_bytes()
assert alloc(md_rom2, 'MD AOT tables') == rom2_addr
sndidx = (gbabuild.O / 'md_sndidx.bin').read_bytes()
assert len(sndidx) == IDX_SIZE, (len(sndidx), IDX_SIZE)
tabs = (A / 'gsnd/gsnd_bank_0.bin').read_bytes()
put(BANK_BASE, sndidx, BANK_BASE, TAB_BASE, 'sound index')
put(TAB_BASE, tabs, TAB_BASE, ZAOT_BASE, 'sound tables')
bank_end = TAB_BASE + len(tabs)
zaot = (gbabuild.O / 'md_zaot.bin').read_bytes()
put(ZAOT_BASE, zaot, ZAOT_BASE, 0x078000, 'translated Z80 sound driver')
zaot_end = ZAOT_BASE + len(zaot)
# Native ROM extension in verified unused PSIII panel padding.
aotmore=(gbabuild.O/'md_aotmore.bin').read_bytes()
assert not any(base[0x079000:0x07c000]),'native extension padding is occupied'
put(0x079000,aotmore,0x079000,0x07c000,'native 68000 extension')

# Menu: old blob 0x5000..0x5858, free (0xFF) up to the trampoline at 0x7f00.
assert all(b == 0xff for b in base[0x5858:0x7f00])
menu, menu_elf = gbabuild.build_menu(a.opt)
rom[0x5000:0x7f00] = b'\xff' * (0x7f00 - 0x5000)
put(0x5000, menu, 0x5000, 0x7f00, 'menu')

rom[PSC8_LO:PSC8_HI] = bytes(PSC8_HI - PSC8_LO)
put(PSC8_LO, psc8, PSC8_LO, PSC8_HI, 'PSC8 engine')
aotextra = (gbabuild.O / 'md_aotextra.bin').read_bytes()
put(AOT_EXTRA_BASE, aotextra, AOT_EXTRA_BASE, PSC8_HI, 'additional native 68000 code')

put(PS2D_BASE, ps2d, PS2D_BASE, PS2D_END, 'PSII debug module')
thunk = sym(ps2d_elf, 'ps2dbg_thunk') & ~1
off = thunk - (0x08000000 + PS2_HOOK_SITE + 4)
assert -(1 << 22) <= off < (1 << 22) and not off & 1


def thumb_bl(site, target):
    off = target - (0x08000000 + site + 4)
    assert -(1 << 22) <= off < (1 << 22) and not off & 1
    return struct.pack('<HH', 0xF000 | ((off >> 12) & 0x7FF), 0xF800 | ((off >> 1) & 0x7FF))


rom[PS2_HOOK_SITE:PS2_HOOK_SITE + 4] = thumb_bl(PS2_HOOK_SITE, thunk)
tha, thb = sym(ps2d_elf, 'ps2warp_thunk_a') & ~1, sym(ps2d_elf, 'ps2warp_thunk_b') & ~1
rom[PS2_WARP_A:PS2_WARP_A + 4] = thumb_bl(PS2_WARP_A, tha)
rom[PS2_WARP_B:PS2_WARP_B + 4] = thumb_bl(PS2_WARP_B, thb)

out = a.out.resolve()
out.parent.mkdir(parents=True, exist_ok=True)
out.write_bytes(rom)
crc = zlib.crc32(rom)
hot_end = sym(md_elf, '__hot_end')
report = {
    'output': out.name, 'CRC32': f'{crc:08x}', 'base_CRC32': f'{CP07_CRC:08x}', 'size': len(rom),
    'nei': {'crc32': f'{NEI_CRC:08x}', 'pages_total': len(nei_pages), 'pages_new': new_pages, 'pages_shared_with_existing': shared_pages,
            'table': hex(nei_table), 'flags': 4},
    'catalog': entries,
    'ps2_debug': {'bytes': len(ps2d), 'base': hex(0x08000000 + PS2D_BASE), 'hook_site': hex(0x08000000 + PS2_HOOK_SITE), 'thunk': hex(thunk),
                  'warp_hooks': {'title_start': hex(0x08000000 + PS2_WARP_A), 'opening_end': hex(0x08000000 + PS2_WARP_B)}},
    'psc8': {'base': hex(0x08000000 + PSC8_LO), 'bytes': len(psc8), 'slack_bytes': PSC8_HI - PSC8_LO - len(psc8),
             'crc32': f'{zlib.crc32(psc8):08x}', 'iwram_end': hex(psc8_hot_end)},
    'engines': {'md_bytes': len(md), 'menu_bytes': len(menu), 'md_iwram_end': hex(hot_end), 'md_iwram_bytes': hot_end - 0x03000000},
    'sound_bank': {'summary': bank_info, 'base': hex(0x08000000 + BANK_BASE), 'index_bytes': IDX_SIZE, 'tables_bytes': len(tabs), 'end': hex(0x08000000 + bank_end),
                   'padding_free_after': ZAOT_BASE - bank_end},
    'z80_driver_code': {'base': hex(0x08000000 + ZAOT_BASE), 'bytes': len(zaot), 'end': hex(0x08000000 + zaot_end), 'padding_free_after': BANK_LIMIT - zaot_end},
    'aot_more': {'base':'0x08079000','bytes':len(aotmore),'end':hex(0x08079000+len(aotmore))},
    'aot_extra': {'base': hex(0x08000000 + AOT_EXTRA_BASE), 'bytes': len(aotextra), 'end': hex(0x08000000 + AOT_EXTRA_BASE + len(aotextra)), 'padding_free_after': PSC8_HI - AOT_EXTRA_BASE - len(aotextra)},
    'tail_free_after_bytes': FREE_HI - cursor,
    'allocations': allocs,
}
a.report.write_text(json.dumps(report, indent=1) + '\n')
print(json.dumps({k: v for k, v in report.items() if k not in ('allocations', 'catalog')}, indent=1))
