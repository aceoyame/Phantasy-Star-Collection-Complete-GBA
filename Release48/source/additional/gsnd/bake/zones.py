#!/usr/bin/env python3
"""Collect the FM sub-voices (and the octave blocks they are played in) of the eight Genesis adventures.

  zones.py OUT.txt TRACE.log [TRACE.log ...]

The traces are YM2612 register-write logs of the games' real sound driver (`F<frame> T<tstate> YM<part> <reg> <val>`).
At every key-on the channel's patch is split into its sub-voices (a carrier together with the modulators that feed it,
see SUBV in gbake.cpp).  A sub-voice that needs a baked table (several operators, or a feedback carrier) is identified by
the bytes that decide its timbre: the algorithm (+ feedback when operator 1 takes part) and, for each operator of the
sub-voice, DT/MUL - and, for the modulators, TL, KS/AR, DR, SR, SL/RR as well.  The carrier's own level and envelope
are not part of it (the runtime applies the real carrier envelope), so patches that differ only there share one table.

Output: one line per (sub-voice signature, octave block):
  HASH ALG FB BLOCK FNUM  <24 operator bytes>  FMAXFNUM  COUNT  SUB
HASH is FNV-1a of the signature bytes (the runtime computes the same), FNUM the median F-number, FMAXFNUM the highest
one played (the table must be band-limited for it), COUNT the number of key-ons seen, SUB the sub-voice number.
"""
import re, sys, collections, statistics

# sub-voices per algorithm: (carrier operator, mask of the operators that take part)
SUBV = {0: [(3, 15)], 1: [(3, 15)], 2: [(3, 15)], 3: [(3, 15)], 4: [(1, 3), (3, 12)], 5: [(1, 3), (2, 5), (3, 9)],
        6: [(1, 3), (2, 4), (3, 8)], 7: [(0, 1), (1, 2), (2, 4), (3, 8)]}
SLOT2OP = [0, 2, 1, 3]            # register slot order (S1,S3,S2,S4) -> operator index (S1,S2,S3,S4)
rx = re.compile(r'^F(\d+) T(\d+) YM(\d) ([0-9a-f]{2}) ([0-9a-f]{2})')


def fnv1a(bs):
    h = 0x811c9dc5
    for b in bs:
        h = ((h ^ b) * 0x01000193) & 0xffffffff
    return h


def sub_sig(alg, fb, ops, vi):
    """ops[i] = [dtmul, tl, ksar, dr, sr, slrr]; the signature bytes of sub-voice vi (the runtime builds the same)."""
    car, mask = SUBV[alg][vi]
    out = [alg | ((fb if (mask & 1) else 0) << 3), vi]
    for i in range(4):
        if not (mask >> i) & 1:
            continue
        o = ops[i]
        if i == car:
            out += [o[0]]
        else:
            out += [o[0], o[1] & 0x7f, o[2], o[3] & 0x7f, o[4], o[5]]
    return bytes(out)


def table_voice(alg, fb, vi):
    car, mask = SUBV[alg][vi]
    return (mask & (mask - 1)) != 0 or (car == 0 and fb != 0)


def scan(path, zones):
    reg = [bytearray(256), bytearray(256)]
    keyed = [0] * 6
    for line in open(path):
        m = rx.match(line)
        if not m:
            continue
        part, r, v = int(m.group(3)), int(m.group(4), 16), int(m.group(5), 16)
        if part == 0 and r == 0x28:
            ch = v & 7
            if (ch & 3) == 3:
                continue
            n = (ch & 3) + (3 if ch & 4 else 0)
            ops = v >> 4
            if ops and not keyed[n]:
                p, c = (0, n) if n < 3 else (1, n - 3)
                rg = reg[p]
                alg = rg[0xB0 + c] & 7
                fb = (rg[0xB0 + c] >> 3) & 7
                opb = []
                for i in range(4):
                    off = c + 4 * SLOT2OP.index(i)
                    opb.append([rg[0x30 + off], rg[0x40 + off], rg[0x50 + off], rg[0x60 + off], rg[0x70 + off], rg[0x80 + off]])
                bf = ((rg[0xA4 + c] & 0x3f) << 8) | rg[0xA0 + c]
                for vi in range(len(SUBV[alg])):
                    if not table_voice(alg, fb, vi):
                        continue
                    sb = sub_sig(alg, fb, opb, vi)
                    z = zones[(fnv1a(sb), (bf >> 11) & 7)]
                    z['fn'].append(bf & 0x7ff)
                    if 'rep' not in z:
                        z['rep'] = (alg, fb, vi, [b for o in opb for b in o])
            keyed[n] = 1 if ops else 0
            continue
        if r >= 0x30:
            reg[part][r] = v


def main():
    zones = collections.defaultdict(lambda: {'fn': []})
    for p in sys.argv[2:]:
        scan(p, zones)
    with open(sys.argv[1], 'w') as f:
        for (h, blk), z in sorted(zones.items()):
            alg, fb, vi, opb = z['rep']
            fns = z['fn']
            f.write('%08x %d %d %d %d %s %d %d %d\n' % (h, alg, fb, blk, int(statistics.median(fns)), ' '.join('%02x' % b for b in opb), max(fns), len(fns), vi))
    print('table zones %d  distinct sub-voices %d  key-ons %d' % (len(zones), len({h for h, _ in zones}), sum(len(z['fn']) for z in zones.values())))


main()
