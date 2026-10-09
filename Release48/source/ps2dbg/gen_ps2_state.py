#!/usr/bin/env python3
"""Generate ps2state.h: Phantasy Star II (Mega Drive) party RAM images for the PSC debug warp.

Everything is derived from the PSII disassembly build (ps2built.bin + ps2.lst), the same data the
game uses itself: CharInitialSetup, character experience/stat tables, InventoryData, CharNames.
`--check DUMP` compares the level-1 "new game" image this script produces against 68k RAM dumped
from the running PSC PSII (character_data_buffer $C000..$C1FF and names at $C660).

Usage: gen_ps2_state.py <ps2disassm dir> <out ps2state.h> [--check <emulator ram dump json>]
"""
import sys, re, json, struct, pathlib

D = pathlib.Path(sys.argv[1])
BIN = (D / 'ps2built.bin').read_bytes()
LST = (D / 'ps2.lst').read_text(errors='replace')

def label(name):
    m = re.search(r'^\s*\d+/\s*([0-9A-F]+) :\s+%s:' % re.escape(name), LST, re.M)
    assert m, name
    return int(m.group(1), 16)

CHARNAMES = label('CharNames'); INIT = label('CharInitialSetup'); INV = label('InventoryData')
EXPPTR = label('CharExperiencePtrs') if re.search(r'CharExperiencePtrs:', LST) else 0x1619E
AGI = label('AgilityIncreasesArray')
def u16(a): return struct.unpack_from('>H', BIN, a)[0]
def u32(a): return struct.unpack_from('>I', BIN, a)[0]

NCHAR = 8
CH = ['ROLF', 'NEI', 'RUDO', 'AMY', 'HUGH', 'ANNA', 'KAIN', 'SHIR']
# --- items -----------------------------------------------------------------------------------
def item(i):
    e = BIN[INV + i * 16: INV + i * 16 + 16]
    return dict(id=i, name=e[:10], price=struct.unpack('>H', e[10:12])[0], typ=e[12], mask=e[13], atk=e[14], dfn=e[15])
def dec(b):
    s = ''
    for c in b:
        if c == 0: s += ' '
        elif 11 <= c <= 36: s += chr(65 + c - 11)
        elif 37 <= c <= 62: s += chr(97 + c - 37)
        elif 1 <= c <= 10: s += str(c - 1)
        elif c == 0xC4: break
    return s.strip()
ITEMS = [item(i) for i in range(0x80)]
ID = {dec(it['name']): it['id'] for it in ITEMS}
SLOTS = dict(head=range(0x17, 0x2B), body=range(0x2B, 0x3F), legs=range(0x3F, 0x47), left=range(0x47, 0x56), right=range(0x56, 0x7B))
def cands(slot, ch):
    return [ITEMS[i] for i in SLOTS[slot] if ITEMS[i]['mask'] & (1 << ch) and not dec(ITEMS[i]['name']).startswith('NEI')]
def best(slot, ch, key):
    c = cands(slot, ch)
    return max(c, key=lambda x: (x[key], x['price']))['id'] if c else 0
MAGIC_CAP = ID['MAGIC CAP']
CONSUM = [ID['TRIMATE'], ID['STAR MIST'], ID['MOON DEW']]

# --- characters --------------------------------------------------------------------------------
def exp_table(ch):
    return u32(EXPPTR + 4 * ch)
def stats_at(ch, level):
    t = exp_table(ch)
    row0 = BIN[t + 4: t + 14]
    st = list(row0)                       # HP TP STR MEN AGI LUC DEX ATK DEF TECH
    tech_hi, tech_lo = st[9] >> 4, st[9] & 15
    for lv in range(1, level):            # row[lv-1+1]: gains for reaching level lv+1 (average roll = 1.0x)
        r = BIN[t + 14 * lv + 4: t + 14 * lv + 14]
        for k in range(9): st[k] += r[k]
        tech_hi += r[9] >> 4; tech_lo += r[9] & 15
    exp = u32(t + 14 * (level - 1)) & 0xFFFFFF if level > 1 else 0
    return st[:9], tech_hi, tech_lo, exp

def char_block(ch, level, gear):
    b = bytearray(64)
    st, th, tl, exp = stats_at(ch, level)
    hp, tp, strn, men, agi, luc, dex, atk, dfn = st
    def w(o, v): struct.pack_into('>H', b, o, v & 0xFFFF)
    w(2, hp); w(4, hp); w(6, tp); w(8, tp); w(0xA, level); struct.pack_into('>I', b, 0xC, exp)
    w(0x10, strn); w(0x12, men); w(0x14, agi); w(0x16, luc); w(0x18, dex); w(0x1A, atk); w(0x1E, dfn)
    b[0x25], b[0x26] = th, tl
    return b, gear

