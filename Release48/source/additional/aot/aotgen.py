#!/usr/bin/env python3
"""Builds the statically recompiled code for all SEGANET adventures.

Inputs: the eight cartridge images and executed-instruction coverage files (tools_host/md_host `mdhost_ref cover`).
Selection: per game, the hottest instructions covering --share of all executed instructions in ROM (0x0-0x3FFFF)
and in the RAM code window (0xFF0000-0xFF7FFF, a verbatim copy of cartridge 0x20000-0x27FFF made by each game at
boot); everything else stays with the interpreter.  Identical regions (same C text) are emitted once and shared
between games (the engine code in 0x0-0x1FFFF is common to all of them).
Outputs: aot_code.c (region functions), aot_tabs.c (per-game entry tables, bitmaps, RAM-region checks, games list).

  python3 aotgen.py --roms DIR --cover DIR --out DIR [--share 0.99]
"""
import argparse, collections, glob, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from native_spans import fuse
from aot68k import Tx, decode_all, regions_of, emit_region

GAMES = [('anne', 2, 'Anne'), ('rudger', 3, 'Rudger'), ('kinds', 4, 'Kinds'), ('shilka', 5, 'Shilka'),
         ('amia', 6, 'Amia'), ('huey', 7, 'Huey'), ('eusis', 8, 'Eusis'), ('nei', 9, 'Nei')]
RAMLO, RAMHI, RAMSRC = 0xFF0000, 0xFF8000, 0x20000


IDLE = (0x3400, 0x3500)          # the engine's wait-for-vblank page: always translated, excluded from the share


def hot(counts, share, minc):
    sel = set(p for p in counts if IDLE[0] <= p < IDLE[1])
    counts = {p: c for p, c in counts.items() if not (IDLE[0] <= p < IDLE[1])}
    v = sorted(counts.items(), key=lambda kv: -kv[1]); tot = sum(c for _, c in v); acc = 0
    for a, c in v:
        if acc >= share * tot and c < minc: break
        sel.add(a); acc += c
    return sel


def load_counts(cover, stem):
    """per-PC (all executions, executions in busy frames)"""
    tot = collections.Counter(); busy = collections.Counter()
    for f in glob.glob(os.path.join(cover, 'cov_%s*.txt' % stem)):
        for l in open(f):
            p = l.split(); a = int(p[0], 16); tot[a] += int(p[1])
            if len(p) > 2: busy[a] += int(p[2])
    return tot, busy


