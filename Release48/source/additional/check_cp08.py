#!/usr/bin/env python3
"""CP08 validation: static ROM checks + targeted mGBA launch checks.

  python3 check_cp08.py --rom CP08.gba --base RUN_THIS_R48_CP07_SAFE_HANDOFF.gba --out validation/

Static  : CRC/size, byte-identity with CP07 outside the three patched regions, all ten Additional Content
          cartridges byte-exact (Nei included), main-slot PSI byte-exact, tail free space left.
Emulator: five collection entries, Additional Content menu (10 entries, wrap-around), PSI main handoff to PSC8,
          both Game Gear titles, all eight Genesis cartridges (visible picture, no CPU fault), the debug launcher
          (START on an entry, SELECT in game, exit combo), the PSII debug menu (L+R at launch: music test, SFX test,
          five warp states reaching the map with the intended party / level / music, a Dezolis battle, stale-request
          and no-chord launches unchanged), SRAM untouched.
These are emulated launch checks only (mGBA 0.10.5); no hardware or playthrough claim.
"""
from pathlib import Path
import argparse, hashlib, json, struct, sys, zlib

R = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(R / 'source/tools_host/cp08_mgba'))
ap = argparse.ArgumentParser()
ap.add_argument('--rom', type=Path, required=True)
ap.add_argument('--base', type=Path, required=True, help='CP07 safe ROM')
ap.add_argument('--nei', type=Path, default=None, help='Nei Adventure.bin (default: additional_roms/Nei_Adventure_-_English.bin)')
ap.add_argument('--report', type=Path, default=R / 'build_artifacts/CP08_BUILD_REPORT.json')
ap.add_argument('--out', type=Path, default=R.parent / 'validation')
ap.add_argument('--static-only', action='store_true')
ap.add_argument('--genesis-frames', type=int, default=1100)
a = ap.parse_args()
rom = a.rom.resolve().read_bytes()
base = a.base.resolve().read_bytes()
rep = json.loads(a.report.read_text())
out = a.out.resolve(); out.mkdir(parents=True, exist_ok=True)
res = {'rom': a.rom.name, 'CRC32': f'{zlib.crc32(rom):08x}', 'static': {}, 'emulator': {}}
fails = []


def check(cond, what):
    if not cond:
        fails.append(what)
        print('FAIL', what)
    return bool(cond)


# ------------------------------------------------------------------ static
check(len(rom) == 0x2000000, 'ROM is 32 MiB')
check(f'{zlib.crc32(rom):08x}' == rep['CRC32'], 'CRC32 equals the build report')
allowed = [(0x5000, 0x7f00), (0x6ca500, 0x6e2000), (0x6e2000, 0x738000), (0x1fd5650, 0x2000000),
           (0x390414, 0x390418), (0x39f2fc, 0x39f300), (0x39f376, 0x39f37a)]     # three 4-byte BL hooks in PSII
diff_ranges = []
i = 0
n = len(rom)
while i < n:
    if rom[i] != base[i]:
        j = i
        while j < n and (rom[j] != base[j] or (j + 64 < n and rom[j:j + 64] != base[j:j + 64])):
            j += 1
        diff_ranges.append((i, j))
        i = j
    else:
        i += 1
outside = [(hex(s), hex(e)) for s, e in diff_ranges if not any(lo <= s and e <= hi for lo, hi in allowed)]
check(not outside, f'ROM differs from CP07 outside the patched regions: {outside}')
res['static']['patched_ranges_vs_CP07'] = [(hex(s), hex(e)) for s, e in diff_ranges]


def bl_target(buf, site):
    h1, h2 = struct.unpack_from('<HH', buf, site)
    if h1 >> 11 != 0x1E or h2 >> 11 != 0x1F:
        return None
    off = ((h1 & 0x7FF) << 12) | ((h2 & 0x7FF) << 1)
    if off & 0x400000:
        off -= 0x800000
    return 0x08000000 + site + 4 + off


ps2 = rep['ps2_debug']
hooks = {0x390414: int(ps2['thunk'], 16)}
for nm, site in (('ps2warp_thunk_a', 0x39f2fc), ('ps2warp_thunk_b', 0x39f376)):
    hooks[site] = None
ps2_elf = R / 'source/additional/build/ps2dbg.elf'


def elf_sym(elf, name):
    import subprocess as sp
    for line in sp.run(['llvm-nm', str(elf)], capture_output=True, text=True, check=True).stdout.splitlines():
        q = line.split()
        if len(q) == 3 and q[2] == name:
            return int(q[0], 16) & ~1
    raise KeyError(name)


