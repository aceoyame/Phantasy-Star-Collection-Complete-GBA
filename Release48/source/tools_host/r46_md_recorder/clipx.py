"""Extract a playback clip from a Genesis battle recording (source-recorded effect objects)."""
import struct, hashlib
from mdrec import ROMB, pieces

UI_TYPES_MAX = 0x40  # group-1 objects are battle cursors / command UI

def _rd(ram, a):
    a &= 0xFFFFFF
    return ram[a & 0xFFFF] if a >= 0xFF0000 else ROMB[a]

class _M:  # adapter so mdrec.pieces can read mapping bytes from a frame's RAM image
    def __init__(self, ram): self.ram = ram
    def r8(self, a): return self.ram[a & 0xFFFF]

GBA_SHAPES = {(1,1):(0,0),(2,2):(0,1),(4,4):(0,2),(8,8):(0,3),(2,1):(1,0),(4,1):(1,1),(4,2):(1,2),(8,4):(1,3),(1,2):(2,0),(1,4):(2,1),(2,4):(2,2),(4,8):(2,3)}
def gba_box(w, h):
    """Smallest GBA OBJ shape (in tiles) containing w x h tiles."""
    best = None
    for (gw, gh), sh in GBA_SHAPES.items():
        if gw >= w and gh >= h:
            a = gw * gh
            if best is None or a < best[0]: best = (a, gw, gh, sh)
    return best[1], best[2], best[3]

def md_tile_to_gba(t32):
    return bytes(((b >> 4) | ((b & 15) << 4)) for b in t32)

def block_bytes(vram, t0, w, h):
    gw, gh, sh = gba_box(w, h)
    out = bytearray()
    for gy in range(gh):
        for gx in range(gw):
            if gx < w and gy < h:
                t = (t0 + gx * h + gy) & 0x7FF
                out += md_tile_to_gba(vram[t * 32:t * 32 + 32])
            else:
                out += bytes(32)
    return bytes(out), gw, gh, sh

def md2gba(c):
    cv = (0, 4, 9, 13, 18, 22, 27, 31)
    return cv[(c >> 1) & 7] | (cv[(c >> 5) & 7] << 5) | (cv[(c >> 9) & 7] << 10)

ROLE_SCREEN, ROLE_PARTY, ROLE_ENEMY, ROLE_CASTER, ROLE_TARGET = 0, 1, 2, 3, 4

