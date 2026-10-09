"""Parse PSC's native PSIII music scripts (12-byte commands, six channels) into note events."""
import struct, sys
ROM = open('/home/claude/work/handoff/CLAUDE_R48_CP07_HANDOFF/binaries/RUN_THIS_R48_CP07_SAFE_HANDOFF.gba','rb').read()
SONGS, CMDS, WAVES = 0x269EF0, 0x26A4F0, 0x152080
RATE = 21024.08
def cmd(i):
    o = CMDS + 12*i
    return ROM[o], ROM[o+1], struct.unpack_from('<H',ROM,o+2)[0], struct.unpack_from('<H',ROM,o+4)[0], struct.unpack_from('<I',ROM,o+8)[0]
def wave(i):
    o = WAVES + 60*i
    rep, ptr, cyc = struct.unpack_from('<III', ROM, o)
    return rep, ptr, cyc
def song_start(s): return list(struct.unpack_from('<6H', ROM, SONGS + 12*s))
def sx(v): return v-256 if v>=128 else v

def run_channel(start, frames, maxcmds=2000000):
    """Simulate the native script interpreter for one channel. Returns list of dict(note events) in frames."""
    ev = []; ptr = start; depth = 0; stack = [0]*8
    cnt = [0]*16; base = 0; vol = 0x80000>>16 if False else 0; t = 0; last = None
    vol = 0; n = 0
    while ptr is not None and t < frames and n < maxcmds:
        n += 1
        op, b1, h2, h4, w8 = cmd(ptr)
        if op == 0:                      # NOTE: wait h2, then sound h4
            t += h2
            use = last if b1 == 0x80 and last else (op, b1, h2, h4, w8)
            wi = base + use[1]
            ev.append(dict(t=t, dur=h4, wave=wi, step=use[4], vol=vol, tie=(b1==0x80), idx=ptr))
            if b1 != 0x80: last = (op, b1, h2, h4, w8)
            t += h4; ptr += 1
        elif op == 1: base = b1; ptr += 1
        elif op == 2: vol = sx(b1); ptr += 1
        elif op == 3: ptr = None
        elif op == 4: vol = max(0, min(0x1FC, vol + sx(b1))); ptr += 1
        elif op == 5: ptr = w8
        elif op == 6: stack[depth] = ptr+1; depth += 1; ptr = w8
        elif op == 7: depth -= 1; ptr = stack[depth]
        elif op == 8:
            if cnt[b1] == 0:
                cnt[b1] = h2 - 1; ptr = w8
            else:
                cnt[b1] -= 1
                ptr = w8 if cnt[b1] > 0 else ptr + 1
        elif op == 9: t += h2; ptr += 1
        else: ptr = None
    return ev
def freq(ev):
    rep, ptr, cyc = wave(ev['wave'])
    return RATE*ev['step']/cyc if cyc else 0.0
if __name__ == '__main__':
    s = int(sys.argv[1]) if len(sys.argv) > 1 else 0
    st = song_start(s); print('song', s, 'starts', st)
    for ch, p in enumerate(st):
        if p == 0xFFFF: print(ch, 'unused'); continue
        ev = run_channel(p, 1800)
        print(ch, len(ev), [(e['t'], e['dur'], e['wave'], round(freq(e),1)) for e in ev[:8]])