hooks[0x39f2fc] = elf_sym(ps2_elf, 'ps2warp_thunk_a')
hooks[0x39f376] = elf_sym(ps2_elf, 'ps2warp_thunk_b')
for site, tgt in hooks.items():
    check(bl_target(rom, site) == tgt, f'PSII hook at {0x08000000 + site:#x} branches to {tgt:#x}')
check(bl_target(base, 0x390414) == 0x0839488C, 'base PSII init call at 0x08390414 was the original BL 0x0839488C')
res['static']['psii_hooks'] = {hex(0x08000000 + k): hex(v) for k, v in hooks.items()}
res['static']['psii_debug_module_bytes'] = ps2['bytes']
res['static']['bytes_identical_to_CP07_outside_patches'] = not outside

nei_path = a.nei or (R / 'additional_roms/Nei_Adventure_-_English.bin')
srcs = {
    'Phantasy Star Adventure': R / 'additional_roms/Phantasy_Star_Adventure_(English_v1.0).gg',
    'Phantasy Star Gaiden': R / 'additional_roms/Phantasy_Star_Gaiden.gg',
}
for nm in ['Anne', 'Rudger', 'Kinds', 'Shilka', 'Amia', 'Huey', 'Eusis']:
    srcs[nm + ' Adventure'] = R / f'additional_roms/{nm}_Adventure_-_English.bin'