def select(cnts, lo, hi, share, bshare, minc):
    tot, busy = cnts
    t = {p: n for p, n in tot.items() if lo <= p < hi}
    b = {p: n for p, n in busy.items() if lo <= p < hi and n}
    return hot(t, share, minc) | (hot(b, bshare, minc) if b else set())


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--roms', required=True); ap.add_argument('--cover', required=True); ap.add_argument('--out', required=True)
    ap.add_argument('--share', type=float, default=0.99); ap.add_argument('--ramshare', type=float, default=0.99)
    ap.add_argument('--minc', type=int, default=1 << 30); ap.add_argument('--busyshare', type=float, default=0.995)
    ap.add_argument('--allow-raw-regeneration', action='store_true', help='explicitly discard any existing hand-fused native VRAM AOT transfer')
    a = ap.parse_args()
    existing = os.path.join(a.out, 'aot_code.c')
    if os.path.exists(existing) and 'vram_moves:' in open(existing).read() and not a.allow_raw_regeneration:
        ap.error('Refusing to overwrite the accepted, PSIV-style fused VRAM AOT source. '
                 'Generate into a separate directory and merge validated changes, or explicitly '
                 'supply --allow-raw-regeneration to discard the native fusion.')
    funcs = {}; order = []
    def fn_of(text):
        if text not in funcs: funcs[text] = len(order); order.append(text)
        return funcs[text]
    roms = {stem: open(os.path.join(a.roms, '%s_Adventure_-_English.bin' % stem), 'rb').read() for _, _, stem in GAMES}
    counts = {stem: load_counts(a.cover, stem) for _, _, stem in GAMES}
    stats = []
    # ---- ROM code: one translation for the games whose cartridge code is identical to Anne's, one per other game
    base = roms['Anne']
    def rom_group(stem):
        return 'common' if stem != 'Nei' else 'nei'
    romtabs = {}; xtabs = {}
    def build_ents(rom, sel):
        insns = decode_all(Tx(rom, 0), sel)
        ents = []
        for members in regions_of(insns):
            text, entries = emit_region(insns, members, '@F@')
            f = fn_of(text)
            ents += [(pc, f, i, 0xFFFF) for pc, i in entries]
        return sorted(ents), len(insns)
    for grp in ('common', 'nei'):
        stems = [st for _, _, st in GAMES if rom_group(st) == grp]
        rom = roms[stems[0]] if grp == 'nei' else base
        sel = set(); extra = {st: set() for st in stems}
        txb = Tx(rom, 0)
        for st in stems:
            for p in select(counts[st], 0, 0x40000, a.share, a.busyshare, a.minc):
                if grp == 'common':
                    L = txb.insn(p).length          # an instruction is shared if its own bytes are identical in every game
                    if any(roms[x][p:p + L] != rom[p:p + L] for x in stems): extra[st].add(p); continue
                sel.add(p)
        romtabs[grp], n = build_ents(rom, sel)
        stats.append('rom %s: %d instructions' % (grp, n))
        if grp == 'common':
            for st in stems:
                xtabs[st], n = build_ents(roms[st], extra[st])
                stats.append('rom extra %s: %d instructions' % (st, n))
    # ---- RAM code window, per game
    games = []
    for key, index, stem in GAMES:
        rom = roms[stem]
        sel = select(counts[stem], RAMLO, RAMHI, a.ramshare, a.busyshare, a.minc)
        insns = decode_all(Tx(rom[RAMSRC:RAMSRC + (RAMHI - RAMLO)], RAMLO), sel)
        ents = []; ramregs = []
        for members in regions_of(insns):
            text, entries = emit_region(insns, members, '@F@')
            f = fn_of(text)
            rngs = []
            for m in members:
                s0, e0 = m - RAMLO, m - RAMLO + insns[m].length
                if rngs and rngs[-1][1] >= s0: rngs[-1][1] = max(rngs[-1][1], e0)
                else: rngs.append([s0, e0])
            rr = len(ramregs); ramregs.append(rngs)
            ents += [(pc, f, i, rr) for pc, i in entries]
        games.append((key, index, rom_group(stem), sorted(ents), ramregs))
        stats.append('ram %s: %d instructions, %d regions' % (key, len(insns), len(ramregs)))
    # ---- output
    os.makedirs(a.out, exist_ok=True)
    with open(os.path.join(a.out, 'aot_code.c'), 'w') as f:
        f.write('/* generated by aotgen.py - do not edit */\n#include "aotrt.h"\n')
        # CP12: measured common VDP/idle routines and Anne's hot RAM routine.
        # Guard the profiled IDs against changes to coverage/order.
        # Assign the IWRAM fast-section by stable guest PC, not generated
        # function number: expanding the coverage changes deduplication order.
        # Previously the fixed index (12) caused an assertion as soon as
        # additional hot 68000 instructions were selected.
        hot_pcs=(0x31B8, 0x3440)
        hot_hits={pc:0 for pc in hot_pcs}
        for i, t in enumerate(order):
            matched=[pc for pc in hot_pcs if ('xpc[] AOT_TAB={0x%Xu' % pc) in t]
            if matched:
                t='AOT_HOT '+t
                for pc in matched: hot_hits[pc]+=1
            # Keep the CP23 exact reciprocal at the source-template level:
            # regenerating AOT must not silently reinstate software division.
            t=t.replace('(rem/18u)*18u','aot_div18_arm(rem)*18u')
            t,_=fuse(t)
            f.write(t.replace('@F@', 'aotf%d' % i) + '\n')
        assert all(hot_hits.values()) and all(n==1 for n in hot_hits.values()), 'hot native transfer/idle routine missing or duplicated'
    pool = []; pidx = {}
    def bitpages(ents, lo, npages):
        pages = [0xFFFF] * npages
        words = collections.defaultdict(lambda: [0] * 64)
        for e in ents:
            o = e[0] - lo; words[o >> 12][(o & 0xFFF) >> 6] |= 1 << ((o >> 1) & 31)
        for pg, w in words.items():
            t = tuple(w)
            if t not in pidx: pidx[t] = len(pool); pool.append(t)
            pages[pg] = pidx[t]
        return pages
    with open(os.path.join(a.out, 'aot_tabs.c'), 'w') as f:
        f.write('/* generated by aotgen.py - do not edit */\n#include "aotrt.h"\n')
        for i in range(len(order)): f.write('u32 aotf%d(u32);\n' % i)
        f.write('const aot_fn AOT_TAB aot_fns[]={%s};\n' % ','.join('aotf%d' % i for i in range(len(order))))
        def tab(name, ents):
            f.write('static const AotEnt AOT_TAB %s[]={\n%s};\n' % (name, ''.join('{0x%06xu,%d,%d,%d},\n' % e for e in ents) or '{0,0,0,0}'))
        for grp, ents in romtabs.items(): tab('rom_%s' % grp, ents)
        gl = []
        for key, index, grp, ents, ramregs in games:
            tab('ram_%s' % key, ents)
            xe = {k.lower(): v for k, v in xtabs.items()}.get(key, [])
            tab('xrom_%s' % key, xe)
            f.write('static const u16 AOT_TAB pg_%s[64]={%s};\n' % (key, ','.join(map(str, bitpages(romtabs[grp] + xe, 0, 64)))))
            rpages = bitpages(ents, RAMLO, 8)
            flat = []; rr = []
            for rngs in ramregs:
                rr.append((len(flat) // 2, len(rngs)))
                for s0, e0 in rngs: flat += [s0, e0 - s0]
            f.write('static const u16 AOT_TAB rng_%s[]={%s};\n' % (key, ','.join(map(str, flat)) or '0'))
            f.write('static const AotRamReg AOT_TAB rr_%s[]={%s};\n' % (key, ','.join('{%d,%d}' % x for x in rr) or '{0,0}'))
            f.write('static const u16 AOT_TAB rpg_%s[8]={%s};\n' % (key, ','.join(map(str, rpages))))
            gl.append('{%d,rom_%s,%d,ram_%s,%d,pg_%s,rpg_%s,rr_%s,%d,rng_%s,xrom_%s,%d}' % (index, grp, len(romtabs[grp]), key, len(ents), key, key, key, len(ramregs), key, key, len(xe)))
        f.write('const u32 AOT_TAB aot_pool[][64]={%s};\n' % ','.join('{%s}' % ','.join('0x%xu' % w for w in t) for t in pool))
        f.write('const AotGame aot_games[]={%s};\nconst u32 aot_ngames=%d;\n' % (','.join(gl), len(gl)))
    print('\n'.join(stats)); print('functions', len(order), 'bitmap pages', len(pool))


if __name__ == '__main__':
    main()
