#!/usr/bin/env python3
"""ROM free-space map for the CP08 layout: trailing filler after each engine blob / at the ROM tail (known free), plus
large filler runs inside payloads (not free).

  python3 rom_space.py ROM.gba [--big 16384] [--json out.json]
"""
import argparse, json, zlib

# (start, end, owner, known_free_if_filler)
LAYOUT = [
    (0x0000000, 0x0005000, 'launcher / collection', False),
    (0x0005000, 0x0007F00, 'Additional Content menu blob (free after the blob)', True),
    (0x0007F00, 0x0041800, 'launcher trampoline / card art / PSC area', False),
    (0x0041800, 0x0080000, 'collection slot IV entry area', False),
    (0x0080000, 0x02D3B80, 'PSIII native module', False),
    (0x02D3B80, 0x0353B80, 'main-slot PSI v2.6.1 SMS image (512 KiB, game data incl. its own padding)', False),
    (0x0353B80, 0x0390000, 'between PSI image and PSII (not a known-free region)', False),
    (0x0390000, 0x06CA500, 'PSII native module', False),
    (0x06CA500, 0x06E2000, 'PSC8 engine (free after the blob)', True),
    (0x06E2000, 0x0730000, 'Genesis (MD) runtime (free after the blob)', True),
    (0x0730000, 0x0738000, 'PSII debug module (free after the blob)', True),
    (0x0738000, 0x0800000, 'PSI-era tail area (re-audit before use)', False),
    (0x0800000, 0x1FD5650, 'PSIV module + Additional Content cartridges', False),
    (0x1FD5650, 0x2000000, 'ROM tail (Nei pages appended here; rest erased)', True),
]


def owner(off):
    for s, e, name, free in LAYOUT:
        if s <= off < e:
            return name, free, e
    return '?', False, 0x2000000


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('rom')
    ap.add_argument('--json')
    ap.add_argument('--big', type=int, default=16384, help='also list filler runs inside payloads of at least this size')
    a = ap.parse_args()
    rom = open(a.rom, 'rb').read()
    print(f'ROM {a.rom}  CRC32 {zlib.crc32(rom):08x}  size {len(rom)}')
    free = []
    for s0, e0, name, ok in LAYOUT:
        if not ok:
            continue
        b = rom[e0 - 1]
        if b not in (0, 0xFF):
            continue
        j = e0
        while j > s0 and rom[j - 1] == b:
            j -= 1
        if e0 - j:
            free.append({'start': j, 'end': e0, 'bytes': e0 - j, 'fill': f'{b:02X}', 'region': name})
    print('KNOWN FREE (trailing filler after a blob, up to the next owner):')
    for r in free:
        print(f'  {r["start"]:#010x}..{r["end"]:#010x} (cart {0x8000000 + r["start"]:#010x})  {r["bytes"]:>8} B  fill {r["fill"]}  {r["region"]}')
    tot = sum(r['bytes'] for r in free)
    print(f'  total {tot} B = {tot / 1024:.1f} KiB')
    big = []
    i, n = 0, len(rom)
    while i < n:
        b = rom[i]
        if b in (0, 0xFF):
            j = i
            while j < n and rom[j] == b:
                j += 1
            nm, ok, _ = owner(i)
            if j - i >= a.big and not any(r['start'] <= i < r['end'] for r in free):
                big.append({'start': i, 'end': j, 'bytes': j - i, 'fill': f'{b:02X}', 'region': nm})
            i = j
        else:
            i += 1
    print(f'FILLER INSIDE PAYLOADS >= {a.big} B (NOT free: data the owning game may rely on; audit before use):')
    for r in big:
        print(f'  {r["start"]:#010x}..{r["end"]:#010x}  {r["bytes"]:>8} B  fill {r["fill"]}  {r["region"]}')
    if a.json:
        json.dump({'crc32': f'{zlib.crc32(rom):08x}', 'known_free': free, 'known_free_bytes': tot, 'filler_inside_payloads': big}, open(a.json, 'w'), indent=1)


if __name__ == '__main__':
    main()
