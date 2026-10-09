"""Simulate GBA affine playback of an extracted clip (same math as the GBA runtime)."""
from PIL import Image
PA = 341   # 256*4/3  (x scale 3/4)
PD = 358   # 256*7/5  (y scale 5/7)

def place(X, Y, w, h, gw, gh, hf, vf):
    """Return OBJ box origin (x,y) in GBA pixels for an MD piece at sprite coords X,Y. Mirrors C code."""
    cw, ch, W, H = w*8, h*8, gw*8, gh*8
    # content centre in GBA pixels, 1/56 fixed point avoided: use integer math *28
    pcx28 = (X - 128) * 21 + cw * 21 // 2          # *28: x*3/4 -> *21
    pcy28 = (Y - 128) * 20 + ch * 10               # *28: y*5/7 -> *20
    fx = -1 if hf else 1; fy = -1 if vf else 1
    bcx28 = pcx28 - fx * (cw - W) * 21 // 2
    bcy28 = pcy28 - fy * (ch - H) * 10
    bx = (bcx28 - W * 14) // 28
    by = (bcy28 - H * 14) // 28
    return bx, by

def draw_obj(img, block, pal, bx, by, gw, gh, hf, vf):
    W, H = gw*8, gh*8
    px = img.load()
    pa = -PA if hf else PA; pd = -PD if vf else PD
    for sy in range(by, by + H):
        if not 0 <= sy < 160: continue
        dy = sy - (by + H // 2)
        for sx in range(bx, bx + W):
            if not 0 <= sx < 240: continue
            dx = sx - (bx + W // 2)
            tx = ((pa * dx) >> 8) + W // 2; ty = ((pd * dy) >> 8) + H // 2
            if not (0 <= tx < W and 0 <= ty < H): continue
            t = (ty >> 3) * gw + (tx >> 3)
            b = block[t*32 + (ty & 7)*4 + ((tx & 7) >> 1)]
            ci = (b >> 4) if tx & 1 else (b & 15)
            if ci:
                c = pal[ci]; px[sx, sy] = (((c) & 31) << 3, ((c >> 5) & 31) << 3, ((c >> 10) & 31) << 3)

def render_tick(clip, blocks_by_id, t, bg, anchors_md_x):
    img = bg.copy()
    pals = clip['palettes']; tp = clip['tick_pal'][t]
    for tr in clip['tracks']:
        if not (tr['start'] <= t < tr['end']): continue
        frame, rx, Y = tr['ent'][t - tr['start']]
        xa = anchors_md_x(tr['A'])
        if xa is None: continue
        if tr['mode'] == 1:
            xb = anchors_md_x(tr['B']); den = tr['bx'] - tr['ax']
            TX = xa + ((rx - tr['ax']) * (xb - xa)) // den if den else xa
        else:
            TX = rx if tr['A'][0] == 0 else xa + rx
        for (dx, dy, bid, fl, line, prio) in frame:
            _, data, gw, gh, sh, w, h = blocks_by_id[bid]
            X = TX + dx; YY = Y + dy
            bx, by = place(X, YY, w, h, gw, gh, fl & 1, fl & 2)
            pal = pals[tp[line]] if tp[line] != 0xFF else [0]*16
            draw_obj(img, data, pal, bx, by, gw, gh, fl & 1, fl & 2)
    return img