def build(level, party_gear):
    """party_gear: {ch: (head,right,left,body,legs,[consumables])}; returns 512-byte character buffer."""
    buf = bytearray(512)
    for ch in range(NCHAR):
        gear = party_gear[ch]
        b, _ = char_block(ch, level, gear)
        eq = list(gear[:5]); b[0x20:0x25] = bytes(eq)
        items = []
        for x in eq:
            if x and (0x80 | x) not in items: items.append(0x80 | x)
        items += list(gear[5])
        b[0x27] = len(items); b[0x28:0x28 + len(items)] = bytes(items)
        atk_total = struct.unpack_from('>H', b, 0x1A)[0]; def_total = struct.unpack_from('>H', b, 0x1E)[0]; agi = struct.unpack_from('>H', b, 0x14)[0]
        for x in eq:
            if x: atk_total += ITEMS[x]['atk']; def_total += ITEMS[x]['dfn']
        legs = eq[4]
        if ID['SHOES'] <= legs <= ID['GARDA BOOTS'] if 'GARDA BOOTS' in ID else False: pass
        sh = legs - ID['SHOES']
        if 0 <= sh <= 8 - 0: agi += BIN[AGI + sh] if sh < 8 else 0
        struct.pack_into('>H', b, 0x1C, atk_total); struct.pack_into('>H', b, 0x1E, def_total); struct.pack_into('>H', b, 0x14, agi)
        buf[ch * 64: ch * 64 + 64] = b
    return buf

def newgame_gear():
    """The game's own CharInitialSetup entries (16 bytes per character)."""
    g = {}
    for ch in range(NCHAR):
        e = BIN[INIT + 16 * ch: INIT + 16 * ch + 16]
        n = e[7]
        g[ch] = (e[0], e[1], e[2], e[3], e[4], [x for x in e[8:8 + n] if not x & 0x80])
    return g

def party_gear(party_ids):
    g = {}
    for ch in range(NCHAR):
        head = best('head', ch, 'dfn'); body = best('body', ch, 'dfn'); legs = best('legs', ch, 'dfn')
        right = best('right', ch, 'atk')
        if ITEMS[right]['typ'] & 7 == 3 if right else False:
            left = right                                    # two-handed weapon: the game fills both hand slots
        else:
            left = best('left', ch, 'dfn')
        if ch == 0: head = MAGIC_CAP                        # Dezolis: Rolf must wear the Magic Cap (cold-zone check)
        cons = list(CONSUM)
        if ch == 0: cons = [ID['TELEPIPE']] + cons[:2]
        g[ch] = (head, right, left, body, legs, cons)
    return g

def names():
    return bytes(BIN[CHARNAMES: CHARNAMES + 8 * 4])

def check(dump):
    d = json.load(open(dump)); ram = bytes.fromhex(d['c000'])
    ng = build(1, {c: (g[0], g[1], g[2], g[3], g[4], g[5]) for c, g in newgame_gear().items()})
    # new game initialises equipment slots + 'GOLD' items from CharInitialSetup; compare stat area only
    bad = [(i, ng[i], ram[i]) for i in range(512) if ng[i] != ram[i] and (i & 63) not in range(0x38, 0x40)]  # +38..3F: intro text scratch
    print('new-game check: %d/512 bytes differ' % len(bad)); [print(' +%03X gen=%02X ram=%02X' % x) for x in bad[:40]]
    nm = bytes.fromhex(d['names']); print('names match:', nm == names(), nm.hex(), names().hex())
    return not bad and nm == names()

PARTY = [0, 2, 3, 4]               # Rolf, Rudo, Amy, Hugh (PSII's battle screen holds four)
LEVEL = 32

def emit(out):
    chars = build(LEVEL, party_gear(PARTY))
    lines = ['/* generated by gen_ps2_state.py from the PSII disassembly tables; do not edit */',
             '#define PS2ST_LEVEL %d' % LEVEL,
             'static const u8 ps2st_chars[512]={%s};' % ','.join('0x%02X' % x for x in chars),
             'static const u8 ps2st_names[32]={%s};' % ','.join('0x%02X' % x for x in names()),
             '#define PS2ST_PARTY_N %d' % len(PARTY),
             'static const u16 ps2st_party[PS2ST_PARTY_N]={%s};' % ','.join(str(x) for x in PARTY)]
    pathlib.Path(out).write_text('\n'.join(lines) + '\n')
    for ch in PARTY:
        b = chars[ch * 64: ch * 64 + 64]
        print(CH[ch], 'Lv', struct.unpack_from('>H', b, 0xA)[0], 'HP', struct.unpack_from('>H', b, 2)[0], 'TP', struct.unpack_from('>H', b, 6)[0],
              'ATK', struct.unpack_from('>H', b, 0x1C)[0], 'DEF', struct.unpack_from('>H', b, 0x1E)[0],
              [dec(ITEMS[x]['name']) for x in b[0x20:0x25]], 'items', [hex(x) for x in b[0x28:0x28 + b[0x27]]])

if __name__ == '__main__':
    if '--check' in sys.argv:
        sys.exit(0 if check(sys.argv[sys.argv.index('--check') + 1]) else 1)
    emit(sys.argv[2])