def extract(r, blocks):
    """r: recording dict; blocks: global dict content->block (mutated).
    Tracks get a spawn anchor role and, when the object travels from one fighter to another,
    an end anchor role (two-point x mapping at runtime)."""
    rec = r['rec']; fi = r['fighters']
    ids = {k + 1: f['id'] for k, f in enumerate(fi[:9])}
    def fx_at(t):
        ram = rec[t]['ram']; out = {}
        for k in range(9):
            if not ids[k + 1]: continue
            b = 0x4400 + k * 0x40 + 0xC
            out[k + 1] = struct.unpack('>h', ram[b:b + 2])[0]
        return out
    fx = fx_at(0)
    caster = r.get('caster_fighter'); target = r.get('target_fighter')
    def role_of(fn):
        if fn == caster: return (ROLE_CASTER, 0)
        if fn == target: return (ROLE_TARGET, 0)
        if fn <= 5: return (ROLE_PARTY, fn - 1)
        return (ROLE_ENEMY, fn - 6)
    def near(x, y, tol, region=True, fxx=None):
        best = None
        for fn, X in (fxx or fx).items():
            if region:
                if y >= 232 and fn > 5: continue
                if y < 232 and fn <= 5: continue
            d = abs(X - x) + (0 if fn in (caster, target) else 1000)
            if abs(X - x) <= tol and (best is None or d < best[0]): best = (d, fn)
        return best[1] if best else None
    def eff_objs(fr): return [o for o in fr['objs'] if o['type'] >= UI_TYPES_MAX]
    if 'planeb' in rec[0]:
        from planescan import plane_sig
        _s0 = plane_sig(rec[0], 'B'); pchg = [plane_sig(fr, 'B') != _s0 for fr in rec]
    else: pchg = [False] * len(rec)
    # Plane B is only rewritten by a few source effects (Profound Darkness, DarkForce2's
    # opening, Zio3, Black Hole).  Elsewhere a plane rewrite means the source reloaded the
    # formation (Fission / Tower reinforcements), which the port lowers in C: end the clip there.
    # Profound Darkness attacks: recorded on the stand-in beach background their plane-B
    # writes land on the wrong art, and the PD backgrounds are owned by the R45 PD system,
    # so their plane changes are ignored (objects/palettes still recorded).
    pd = r.get('eid') in (0x85, 0x86, 0x87)
    allow_bg = (r.get('eid') == 0x98 or r.get('ee87') is not None or r.get('kind') in ('combo', 'tech', 'skill'))
    if pd: pchg = [False] * len(rec)
    if not allow_bg and not pd and any(pchg):
        cut = pchg.index(True); rec = rec[:cut]; pchg = pchg[:cut]
    # boss openings leave their plane state behind (DarkForce2 rises on plane B and stays):
    # their length is the opening objects' lifetime only
    intro = r.get('ee87') is not None
    act = [i for i, fr in enumerate(rec) if eff_objs(fr) or (pchg[i] and not intro)]
    if not act:
        return None
    first = act[0]
    # stop at the first long idle gap (later objects belong to the next actor / victory windows)
    last = act[0]
    for i in act[1:]:
        if i - last > 60: break
        last = i
    sfx_last = max((i for i, fr in enumerate(rec[:last + 30]) if fr['sfx']), default=-1)
    end = max(last, sfx_last) + 1
    tracks = {}; order = []
    palettes = []; palidx = {}; tick_pal = []
    for t in range(end):
        fr = rec[t]; m = _M(fr['ram']); lines_used = set()
        for o in eff_objs(fr):
            key = (o['slot'], o['type'])
            tr = tracks.get(key)
            if tr is None or tr['end'] != t:
                tr = dict(slot=o['slot'], type=o['type'], start=t, end=t, raw=[])
                tracks[key] = tr; order.append(tr)
            pl = pieces(m, o)
            frame = []
            for (X, Y, size, tw) in pl:
                w = ((size >> 2) & 3) + 1; h = (size & 3) + 1
                data, gw, gh, sh = block_bytes(fr['vram'], tw & 0x7FF, w, h)
                if not any(data): continue
                k = hashlib.sha1(data).digest()
                if k not in blocks: blocks[k] = (len(blocks), data, gw, gh, sh, w, h)
                line = (tw >> 13) & 3; lines_used.add(line)
                frame.append((X - o['x'], Y - o['y'], blocks[k][0], (tw >> 11) & 3, line, (tw >> 15) & 1))
            tr['raw'].append((tuple(frame), o['x'], o['y']))
            tr['end'] = t + 1
        pp = [0xFF] * 4
        for ln in lines_used:
            pal = tuple(md2gba(c) for c in fr['cram'][ln * 16:(ln + 1) * 16])
            if pal not in palidx: palidx[pal] = len(palettes); palettes.append(pal)
            pp[ln] = palidx[pal]
        tick_pal.append(pp)
    # scene palette events: uniform additive tints (MD 3-bit steps) or absolute lines
    def lv(c): return ((c >> 1) & 7, (c >> 5) & 7, (c >> 9) & 7)
    base = rec[0]['cram']; scene = []; state = {}
    if r.get('ee87') is not None:
        # boss opening ($FFEE87): the source starts from its own palette state (e.g. DarkForce1
        # opens with a near-black background line and a black enemy line, then reveals them).
        # Record absolute lines 0-2 from tick 0; 'ref' = the steady state reached at the end,
        # which is what the port composed, so the runtime can map its background slots to MD
        # line-0 entries by colour.
        fin = rec[end - 1]['cram']
        scene.append((0, 0, ('ref',) + tuple(md2gba(c) for c in fin[0:16])))
        scene.append((0, 0, ('ref',) + tuple(md2gba(c) for c in base[0:16])))   # cycling entries: start phase
        for t in range(end):
            cr = rec[t]['cram']
            for ln in (0, 1, 2):
                ev = ('abs',) + tuple(md2gba(c) for c in cr[ln*16:ln*16+16])
                if state.get(ln) != ev: scene.append((t, ln, ev)); state[ln] = ev
        end_scene = True
    else: end_scene = False
    for t in (range(end) if not end_scene else ()):
        cr = rec[t]['cram']
        for ln in (0, 1, 2):
            a = base[ln*16:ln*16+16]; b = cr[ln*16:ln*16+16]
            if a == b: ev = ('none',)
            else:
                ch = [i for i in range(1, 16) if a[i] != b[i]]
                cands = set()
                for i in ch:
                    A = lv(a[i]); B = lv(b[i]); cands.add((B[0]-A[0], B[1]-A[1], B[2]-A[2]))
                def compat(i, d):
                    A = lv(a[i]); B = lv(b[i])
                    return all(B[k] == max(0, min(7, A[k] + d[k])) for k in range(3))
                best = None
                for d in cands:
                    if d == (0, 0, 0): continue
                    n = sum(1 for i in ch if compat(i, d))
                    if best is None or n > best[1]: best = (d, n)
                if best and best[1] >= 4 and best[1] * 2 >= len(ch):
                    mask = 0
                    for i in range(1, 16):
                        if compat(i, best[0]) and (a[i] != b[i] or lv(a[i]) in ((0,0,0),(7,7,7))): mask |= 1 << i
                    ev = ('tint',) + best[0] + (mask,)
                elif ln > 0 and len(set(b[1:])) == 1: ev = ('fill', md2gba(b[1]))
                elif ln > 0 and r.get('kind') in (None, 'enemy'): ev = ('abs',) + tuple(md2gba(c) for c in b)
                else: ev = ('none',)
            if state.get(ln, ('none',)) != ev:
                scene.append((t, ln, ev)); state[ln] = ev
    # background-plane replacement (e.g. Black Hole): while plane B's nametable/pattern
    # signature differs from the action's first frame, the clip carries one projected
    # 240x160 image per distinct signature (MD CRAM indices; transparent -> backdrop) plus,
    # per tick, the per-row horizontal shift (H-scroll relative to the image's reference
    # tick, 320->240) and a vertical row offset (V-scroll, 224->160).
    bg_frames = []; bg_shifts = []; bg = []; bgpal = []; cur = None; cur_pal = None
    def hscroll(fr):
        regs = fr['regs']; v = fr['vram']; base = (regs[13] & 0x3F) << 10; mode = regs[11] & 3
        out = []
        for l in range(224):
            ll = 0 if mode == 0 else (l & ~7 if mode == 2 else l)
            a_ = base + 4 * ll + 2
            out.append(((v[a_] << 8) | v[a_ + 1]) & 0x3FF)
        return out
    def sgn10(x): return x - 1024 if x >= 512 else x
    if 'planeb' in rec[0] and not intro and allow_bg:
        from planescan import plane_sig
        sig0 = plane_sig(rec[0], 'B')
        sigs = [plane_sig(rec[t], 'B') for t in range(end)]
        refs = {}
        for t in range(end):
            if sigs[t] == sig0: continue
            hs = hscroll(rec[t]); w = sum(abs(sgn10(x)) for x in hs) + abs(sgn10(rec[t]['vsram'][1] & 0x3FF))
            # the displayed plane lags VRAM by a frame: prefer ticks whose signature was already up
            if t == 0 or sigs[t - 1] != sigs[t]: w += 1 << 20
            if sigs[t] not in refs or w < refs[sigs[t]][0]: refs[sigs[t]] = (w, t)
        img_of = {}
        for sg, (w, t) in sorted(refs.items(), key=lambda kv: kv[1][1]):
            fr = rec[t]; pb = fr['planeb']; bd = fr['regs'][7] & 63
            out = bytearray(240 * 160)
            for gy in range(160):
                row = (gy * 7 // 5) * 320
                for gx in range(240):
                    v = pb[row + gx * 4 // 3]
                    out[gy * 240 + gx] = v if v else bd
            img_of[sg] = (len(bg_frames), hscroll(fr), fr['vsram'][1] & 0x3FF); bg_frames.append(bytes(out))
        img_used = [set(f) for f in bg_frames]
        shift_idx = {}
        for t in range(end):
            fr = rec[t]
            if sigs[t] == sig0:
                key = (-1, -1, 0); cur_pal = None
            else:
                ii, hs0, vs0 = img_of[sigs[t]]; hs = hscroll(fr)
                sh = tuple(max(-128, min(127, int(round(sgn10((hs[(gy * 7) // 5] - hs0[(gy * 7) // 5]) & 0x3FF) * 0.75)))) for gy in range(160))
                if any(sh):
                    if sh not in shift_idx: shift_idx[sh] = len(bg_shifts); bg_shifts.append(sh)
                    si = shift_idx[sh]
                else: si = -1
                vd = sgn10(((fr['vsram'][1] & 0x3FF) - vs0) & 0x3FF)
                pal = tuple(md2gba(c) for c in fr['cram'])
                used = img_used[ii]
                cols = set(pal[i] for i in used)
                if len(cols) == 1:   # every pixel shows one colour this tick (e.g. faded out): a fill
                    key = ('fill', cols.pop())
                else: key = (ii, si, int(round(vd * 5 / 7)))
                if pal != cur_pal: bgpal.append((t, pal)); cur_pal = pal
            if key != cur: bg.append((t,) + key); cur = key
        ref_imgs = sorted(set(e[1] for e in bg if e[1] != 'fill' and e[1] >= 0))
        remap = {o: n for n, o in enumerate(ref_imgs)}
        bg_frames = [bg_frames[o] for o in ref_imgs]
        bg = [e if (e[1] == 'fill' or e[1] < 0) else (e[0], remap[e[1]]) + e[2:] for e in bg]
    for tr in order:
        fxs = fx_at(tr['start']); fxe = fx_at(tr['end'] - 1)
        vis = [(x, y) for (f, x, y) in tr['raw'] if f]
        if not vis: vis = [(x, y) for (f, x, y) in tr['raw']]
        x0, y0 = vis[0]; x1, y1 = vis[-1]
        a = near(x0, y0, 48, fxx=fxs)
        b = near(x1, y1, 16, region=False, fxx=fxe)
        if a is not None and b is not None and b != a and abs(x1 - x0) >= 16 and abs(fxs[a] - fxe[b]) >= 16:
            tr['mode'] = 1; tr['A'] = role_of(a); tr['B'] = role_of(b); tr['ax'] = fxs[a]; tr['bx'] = fxe[b]
            tr['ent'] = [(f, x, y) for (f, x, y) in tr['raw']]          # absolute md x
        elif a is not None:
            tr['mode'] = 0; tr['A'] = role_of(a); tr['B'] = (ROLE_SCREEN, 0); tr['ax'] = fxs[a]; tr['bx'] = 0
            tr['ent'] = [(f, x - fxs[a], y) for (f, x, y) in tr['raw']]
        else:
            tr['mode'] = 0; tr['A'] = (ROLE_SCREEN, 0); tr['B'] = (ROLE_SCREEN, 0); tr['ax'] = 0; tr['bx'] = 0
            tr['ent'] = [(f, x, y) for (f, x, y) in tr['raw']]
        tr['anchor'] = tr['A']
        del tr['raw']
    return dict(ticks=end, first=first, tracks=order, palettes=palettes, tick_pal=tick_pal,
                sfx=[(t, s) for t in range(end) for s in rec[t]['sfx']], caster=caster, target=target, fx_first=fx_at(first), scene=scene,
                bg=bg, bg_frames=bg_frames, bg_shifts=bg_shifts, bgpal=bgpal)