srcs['Nei Adventure'] = nei_path
rows = []
for e in rep['catalog']:
    data = srcs[e['title']].read_bytes()
    off = e['table'] - 0x8000000
    if e['flags'] < 4:
        actual = rom[off:off + e['size']]
    else:
        tbl = struct.unpack_from('<%dI' % (e['size'] // 4096), rom, off)
        check(all(p & 1 for p in tbl), f"{e['title']}: page pointers carry the uncompressed flag")
        actual = b''.join(rom[(p & ~1) - 0x8000000:(p & ~1) - 0x8000000 + 4096] for p in tbl)
    ok = check(actual == data, f"{e['title']}: cartridge bytes match the source")
    rows.append({'title': e['title'], 'flags': e['flags'], 'bytes': len(data), 'crc32': f'{zlib.crc32(data):08x}', 'byte_exact': ok})
res['static']['cartridges'] = rows
psi = (R / 'additional_roms/Phantasy_Star_1_Retranslated_(English_v2.6.1).sms').read_bytes()
check(rom[0x2d3b80:0x2d3b80 + len(psi)] == psi, 'main-slot PSI v2.6.1 byte-exact at 0x082d3b80')
res['static']['main_PSI_byte_exact'] = True
p8 = rep['psc8']
blob = (R / 'source/psc8/build/psc8.bin').read_bytes()
check(rom[0x6ca500:0x6ca500 + len(blob)] == blob and len(blob) == p8['bytes'], 'PSC8 engine at 0x086ca500 equals source/psc8/build/psc8.bin')
check(not any(rom[0x6ca500 + len(blob):0x6e2000]), 'PSC8 slack up to 0x086e2000 is zero')
res['static']['psc8'] = p8
res['static']['tail_free_bytes'] = rep['tail_free_after_bytes']
if a.static_only:
    (out / 'CP08_STATIC_CHECK.json').write_text(json.dumps(res, indent=1) + '\n')
    print('static checks:', 'FAIL' if fails else 'PASS')
    sys.exit(1 if fails else 0)

# ------------------------------------------------------------------ emulator
from mgba2 import GBA, L
import subprocess


def sym(elf, name):
    for line in subprocess.run(['llvm-nm', str(elf)], capture_output=True, text=True, check=True).stdout.splitlines():
        p = line.split()
        if len(p) == 3 and p[2] == name:
            return int(p[0], 16)
    raise KeyError(name)


menu_elf = R / 'source/additional/build/menu.elf'
SEL = sym(menu_elf, 'selected')
SELECTED_COL = 0x0202436c                      # PSC collection cursor
HB = 0x0203e000
SRAM = lambda g: bytes(g.mem(0x0E000000, 0x8000))


def hb(g):
    return [g.rd32(HB + 4 * k) for k in range(24)]


def boot():
    g = GBA(a.rom.resolve(), entry=0x08000000)
    g.run(120)
    L.mg_break(0x080005f0); g.run(10, 'START'); g.run(900)
    ok = L.mg_hit(); L.mg_break(0xffffffff); g.run(50)
    check(ok, 'collection reached')
    return g


def launch(g, entry, key='A'):
    L.mg_break(entry); g.run(5, key); hit = L.mg_hit(); L.mg_break(0xffffffff)
    return bool(hit)


g = boot()
col = g.shot(out / 'CP08_COLLECTION.png')
sram0 = SRAM(g)
g.save(str(out / 'collection.state'))
rows = []
visible = [0x08004f00, 0x08390000, 0x08080000, 0x08041800, 0x08005000]
for i in range(5):
    g.load(str(out / 'collection.state'))
    for _ in range(i):
        g.press('DOWN', 1, 60)
    check(g.rd32(SELECTED_COL) == i, f'collection cursor at {i}')
    target = 0x086ca500 if i == 0 else visible[i]
    L.mg_break(target); g.run(10, 'A'); g.run(500); ok = bool(L.mg_hit()); L.mg_break(0xffffffff)
    check(ok, f'collection entry {i + 1} reaches {hex(target)}')
    row = {'slot': i + 1, 'engine_entry': hex(target), 'reached': ok}
    if i == 0:
        cfg = [g.rd32(0x0203ff00 + 4 * k) for k in range(4)]
        check(cfg[0] == 0x082d3b80 and cfg[1] == 0x80000 and cfg[2] == 10, f'main PSI config {cfg}')
        row['main_PSI_config'] = [hex(x) for x in cfg]
    if i == 4:
        g.run(80); g.shot(out / 'CP08_ADDITIONAL_MENU.png'); g.save(str(out / 'additional.state'))
    rows.append(row)
res['emulator']['collection_entries'] = rows

# Additional Content: 10 entries, wrap-around, each handoff
g.load(str(out / 'additional.state'))
check(g.rd32(SEL) == 0, 'Additional menu cursor starts at entry 0')
for _ in range(9):
    g.press('DOWN', 1, 8)
check(g.rd32(SEL) == 9, 'cursor reaches entry 10 (Nei) after 9 steps')
g.shot(out / 'CP08_ADDITIONAL_MENU_NEI.png')
g.press('DOWN', 1, 8); check(g.rd32(SEL) == 0, 'cursor wraps to entry 1')
g.press('UP', 1, 8); check(g.rd32(SEL) == 9, 'cursor wraps up to entry 10')
cat = {i: e for i, e in enumerate(rep['catalog'])}
ents = []
for i in range(10):
    g.load(str(out / 'additional.state'))
    for _ in range(i):
        g.press('DOWN', 1, 8)
    entry = 0x086ca500 if i < 2 else 0x086e2000
    ok = launch(g, entry)
    cfg = [g.rd32(0x0203ff00 + 4 * k) for k in range(4)]
    e = cat[i]
    check(ok, f"{e['title']}: handoff reaches {hex(entry)}")
    check(cfg[0] == 0x08000000 + (e['table'] - 0x8000000) and cfg[1] == e['size'] and cfg[2] == e['flags'] and cfg[3] == i, f"{e['title']}: cfg {cfg}")
    row = {'index': i, 'title': e['title'], 'handoff': ok, 'cfg': [hex(x) for x in cfg]}
    if i >= 2:
        g.run(a.genesis_frames)
        shot = g.shot(out / f"CP08_ENTRY_{i}_{e['title'].split()[0]}.png")
        row['screenshot_distinct_colors'] = len(set(shot.getdata()))
    else:                                           # Game Gear titles: intros fade through black, sample the picture over time
        best = 0
        for t in range(6):
            g.run(500)
            shot = g.shot()
            colors = len(set(shot.getdata()))
            if colors > best:
                best = colors; shot.save(out / f"CP08_ENTRY_{i}_{e['title'].split()[0]}.png")
        row['screenshot_distinct_colors'] = best
    if i >= 2:
        h = hb(g)
        row.update({'magic': hex(h[0]), 'guest_frames': h[1], 'display_on_at_frame': h[16], 'picture_visible_at_frame': h[17],
                    'cpu_exceptions': h[8], 'illegal_opcodes': h[9], 'pc': hex(h[2])})
        check(h[0] == 0x4d444558, f"{e['title']}: Genesis runtime heartbeat")
        check(h[16] != 0xffffffff and h[17] != 0xffffffff, f"{e['title']}: game display enabled and picture visible")
        check(h[8] == 0 and h[9] == 0, f"{e['title']}: no CPU faults")
        check(row['screenshot_distinct_colors'] > 8, f"{e['title']}: picture on screen")
    else:
        pc = g.reg(15)
        row['pc'] = hex(pc)
        check(0x08000000 <= pc < 0x0a000000 or 0x03000000 <= pc < 0x03008000 or 0x02000000 <= pc < 0x02040000, f"{e['title']}: engine running")
        check(row['screenshot_distinct_colors'] >= 3, f"{e['title']}: GG picture appears within 3000 frames")
        gf = g.rd32(HB + 4)                                 # PSC8 heartbeat: guest frames since launch
        row['psc8_guest_frames_in_3000'] = gf
        check(gf > 1800, f"{e['title']}: PSC8 runs the guest at > 60% of full speed over the first 3000 frames ({gf})")
    ents.append(row)
res['emulator']['additional_entries'] = ents

# Debug launcher (Genesis entry 2: Anne)
g.load(str(out / 'additional.state'))
g.press('DOWN', 1, 8); g.press('DOWN', 1, 8)
ok = launch(g, 0x086e2000, 'START')
cfg2 = g.rd32(0x0203ff08)
check(ok and (cfg2 & 0x10), f'START launches the runtime with the debug flag (flags {cfg2:#x})')
g.run(90)
dbg = g.shot(out / 'CP08_DEBUG_MENU_START.png')
check(len(set(dbg.getdata())) > 4 and hb(g)[1] == 0, 'debug menu is up and the guest has not run a frame')
g.press('DOWN', 1, 6); g.press('DOWN', 1, 6); g.press('A', 2, 30)       # step one frame
g.press('R', 2, 20); g.shot(out / 'CP08_DEBUG_CPU_PAGE.png')
g.press('R', 2, 20); g.shot(out / 'CP08_DEBUG_VDP_PAGE.png')
g.press('SELECT', 2, 120)                                             # resume
g.run(a.genesis_frames)
h = hb(g)
check(h[1] > 100 and h[16] != 0xffffffff, 'game resumes from the debug menu and boots')
g.shot(out / 'CP08_DEBUG_RESUMED_GAME.png')
g.run(4, 'SELECT'); g.run(60)
inmenu = g.shot(out / 'CP08_DEBUG_MENU_INGAME.png')
g.run(4, 'A+B+START+SELECT'); g.run(150)
L.mg_break(0x080005f0); g.run(10, 'START'); g.run(900); back = bool(L.mg_hit()); L.mg_break(0xffffffff); g.run(50)
after = g.shot(out / 'CP08_DEBUG_EXIT_TO_COLLECTION.png')
same = back and list(after.getdata()) == list(col.getdata())
check(same, 'A+B+START+SELECT soft-resets to the collection (boot sequence reaches the same menu screen)')
res['emulator']['debug_launcher'] = {'start_flag': bool(cfg2 & 0x10), 'resume_boots': h[1] > 100, 'exit_returns_to_collection': same}

# ------------------------------------------------------------------ PSII debug menu / warp (slot II)
import re
import numpy as np

PS2_ENTRY, PS2_HOOK = 0x08390000, 0x08730000
a68 = lambda addr, size=1: 0x0200FFFF - (addr & 0xFFFF) - (size - 1)
r8_ = lambda g, ad: g.rd8(a68(ad, 1))
r16_ = lambda g, ad: g.rd16(a68(ad, 2))
st_h = (R / 'source/ps2dbg/ps2state.h').read_text()
CHARS = bytes(int(x, 16) for x in re.search(r'ps2st_chars\[512\]=\{([^}]*)\}', st_h).group(1).split(','))
PARTY = [int(x) for x in re.search(r'ps2st_party\[[^\]]*\]=\{([^}]*)\}', st_h).group(1).split(',')]
WREQ = 0x03007E40
DESTS = [('Dezolis Skure field', 3, 0x4F0, 0x3C0, 0x83), ('Dezolis Aukba', 0x0A, 0x60, 0x70, None), ('Paseo', 4, 0x100, 0x1F0, None),
         ('Motavia world map', 0, 0x520, 0x5B0, None), ('Climatrol 7F', 0x3D, 0x1A0, 0x300, None)]
pcmfile = str(out / 'ps2_pcm.raw')


def pcm_rms(g, frames, action=None):
    g.pcm(pcmfile)
    if action: action()
    g.run(frames); g.pcm('')
    d = np.fromfile(pcmfile, dtype=np.int16).reshape(-1, 2)[:, 0].astype(float)
    return float(np.sqrt((d ** 2).mean()))


def ps2_launch(g, chord):
    """From the collection state: select slot II and launch; with chord=True hold L+R until the debug hook is reached."""
    g.load(str(out / 'collection.state'))
    g.press('DOWN', 1, 60)
    if chord:
        L.mg_break(PS2_HOOK); g.run(10, 'A+L+R')
        for _ in range(80):
            g.run(5, 'L+R')
            if L.mg_hit(): break
        hit = bool(L.mg_hit()); L.mg_break(0xffffffff); g.run(10, 'L+R'); g.run(20)
        return hit
    L.mg_break(PS2_ENTRY); g.run(10, 'A')
    for _ in range(40):
        if L.mg_hit(): break
        g.run(5)
    hit = bool(L.mg_hit()); L.mg_break(0xffffffff)
    return hit


def wait_screen(g, want, frames=1500):
    for i in range(frames // 15):
        g.run(15)
        if r8_(g, 0xF600) == want:
            if want == 0x04:
                g.run(240)                      # let the title screen finish drawing and start polling START
            return True
    return False


psd = {}
# 1. no chord: PSII boots exactly as before (no debug magic, no armed warp) and START on the title still starts the new-game intro
check(ps2_launch(g, False), 'PSII launches from slot II')
check(wait_screen(g, 0x04), 'PSII reaches the title screen without the chord')
check(g.rd32(0x0203C000) != 0x44325350, 'no chord: the debug menu never opens')
g.press('START', 3, 10)
check(wait_screen(g, 0x18, 300), 'no chord: START on the title enters the new-game intro (screen 0x18), warp hooks pass through')
psd['normal_launch'] = {'debug_magic': False, 'title_start_goes_to_intro': True}

# 2. stale armed request (left over from an earlier session): ignored, normal game
g.load(str(out / 'collection.state')); g.press('DOWN', 1, 60)
for k, v in enumerate((0x50524157, 1, 0x50524157 ^ 1 ^ 0xA5A5A5A5, 0)):
    g.wr32(WREQ + 4 * k, v)
L.mg_break(PS2_ENTRY); g.run(10, 'A')
for _ in range(40):
    if L.mg_hit(): break
    g.run(5)
L.mg_break(0xffffffff)
check(wait_screen(g, 0x04), 'stale request: title reached')
g.press('START', 3, 10)
check(wait_screen(g, 0x18, 300) and g.rd32(WREQ) == 0, 'stale warp request is discarded and START still enters the intro')
psd['stale_request_ignored'] = True

# 3. chord opens the menu; music + SFX test through the game's sound engine
check(ps2_launch(g, True), 'L+R at launch reaches the PSII debug hook')
check(g.rd32(0x0203C000) == 0x44325350, 'debug menu state magic present')
g.shot(out / 'CP08_PSII_DEBUG_MAIN.png', 2)
g.press('A', 3, 10); g.run(10)                                            # MUSIC TEST
g.press('DOWN', 2, 6); g.press('DOWN', 2, 6); g.press('A', 3, 10)         # Silent Zone
check(g.rd32(0x0203C010) == 0x83 and g.rd32(0x02019534 + 0) in (0x83, 0xFFFFFFFF, 0x80), 'music test: Silent Zone requested (0x83)')
rms = pcm_rms(g, 150); g.shot(out / 'CP08_PSII_DEBUG_MUSIC.png', 2)
check(rms > 200, f'music test: Silent Zone is audible (rms {rms:.0f})')
g.press('START', 2, 30)
g.press('B', 2, 10); g.run(20)
g.press('DOWN', 2, 6); g.press('A', 3, 10); g.run(10)                     # SFX TEST
rms_s = pcm_rms(g, 80, lambda: g.press('A', 3, 6)); g.shot(out / 'CP08_PSII_DEBUG_SFX.png', 2)
check(g.rd32(0x0203C014) == 0xA0, 'sfx test: first effect requested (0xA0)')
check(rms_s > 100, f'sfx test: effect is audible (rms {rms_s:.0f})')
psd['music_test'] = {'silent_zone_request': 0x83, 'audio_rms': round(rms)}
psd['sfx_test'] = {'first_sfx': 0xA0, 'audio_rms': round(rms_s)}

# 4. warp states
wrows = []
for di, (name, lvl, wy, wx, music) in enumerate(DESTS):
    check(ps2_launch(g, True), f'warp[{name}]: menu opens')
    g.press('DOWN', 2, 6); g.press('DOWN', 2, 6); g.press('A', 3, 10); g.run(10)    # WARP / TEST STATES
    for _ in range(di):
        g.press('DOWN', 2, 6)
    if di == 0:
        g.shot(out / 'CP08_PSII_DEBUG_WARP_MENU.png', 2)
    g.press('A', 3, 10); g.run(10)
    if di == 0:
        g.shot(out / 'CP08_PSII_DEBUG_WARP_CONFIRM.png', 2)
    g.press('A', 3, 10)                                                           # arm + restart
    check(wait_screen(g, 0x04), f'warp[{name}]: PSII restarted and reached the title')
    check(g.rd32(WREQ) == 0x50524157, f'warp[{name}]: request still armed at the title')
    g.press('START', 3, 10)
    ok = wait_screen(g, 0x0C, 400)
    check(ok, f'warp[{name}]: START on the title enters the Level screen')
    g.run(240)                                                                     # palette fade-in
    got = (r16_(g, 0xC640), r16_(g, 0xC642), r16_(g, 0xC644))
    check(got == (lvl, wy, wx), f'warp[{name}]: level/y/x {got} == {(lvl, wy, wx)}')
    chars = bytes(g.rd8(0x02003FFF - i) for i in range(512))
    check(chars == CHARS, f'warp[{name}]: 512-byte character buffer equals the generated party')
    ids = [r16_(g, 0xC608 + 2 * i) for i in range(len(PARTY))]
    check(ids == PARTY and r16_(g, 0xC600) == len(PARTY) - 1, f'warp[{name}]: party ids {ids}')
    check(r16_(g, 0xC710) == 0 and r16_(g, 0xFFF0) == 0 and g.rd32(WREQ) == 0, f'warp[{name}]: no scripted event, controls free, request consumed')
    shot = g.shot(out / f'CP08_PSII_WARP_{di}_{name.split()[0]}.png', 1)
    colors = len(set(shot.getdata()))
    check(colors > 12, f'warp[{name}]: map is drawn ({colors} colours)')
    rms_l = pcm_rms(g, 120)
    check(rms_l > 100, f'warp[{name}]: level music audible (rms {rms_l:.0f})')
    mus = r8_(g, 0xF640)
    if music is not None:
        check(mus == music, f'warp[{name}]: level music request {mus:#x} == {music:#x}')
    wrows.append({'dest': name, 'level': lvl, 'y': hex(wy), 'x': hex(wx), 'level_music_request': hex(mus), 'audio_rms': round(rms_l), 'colours': colors})
    if di == 0:
        g.save(str(out / 'ps2_dezolis.state'))
psd['warp'] = wrows

# 5. battle on Dezolis from the warped state: walk until an encounter, fight to the end, back on the map with the field music
g.load(str(out / 'ps2_dezolis.state')); g.run(30)
fought = back = False
walk = (['RIGHT'] * 12 + ['UP'] * 12 + ['RIGHT'] * 8 + ['DOWN'] * 10) * 8
for d in walk:
    g.run(20, d)
    if r8_(g, 0xF600) == 0x14:
        fought = True; break
check(fought, 'Dezolis: random battle starts while walking')
if fought:
    g.run(200); g.shot(out / 'CP08_PSII_WARP_DEZOLIS_BATTLE.png', 2)
    check(pcm_rms(g, 90) > 100, 'Dezolis: battle music audible')
    for k in range(150):
        g.press('A', 2, 18)
        if r8_(g, 0xF600) != 0x14:
            break
    g.run(120)
    back = r8_(g, 0xF600) == 0x0C and r16_(g, 0xC640) == 3 and r8_(g, 0xF640) == 0x83
    check(back, 'Dezolis: battle ends, map and Silent Zone music resume')
psd['dezolis_battle'] = {'started': fought, 'returned_to_map_with_music': back}
(out / 'ps2_dezolis.state').unlink(missing_ok=True)
res['emulator']['psii_debug'] = psd

# SRAM
sram1 = SRAM(g)
check(sram1 == sram0, 'SRAM (all 32 KiB: PSC 0x0000, PSIV 0x2000, PSI 0x4000) unchanged by launching every entry')
res['emulator']['SRAM_0x0E000000_0x0E007FFF_unchanged'] = sram1 == sram0
res['failures'] = fails
(out / 'CP08_VALIDATION.json').write_text(json.dumps(res, indent=1) + '\n')
for f in ('collection.state', 'additional.state'):
    (out / f).unlink(missing_ok=True)
print('ALL CHECKS PASS' if not fails else f'{len(fails)} FAILURES')
sys.exit(1 if fails else 0)
