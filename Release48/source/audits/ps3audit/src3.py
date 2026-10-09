"""Parse a VGM from the PSIII Z80 driver (YM2612 + SN76489) into per-channel note events (frame units)."""
import struct, sys, math
YMCLK = 7670454.0; PSGCLK = 3579545.0
def parse(path):
    d = open(path,'rb').read()
    pos = 0x100
    frame = 0
    ymaddr = [0,0]
    fnum = [0]*6; blk = [0]*6; fhi = {}
    keyed = [0]*6
    fm = [[] for _ in range(6)]
    dac_en = 0; dac_writes = {}
    psg_reg = [[0,0] for _ in range(4)]   # tone period for 0..2 ; noise for 3
    psg_att = [15]*4
    psg_latch = (0,0)
    psg = [[] for _ in range(4)]
    cur_note = [None]*6
    psg_frames = [[] for _ in range(4)]
    fm_frames = [[] for _ in range(6)]
    regs = [dict(), dict()]
    def fm_freq(ch):
        n = fnum[ch]; b = blk[ch]
        return n * (YMCLK/144.0) / (1 << (21-b)) if n else 0.0
    def psg_freq(p): return PSGCLK/(32.0*p) if p else 0.0
    def psg_note(ch, why):
        att = psg_att[ch]; p = psg_reg[ch][0]
        psg[ch].append(dict(t=frame, att=att, per=p, f=psg_freq(p), why=why))
    n = len(d)
    while pos < n:
        c = d[pos]
        if c == 0x62:
            for k in range(4): psg_frames[k].append((psg_att[k], psg_reg[k][0]))
            frame += 1; pos += 1
        elif c == 0x66: break
        elif c in (0x52, 0x53):
            port = c - 0x52; reg = d[pos+1]; val = d[pos+2]; pos += 3
            regs[port][reg] = val
            if port == 0 and reg == 0x28:
                chn = val & 7
                if chn in (3, 7): continue
                ch = chn if chn < 3 else chn-1
                slots = val >> 4
                if slots and not keyed[ch]:
                    fm[ch].append(dict(t=frame, f=fm_freq(ch), fnum=fnum[ch], blk=blk[ch], slots=slots, kind='on'))
                elif not slots and keyed[ch]:
                    fm[ch].append(dict(t=frame, kind='off'))
                keyed[ch] = slots
            elif port == 0 and reg == 0x2B: dac_en = val >> 7
            elif port == 0 and reg == 0x2A: dac_writes[frame] = dac_writes.get(frame, 0) + 1
            elif 0xA0 <= reg <= 0xAF:
                idx = reg & 3
                if idx == 3: continue
                ch = idx + 3*port
                if reg & 0xC == 4:       # A4..A6: block/fnum high (latches, full freq applies on A0 write)
                    fhi[ch] = val
                elif reg & 0xC == 0:     # A0..A2
                    hi = fhi.get(ch, 0)
                    fnum[ch] = ((hi & 7) << 8) | val; blk[ch] = (hi >> 3) & 7
                    if keyed[ch]: fm[ch].append(dict(t=frame, f=fm_freq(ch), fnum=fnum[ch], blk=blk[ch], kind='bend'))
        elif c == 0x50:
            v = d[pos+1]; pos += 2
            if v & 0x80:
                ch = (v >> 5) & 3; typ = (v >> 4) & 1; low = v & 15
                psg_latch = (ch, typ)
                if typ:
                    old = psg_att[ch]; psg_att[ch] = low
                    if ch < 3:
                        if old == 15 and low < 15: psg_note(ch, 'on')
                        elif low > old and False: pass
                    else: psg_note(3, 'noise_att')
                else:
                    if ch < 3: psg_reg[ch][0] = (psg_reg[ch][0] & 0x3F0) | low
                    else: psg_reg[3][0] = low; psg_note(3, 'noise_mode')
            else:
                ch, typ = psg_latch
                if typ == 0 and ch < 3:
                    psg_reg[ch][0] = (psg_reg[ch][0] & 0xF) | ((v & 0x3F) << 4)
                    if psg_att[ch] < 15: psg_note(ch, 'per')
        elif c == 0x61: pos += 3
        else: pos += 1
    return dict(frames=frame, fm=fm, psg=psg, psg_frames=psg_frames, dac=dac_writes, dac_enabled=dac_en)
def notes_fm(evs):
    out = []; cur = None
    for e in evs:
        if e['kind'] == 'on': cur = dict(t=e['t'], f=e['f'], slots=e['slots'], end=None); out.append(cur)
        elif e['kind'] == 'off' and cur and cur['end'] is None: cur['end'] = e['t']
    return out
if __name__ == '__main__':
    r = parse(sys.argv[1])
    print('frames', r['frames'], 'dac writes frames', len(r['dac']))
    for ch in range(6):
        ns = notes_fm(r['fm'][ch]); print('FM', ch+1, len(ns), [(n['t'], round(n['f'],1)) for n in ns[:6]])
    for ch in range(4): print('PSG', ch, len(r['psg'][ch]), [(n['t'], n['att'], round(n['f'],1), n['why']) for n in r['psg'][ch][:6]])
