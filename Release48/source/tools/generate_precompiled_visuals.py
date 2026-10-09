#!/usr/bin/env python3
from pathlib import Path
from PIL import Image
import argparse, hashlib, struct, json, re
from md_compression import nemesis_decompress, enigma_decompress
ROOT=Path(__file__).resolve().parent.parent
SRC=ROOT/'generated_precomp'
OUT=ROOT/'gba'/'precomp_assets.bin'
HDR=ROOT/'gba'/'precomp_assets.h'
EXPECTED_SHA='511f35cc11f88316f8b8940e28ab298bd75a4da193672a80172884d6eb913b6a'

ap=argparse.ArgumentParser()
ap.add_argument('--rom',required=True,type=Path)
args=ap.parse_args()
rom=args.rom.read_bytes()
digest=hashlib.sha256(rom).hexdigest()
if len(rom)!=0x300000 or digest!=EXPECTED_SHA:
    raise SystemExit(f'wrong PSIV source: size={len(rom)} sha256={digest}')

def gba(c):
    r,g,b=c
    return (r>>3)|((g>>3)<<5)|((b>>3)<<10)

def scale(im,size=(240,160)):
    # nearest neighbor is intentional: preserves authored hard pixel edges.
    return im.convert('RGB').resize(size,Image.Resampling.NEAREST)

def md_rgb(w):
    return (((w>>1)&7)*255//7,((w>>5)&7)*255//7,((w>>9)&7)*255//7)

def render_battle_bg(index):
    """Render one BattleBGArtPtrs entry exactly into the 320x224 source canvas.

    Battle_SetupBackground Nemesis-decompresses art, Enigma-decompresses a
    64x24 Plane-B map, clears CRAM entry 0 and loads 13 palette words into
    entries 1..13.  Only the first 40 columns are visible in the 320-pixel
    Genesis viewport; the final 32 source scanlines are clear because the map
    is only 24 tiles high.
    """
    rec=0x6ED4+index*12
    art=int.from_bytes(rom[rec:rec+4],'big')
    mapping=int.from_bytes(rom[rec+4:rec+8],'big')
    palette=int.from_bytes(rom[rec+8:rec+12],'big')
    tiles=nemesis_decompress(rom[art:])
    cells=enigma_decompress(rom[mapping:])
    if len(cells)!=64*24:
        raise ValueError(f'battle bg {index}: expected 1536 cells, got {len(cells)}')
    cols=[(0,0,0)]+[md_rgb(int.from_bytes(rom[palette+i*2:palette+i*2+2],'big')) for i in range(13)]
    out=Image.new('RGB',(320,224),(0,0,0))
    tile_count=len(tiles)//32
    for ty in range(24):
        for tx in range(40):
            e=cells[ty*64+tx]
            ti=e&0x07FF
            if ti>=tile_count: raise ValueError(f'battle bg {index}: tile {ti} >= {tile_count}')
            hf=bool(e&0x0800); vf=bool(e&0x1000)
            tile=tiles[ti*32:(ti+1)*32]
            for py in range(8):
                sy=7-py if vf else py
                for px in range(8):
                    sx=7-px if hf else px
                    q=tile[sy*4+(sx>>1)]
                    ci=(q&15) if (sx&1) else (q>>4)
                    if ci>=len(cols):
                        raise ValueError(f'battle bg {index}: palette index {ci}')
                    out.putpixel((tx*8+px,ty*8+py),cols[ci])
    return out

def render_battle_command_icons():
    """Decode the five selectable Battle_OpenCharComd icons from PSIV.

    loc_27D860 supplies Nemesis art beginning at VDP tile $15E.  The five
    BattleTiles_* tables at $7724 are 2x2 source-tile maps using battle
    palette line 3.  loc_7634 copies the first 15 words at $76A4 to palette
    entries 49..63, so mapping color n uses source palette word n-1.
    """
    art=nemesis_decompress(rom[0x27D860:])
    base_tile=0x15E
    maps=[
        (0xE15E,0xE15F,0xE16E,0xE16F),
        (0xE160,0xE161,0xE170,0xE171),
        (0xE162,0xE962,0xF162,0xF962),
        (0xE163,0xE164,0xE172,0xE173),
        (0xE165,0xE166,0xE174,0xE974),
    ]
    cols=[None]+[md_rgb(int.from_bytes(rom[0x76A4+(i-1)*2:0x76A6+(i-1)*2],'big')) for i in range(1,16)]
    out=[]
    for words in maps:
        im=Image.new('RGB',(16,16),(248,0,248))
        for cell,w in enumerate(words):
            ti=(w&0x07FF)-base_tile
            if ti<0 or (ti+1)*32>len(art):
                raise ValueError(f'command icon tile {w&0x07ff:03X} outside Nemesis stream')
            hf=bool(w&0x0800);vf=bool(w&0x1000);tile=art[ti*32:(ti+1)*32]
            ox=(cell&1)*8;oy=(cell>>1)*8
            for py in range(8):
                sy=7-py if vf else py
                for px in range(8):
                    sx=7-px if hf else px
                    q=tile[sy*4+(sx>>1)]
                    ci=(q&15) if (sx&1) else (q>>4)
                    if ci: im.putpixel((ox+px,oy+py),cols[ci])
        out.append(im)
    return out

def render_status_portraits_indexed():
    """Decode Win_StatusCharList's 11 exact 10x10 portrait maps.

    loc_5D548 is the Nemesis-art pointer table and loc_5D574 is the matching
    PlaneMapToRAM 10x10 mapping table.  Mapping tile $580 is the decompressed
    art base and palette line 2 is intentionally retained as 0..15 indices;
    runtime maps those indices through the live source CRAM line.
    """
    arts=[0x297960,0x297EB2,0x298468,0x298892,0x298E00,0x299338,0x2998D8,0x299F98,0x29A518,0x29AAD6,0x29B126]
    maps=[0x29B5C4,0x29B68C,0x29B754,0x29B81C,0x29B8E4,0x29B9AC,0x29BA74,0x29BB3C,0x29BC04,0x29BCCC,0x29BD94]
    frames=[]
    for artp,mapp in zip(arts,maps):
        art=nemesis_decompress(rom[artp:]); tile_count=len(art)//32; out=bytearray(80*80)
        for cy in range(10):
            for cx in range(10):
                w=int.from_bytes(rom[mapp+(cy*10+cx)*2:mapp+(cy*10+cx)*2+2],'big')
                ti=(w&0x07ff)-0x580
                if ti<0 or ti>=tile_count: raise ValueError(f'status portrait tile {w&0x7ff:03X} outside {tile_count}')
                hf=bool(w&0x0800);vf=bool(w&0x1000);tile=art[ti*32:(ti+1)*32]
                for py in range(8):
                    sy=7-py if vf else py
                    for px in range(8):
                        sx=7-px if hf else px;q=tile[sy*4+(sx>>1)];ci=(q&15) if (sx&1) else (q>>4)
                        out[(cy*8+py)*80+cx*8+px]=ci
        frames.append(bytes(out))
    return frames

def indexed(im, palette=None, transparent=None):
    pix=list(im.convert('RGB').getdata())
    if palette is None:
        colors=[]
        for c in pix:
            if c not in colors: colors.append(c)
        if len(colors)>256: raise ValueError(len(colors))
        palette=colors
    lut={c:i for i,c in enumerate(palette)}
    if transparent is not None and transparent not in lut:
        raise ValueError('transparent color missing')
    arr=bytes(lut[c] for c in pix)
    pals=b''.join(struct.pack('<H',gba(c)) for c in palette)+b'\0\0'*(256-len(palette))
    return arr,pals,palette

blob=bytearray(); defs={}
def add(name,data,align=4):
    while len(blob)%align: blob.append(0)
    defs[name]=len(blob); blob.extend(data)

# Principal scenes use the complete PSC panel catalog. The three old bring-up
# screenshots and private palettes have no runtime consumers; keep their source
# images for reference without duplicating them in the linked bank.

# Battle uses one exact union palette.  All 32 backgrounds are decoded directly
# from BattleBGArtPtrs; the old hand-authored battle_bg.png is no longer an
# authority. Animated enemy images are generated from PSIV
# EnemySpriteMappingsOffs and stored once per reachable visual state.
battle_bgs=[scale(render_battle_bg(i)) for i in range(32)]
battle_command_icons=[im.resize((13,12),Image.Resampling.NEAREST) for im in render_battle_command_icons()]
# Source 16x16 battle command icons are projected once at build time.
# The 13x12 target is the smallest readability adjustment above the strict
# 12x11 viewport projection reported as undersized in hardware/emulator testing.
# Runtime consumes these pixels directly; no per-frame scalar remains.
anim_dir=SRC/'enemy_anim'
x_frames=[Image.open(p).convert('RGB') for p in sorted(anim_dir.glob('xanaf_*.png'))]
i_frames=[Image.open(p).convert('RGB') for p in sorted(anim_dir.glob('igg_*.png'))]
attack_dir=SRC/'enemy_attack'
party_dir=SRC/'party_battle'
party_attack_dir=SRC/'party_attack'
technique_dir=SRC/'technique'
party_manifest=json.loads((party_dir/'manifest.json').read_text()) if (party_dir/'manifest.json').exists() else None
party_attack_manifest=json.loads((party_attack_dir/'manifest.json').read_text()) if (party_attack_dir/'manifest.json').exists() else None
technique_manifest=json.loads((technique_dir/'manifest.json').read_text()) if (technique_dir/'manifest.json').exists() else None
party_sheets=[]
if party_manifest:
    for name,ch in party_manifest['characters'].items():
        party_sheets.append((name,'poses',Image.open(party_dir/ch['pose_file']).convert('RGB')))
        if 'attack' in ch: party_sheets.append((name,'attack',Image.open(party_dir/ch['attack']['file']).convert('RGB')))
attack_manifest=json.loads((attack_dir/'manifest.json').read_text()) if (attack_dir/'manifest.json').exists() else None
attack_sheets=[]
if attack_manifest:
    for seq in attack_manifest['sequences']:
        for part in seq['parts']: attack_sheets.append((seq['name'],part,Image.open(attack_dir/part['file']).convert('RGB')))
    micro=attack_manifest['micro_missile']
    for group in ('parent','burst','missile','impact'):
        for part in micro[group]['parts']: attack_sheets.append(('jurafa_micro_'+group,part,Image.open(attack_dir/part['file']).convert('RGB')))
trans=(248,0,248)
colors=[trans,(0,0,0)]
party_attack_sheets=[]
if party_attack_manifest:
    for an,a in party_attack_manifest['alys_slasher']['animations'].items():
        for part in a['parts']: party_attack_sheets.append(('alys_slasher_'+an,part,Image.open(party_attack_dir/part['file']).convert('RGB')))
    if 'vortex' in party_attack_manifest:
        for an,a in party_attack_manifest['vortex']['animations'].items():
            for part in a['parts']: party_attack_sheets.append(('vortex_'+an,part,Image.open(party_attack_dir/part['file']).convert('RGB')))
    if 'earth' in party_attack_manifest:
        for an in ('main','success'):
            a=party_attack_manifest['earth'][an]
            for part in a['parts']: party_attack_sheets.append(('earth_'+an,part,Image.open(party_attack_dir/part['file']).convert('RGB')))
    if 'vision' in party_attack_manifest:
        a=party_attack_manifest['vision']['effect']
        for part in a['parts']: party_attack_sheets.append(('vision_effect',part,Image.open(party_attack_dir/part['file']).convert('RGB')))
    for part in party_attack_manifest['demi_impacter']['parts']: party_attack_sheets.append(('demi_impacter_shot',part,Image.open(party_attack_dir/part['file']).convert('RGB')))
technique_sheets=[]
if technique_manifest:
    for an in ('cast','orbit','release','impact'):
        a=technique_manifest['foi'][an]
        for part in a['parts']: technique_sheets.append(('foi_'+an,part,Image.open(technique_dir/part['file']).convert('RGB')))
    if 'nafoi' in technique_manifest:
        for an in ('cast','charge','attack'):
            a=technique_manifest['nafoi'][an]
            for part in a['parts']: technique_sheets.append(('nafoi_'+an,part,Image.open(technique_dir/part['file']).convert('RGB')))
    if 'wat' in technique_manifest:
        for an in ('cast','orbit','release','impact'):
            a=technique_manifest['wat'][an]
            for part in a['parts']: technique_sheets.append(('wat_'+an,part,Image.open(technique_dir/part['file']).convert('RGB')))
    if 'tsu' in technique_manifest:
        for an in ('cast','attack'):
            a=technique_manifest['tsu'][an]
            for part in a['parts']: technique_sheets.append(('tsu_'+an,part,Image.open(technique_dir/part['file']).convert('RGB')))
    if 'githu' in technique_manifest:
        for an in ('cast','child','parent'):
            a=technique_manifest['githu'][an]
            for part in a['parts']: technique_sheets.append(('githu_'+an,part,Image.open(technique_dir/part['file']).convert('RGB')))
    if 'nathu' in technique_manifest:
        for an in ('cast','child','parent'):
            a=technique_manifest['nathu'][an]
            for part in a['parts']: technique_sheets.append(('nathu_'+an,part,Image.open(technique_dir/part['file']).convert('RGB')))
    for variant in technique_manifest['healing']['variants']:
        for an in ('cast','effect'):
            a=variant[an]
            for part in a['parts']: technique_sheets.append((variant['name']+'_heal_'+an,part,Image.open(technique_dir/part['file']).convert('RGB')))
for im in [*battle_bgs,*battle_command_icons,*x_frames,*i_frames,*[x[2] for x in attack_sheets],*[x[2] for x in party_sheets],*[x[2] for x in party_attack_sheets],*[x[2] for x in technique_sheets]]:
    for c in im.getdata():
        if c==trans: continue
        if c not in colors: colors.append(c)
if len(colors)>187: raise ValueError(f'battle colors {len(colors)} exceed shared palette budget')
# Entries 80..84 are the runtime's text/cursor colors and 96..111 are the
# complete source window-palette bank installed by ps4_ui_colors().  Entries
# 192..255 are four source enemy palette lines used by generic decoded enemy
# bitmaps.  Never allocate battle background/party/BattleObj colors into any
# of those ownership ranges: doing so made only the pixels whose union indices
# landed in 96..111 change color as soon as the battle UI appeared.
available=[*range(80),*range(85,96),*range(112,192)]
lut={c:i for c,i in zip(colors,available)}
palette_slots=[(0,0,0)]*256
for c,i in lut.items(): palette_slots[i]=c
bpal=b''.join(struct.pack('<H',gba(c)) for c in palette_slots)
bgp=b''.join(bytes(lut[c] for c in im.getdata()) for im in battle_bgs)
def sprite_obj_bytes(im):
    # Pre-lower any legal 8bpp OBJ canvas into GBA 1D tile order. Each 8x8
    # 8bpp tile is 64 bytes (two 32-byte OBJ tile-number units).
    rgb=im.convert('RGB');w,h=rgb.size
    if w%8 or h%8: raise ValueError(f'OBJ canvas {w}x{h} is not tile aligned')
    out=bytearray()
    for ty in range(h//8):
        for tx in range(w//8):
            for y in range(8):
                for x in range(8):
                    c=rgb.getpixel((tx*8+x,ty*8+y))
                    out.append(0 if c==trans else lut[c])
    if len(out)!=w*h: raise AssertionError((len(out),w*h))
    return bytes(out)

def sheet_obj_bytes(sheet,canvas,frames):
    cw,ch=canvas
    if sheet.size!=(cw*frames,ch): raise ValueError((sheet.size,canvas,frames))
    return b''.join(sprite_obj_bytes(sheet.crop((i*cw,0,(i+1)*cw,ch))) for i in range(frames))

def macro_name(name): return re.sub(r'[^A-Z0-9]+','_',name.upper()).strip('_')
add('BATTLE_BG_PIXELS',bgp)
add('BATTLE_PALETTE',bpal)

# P128-style immutable-resource lowering for generic battle enemies.  The old
# runtime re-ran Nemesis on the source/base tile pools and Enigma on the mapping
# every time a generic enemy frame was composed.  Those inputs are immutable
# for a verified PSIV ROM, so decode them once here and let the GBA hot path use
# direct ROM->EWRAM DMA plus only the tiny authored animation tile patches.
#
# Metadata is eight little-endian u32 values per enemy:
# source_off, source_len, base_off, base_len, map_off, map_count, width, height.
enemy_native_meta=[]
enemy_native_source=bytearray()
enemy_native_base=bytearray()
enemy_native_map=bytearray()
enemy_native_pal=bytearray()
enemy_death_meta=[]
enemy_death_keys=bytearray()
def _align4(buf):
    while len(buf)&3: buf.append(0)
for enemy_id in range(153):
    rec=0x27F3AE+enemy_id*20
    source_ptr=int.from_bytes(rom[rec+6:rec+10],'big')
    base_ptr=int.from_bytes(rom[rec+10:rec+14],'big')
    map_ptr=int.from_bytes(rom[rec+14:rec+18],'big')
    width=rom[rec+18]*2
    height=rom[rec+19]
    source=nemesis_decompress(rom[source_ptr:])
    base=nemesis_decompress(rom[base_ptr:])
    mapping=enigma_decompress(rom[map_ptr:],0)
    need=width*height
    if len(mapping)<need:
        raise ValueError(f'enemy {enemy_id}: Enigma map {len(mapping)} < {need}')
    mapping=mapping[:need]
    _align4(enemy_native_source); source_off=len(enemy_native_source); enemy_native_source.extend(source)
    _align4(enemy_native_base); base_off=len(enemy_native_base); enemy_native_base.extend(base)
    _align4(enemy_native_map); map_off=len(enemy_native_map); enemy_native_map.extend(b''.join(struct.pack('<H',e) for e in mapping))
    enemy_native_meta.append((source_off,len(source),base_off,len(base),map_off,len(mapping),width,height))
    # Enemy_Dead ($C0BE) uses the source table at loc_C21C to assign each
    # mapped tile a group/rank.  loc_CECA contains 64 longword visibility
    # masks, indexed by dissolve phase*4+rank.  Lower the tile key table now so
    # runtime death frames only mask already-resolved native tile rows.
    death_rel=int.from_bytes(rom[0xC21C+enemy_id*2:0xC21E+enemy_id*2],'big',signed=True)
    death_tab=0xC21C+death_rel
    max_group=rom[death_tab]
    counts=list(rom[death_tab+1:death_tab+max_group+2])
    tile_count=(max((e&0x07FF) for e in mapping)+1) if mapping else 0
    keys=bytearray(tile_count);q=death_tab+max_group+3
    for group,count in enumerate(counts):
        for rank in range(count):
            tile=rom[q+rank]
            if tile<tile_count: keys[tile]=1+group*4+(rank&3)
        q+=count
    death_off=len(enemy_death_keys);enemy_death_keys.extend(keys)
    enemy_death_meta.append((death_off,len(keys)))
    # battle_palette_enemy used white/black in slots 1/2 followed by the exact
    # eleven source enemy colors.  Preconvert that 16-entry line as well.
    pal=[0]*16;pal[1]=gba(md_rgb(0x0EEE));pal[2]=0
    pp=0x27FFA2+enemy_id*22
    for i in range(11): pal[3+i]=gba(md_rgb(int.from_bytes(rom[pp+i*2:pp+i*2+2],'big')))
    # DF2's generic EnemyPalette entry is intentionally all zero. Its $83C
    # opening root loads the complete live body line from loc_317DC instead.
    # The native sprite compositor must use that authored line after opening.
    if enemy_id==0x83:
        pal=[gba(md_rgb(int.from_bytes(rom[0x317DC+i*2:0x317DE+i*2],'big'))) for i in range(16)]
    enemy_native_pal.extend(b''.join(struct.pack('<H',q) for q in pal))
meta_blob=b''.join(struct.pack('<8I',*m) for m in enemy_native_meta)
add('BATTLE_ENEMY_NATIVE_META',meta_blob)
add('BATTLE_ENEMY_NATIVE_SOURCE',bytes(enemy_native_source))
add('BATTLE_ENEMY_NATIVE_BASE',bytes(enemy_native_base))
add('BATTLE_ENEMY_NATIVE_MAP',bytes(enemy_native_map))
add('BATTLE_ENEMY_NATIVE_PALETTE',bytes(enemy_native_pal))
add('BATTLE_ENEMY_DEATH_META',b''.join(struct.pack('<2I',*m) for m in enemy_death_meta))
add('BATTLE_ENEMY_DEATH_KEYS',bytes(enemy_death_keys))
death_masks=[int.from_bytes(rom[0xCECA+i*4:0xCECE+i*4],'big') for i in range(64)]
add('BATTLE_ENEMY_DEATH_MASKS',b''.join(struct.pack('<I',v) for v in death_masks))
defs['BATTLE_ENEMY_NATIVE_COUNT']=len(enemy_native_meta)
defs['BATTLE_ENEMY_NATIVE_META_STRIDE']=32
defs['BATTLE_ENEMY_DEATH_META_STRIDE']=8
# The battle/menu red/blue cursor uses tiles $6E7/$6E8 from the same
# immutable WindowTiles Nemesis stream.  Lower these two tiles here so opening
# the first command window never triggers a runtime decompressor.
window_tiles=nemesis_decompress(rom[0x2A2B7E:])
cursor_off=(0x06E7-0x0680)*32
if len(window_tiles)<cursor_off+64:
    raise ValueError('WindowTiles stream does not contain cursor tiles $6E7/$6E8')
add('WINDOW_CURSOR_TILES',window_tiles[cursor_off:cursor_off+64])
add('BATTLE_COMMAND_ICON_PIXELS',b''.join(bytes(0 if c==trans else lut[c] for c in im.getdata()) for im in battle_command_icons))
defs['BATTLE_COMMAND_ICON_FRAMES']=len(battle_command_icons)
defs['BATTLE_COMMAND_ICON_W']=13
defs['BATTLE_COMMAND_ICON_H']=12
status_portraits=render_status_portraits_indexed()
add('STATUS_PORTRAIT_PIXELS',b''.join(status_portraits))
defs['STATUS_PORTRAIT_FRAMES']=len(status_portraits)
defs['STATUS_PORTRAIT_W']=80
defs['STATUS_PORTRAIT_H']=80
# Event battle 0's Xanafalgue/Igglanova idle carriers are authored on the
# same 320x224 battle plane as the background.  Keeping the diagnostic 64x64
# source canvases as 64x64 GBA OBJ made those enemies ~4/3 too wide and ~7/5
# too tall after the background was projected to 240x160.  Project their pixels
# once here using the viewport ratios/source-floor phase, then center
# the 48x46 visible result inside the existing 64x64 OBJ carrier.  Runtime VRAM
# size/DMA/OAM ownership therefore stays unchanged and pays zero scaling cost.
def _nearest_axis_map(src_n,dst_n):
    im=Image.new('I',(src_n,1))
    for x in range(src_n): im.putpixel((x,0),x)
    out=im.resize((dst_n,1),Image.Resampling.NEAREST)
    return [out.getpixel((x,0)) for x in range(dst_n)]
_EVENT_XREL=_nearest_axis_map(64,48)
# Vertical map needs a vertical probe; Pillow applies the same nearest contract
# per axis but keep it explicit so a future library change is caught by asserts.
_yprobe=Image.new('I',(1,224))
for _y in range(224): _yprobe.putpixel((0,_y),_y)
_BATTLE_YMAP=[_yprobe.resize((1,160),Image.Resampling.NEAREST).getpixel((0,y)) for y in range(160)]

# CP14: generic enemies share the full 320x224 battle plane with the
# background, so horizontal nearest-neighbor selection must use the *global*
# 320->240 phase. Pillow's nearest phase is not translation invariant: scaling
# an isolated 64px sprite to 48px can select different columns than cropping
# the same sprite from a full 320px plane and then scaling. Keep one exact
# global X sample map plus an inverse edge LUT. Runtime combines those with the
# source formation placement; no per-frame divide or search is needed.
_BATTLE_XMAP=_nearest_axis_map(320,240)
_BATTLE_XEDGE=[]
for _sx in range(321):
    _dx=0
    while _dx<240 and _BATTLE_XMAP[_dx]<_sx: _dx+=1
    _BATTLE_XEDGE.append(_dx)
assert len(_BATTLE_XMAP)==240 and len(_BATTLE_XEDGE)==321 and _BATTLE_XEDGE[-1]==240

# Vertically every generic mapping shares the source Plane-A exclusive bottom
# edge y=120. Compile only the local source-row choices for each enemy height;
# the first selected GBA scanline is stored in metadata. Metadata is four u32:
# y_off, nominal_projected_width, projected_height, projected_top_y.
_enemy_project_meta=[]
_enemy_project_y=bytearray()
for enemy_id,m in enumerate(enemy_native_meta):
    width,height=m[6],m[7]
    dst_w=(width*8*3)//4
    src_top=120-height*8
    rows=[(dy,sy-src_top) for dy,sy in enumerate(_BATTLE_YMAP) if src_top<=sy<120]
    if not rows: raise AssertionError((enemy_id,'y',src_top,height))
    dst_top=rows[0][0]
    if [dy for dy,_ in rows] != list(range(dst_top,dst_top+len(rows))):
        raise AssertionError((enemy_id,'noncontiguous-y'))
    yrel=[sy for _,sy in rows]
    if any(y<0 or y>=height*8 or y>255 for y in yrel): raise AssertionError((enemy_id,'yrel',height,max(yrel)))
    yoff=len(_enemy_project_y)
    _enemy_project_y.extend(yrel)
    _enemy_project_meta.append((yoff,dst_w,len(yrel),dst_top))
add('BATTLE_ENEMY_PROJECT_META',b''.join(struct.pack('<4I',*m) for m in _enemy_project_meta))
add('BATTLE_ENEMY_PROJECT_XMAP',b''.join(struct.pack('<H',x) for x in _BATTLE_XMAP))
add('BATTLE_ENEMY_PROJECT_XEDGE',bytes(_BATTLE_XEDGE))
add('BATTLE_ENEMY_PROJECT_Y',bytes(_enemy_project_y))
defs['BATTLE_ENEMY_PROJECT_META_STRIDE']=16

_EVENT_YREL=[s-56 for s in _BATTLE_YMAP if 56<=s<120]
assert len(_EVENT_XREL)==48 and len(_EVENT_YREL)==46
# Horizontal source width scales by the exact 3/4 viewport ratio.  Both side
# Xanafalgues share one authored frame/VRAM slot, so use one symmetric local
# 64->48 nearest sample phase for that shared carrier.  Vertically, preserve
# the full 224->160 scene phase because the source floor anchor is fixed.

def project_event_idle64(im):
    rgb=im.convert('RGB')
    if rgb.size!=(64,64): raise ValueError(rgb.size)
    canvas=Image.new('RGB',(64,64),trans)
    proj=Image.new('RGB',(48,46),trans)
    for dy,sy in enumerate(_EVENT_YREL):
        for dx,sx in enumerate(_EVENT_XREL):
            proj.putpixel((dx,dy),rgb.getpixel((sx,sy)))
    canvas.paste(proj,(8,9))
    return canvas

add('XANAF_ANIM_PIXELS',b''.join(sprite_obj_bytes(project_event_idle64(im)) for im in x_frames))
add('IGG_ANIM_PIXELS',b''.join(sprite_obj_bytes(project_event_idle64(im)) for im in i_frames))
add('XANAF_ANIM_SCHEDULE',(anim_dir/'xanaf_schedule.bin').read_bytes())
add('IGG_ANIM_SCHEDULE',(anim_dir/'igg_schedule.bin').read_bytes())

# Source BattleObj overlays. Each part is frame-major and can be DMA'd into
# one reusable Mode-4 OBJ scratch slot. Geometry/timing constants are emitted
# from the direct-ROM generator's manifest.
if attack_manifest:
    for seq in attack_manifest['sequences']:
        base=macro_name(seq['name'])
        defs[f'{base}_FRAMES']=len(seq['holds']);defs[f'{base}_LEAD_IN']=seq['lead_in'];defs[f'{base}_HIT_FRAME']=seq['hit_frame'];defs[f'{base}_SFX']=seq['sfx_id']
        for j,part in enumerate(seq['parts']):
            key=base if len(seq['parts'])==1 else f'{base}_PART{j+1}'
            sh=Image.open(attack_dir/part['file']).convert('RGB');add(f'{key}_PIXELS',sheet_obj_bytes(sh,part['canvas'],part['frames']))
            defs[f'{key}_W']=part['canvas'][0];defs[f'{key}_H']=part['canvas'][1];defs[f'{key}_CENTER_X']=part['center_offset'][0]+128;defs[f'{key}_CENTER_Y']=part['center_offset'][1]+128
        add(f'{base}_HOLDS',bytes(seq['holds']))
    micro=attack_manifest['micro_missile']
    for group in ('parent','burst','missile','impact'):
        parts=micro[group]['parts'];base=macro_name('jurafa_micro_'+group)
        for j,part in enumerate(parts):
            key=base if len(parts)==1 else f'{base}_PART{j+1}'
            sh=Image.open(attack_dir/part['file']).convert('RGB');add(f'{key}_PIXELS',sheet_obj_bytes(sh,part['canvas'],part['frames']))
            defs[f'{key}_FRAMES']=part['frames'];defs[f'{key}_W']=part['canvas'][0];defs[f'{key}_H']=part['canvas'][1];defs[f'{key}_CENTER_X']=part['center_offset'][0]+128;defs[f'{key}_CENTER_Y']=part['center_offset'][1]+128
    add('JURAFA_MICRO_BURST_HOLDS',bytes(micro['burst']['holds']))
    add('JURAFA_MICRO_IMPACT_HOLDS',bytes(micro['impact']['holds']))

# Source party physical BattleObj overlays (Alys slasher and Demi Impacter).
if party_attack_manifest:
    alys=party_attack_manifest['alys_slasher']
    defs['ALYS_SLASHER_BODY_WINDUP']=alys['body_windup']
    # Keep the 12 on-screen source trajectory coordinates (entry 13 is the off-screen terminal).
    rc=alys['right_coords'][:12];lc=alys['left_coords'][:12]
    add('ALYS_SLASHER_RIGHT_COORDS',b''.join(struct.pack('<HH',x&0xFFFF,y&0xFFFF) for x,y in rc))
    add('ALYS_SLASHER_LEFT_COORDS',b''.join(struct.pack('<HH',x&0xFFFF,y&0xFFFF) for x,y in lc))
    defs['ALYS_SLASHER_COORD_COUNT']=len(rc)
    for an,a in alys['animations'].items():
        base=macro_name('alys_slasher_'+an);defs[f'{base}_FRAMES']=len(a['holds'])
        for j,part in enumerate(a['parts']):
            key=base if len(a['parts'])==1 else f'{base}_PART{j+1}'
            sh=Image.open(party_attack_dir/part['file']).convert('RGB');add(f'{key}_PIXELS',sheet_obj_bytes(sh,part['canvas'],part['frames']))
            defs[f'{key}_W']=part['canvas'][0];defs[f'{key}_H']=part['canvas'][1];defs[f'{key}_CENTER_X']=part['center_offset'][0]+128;defs[f'{key}_CENTER_Y']=part['center_offset'][1]+128
        add(f'{base}_HOLDS',bytes(a['holds']))
    vortex=party_attack_manifest.get('vortex')
    if vortex:
        defs['SKILL_VORTEX_ID']=vortex['skill_id'];defs['VORTEX_ROOT_DELAY']=vortex['root_wait'];defs['VORTEX_FINISH_DELAY']=vortex['finish_wait']
        vc=vortex['right_coords'];add('VORTEX_RIGHT_COORDS',b''.join(struct.pack('<HH',x&0xFFFF,y&0xFFFF) for x,y in vc));defs['VORTEX_COORD_COUNT']=len(vc)
        for an,a in vortex['animations'].items():
            base=macro_name('vortex_'+an);defs[f'{base}_FRAMES']=len(a['holds'])
            for j,part in enumerate(a['parts']):
                key=base if len(a['parts'])==1 else f'{base}_PART{j+1}'
                sh=Image.open(party_attack_dir/part['file']).convert('RGB');add(f'{key}_PIXELS',sheet_obj_bytes(sh,part['canvas'],part['frames']))
                defs[f'{key}_W']=part['canvas'][0];defs[f'{key}_H']=part['canvas'][1];defs[f'{key}_CENTER_X']=part['center_offset'][0]+128;defs[f'{key}_CENTER_Y']=part['center_offset'][1]+128
            add(f'{base}_HOLDS',bytes(a['holds']))
    earth=party_attack_manifest.get('earth')
    if earth:
        defs['SKILL_EARTH_ID']=earth['skill_id'];defs['EARTH_ROOT_DELAY']=earth['root_wait'];defs['EARTH_SUCCESS_DELAY']=earth['success_wait']
        for an in ('main','success'):
            a=earth[an];base=macro_name('earth_'+an);defs[f'{base}_FRAMES']=len(a['holds'])
            for j,part in enumerate(a['parts']):
                key=base if len(a['parts'])==1 else f'{base}_PART{j+1}'
                sh=Image.open(party_attack_dir/part['file']).convert('RGB');add(f'{key}_PIXELS',sheet_obj_bytes(sh,part['canvas'],part['frames']))
                defs[f'{key}_W']=part['canvas'][0];defs[f'{key}_H']=part['canvas'][1];defs[f'{key}_CENTER_X']=part['center_offset'][0]+128;defs[f'{key}_CENTER_Y']=part['center_offset'][1]+128
            add(f'{base}_HOLDS',bytes(a['holds']))
    vision=party_attack_manifest.get('vision')
    if vision:
        defs['SKILL_VISION_ID']=vision['skill_id'];defs['VISION_ROOT_DELAY']=vision['root_wait'];a=vision['effect'];base=macro_name('vision_effect');defs[f'{base}_FRAMES']=len(a['holds'])
        for j,part in enumerate(a['parts']):
            key=base if len(a['parts'])==1 else f'{base}_PART{j+1}'
            sh=Image.open(party_attack_dir/part['file']).convert('RGB');add(f'{key}_PIXELS',sheet_obj_bytes(sh,part['canvas'],part['frames']))
            defs[f'{key}_W']=part['canvas'][0];defs[f'{key}_H']=part['canvas'][1];defs[f'{key}_CENTER_X']=part['center_offset'][0]+128;defs[f'{key}_CENTER_Y']=part['center_offset'][1]+128
        add(f'{base}_HOLDS',bytes(a['holds']))
    demi=party_attack_manifest['demi_impacter'];defs['DEMI_IMPACTER_ITEM_ID']=demi['item_id'];defs['DEMI_IMPACTER_BODY_WINDUP']=demi['body_windup'];defs['DEMI_IMPACTER_SHOT_DELAY']=demi['shot_delay'];defs['DEMI_IMPACTER_SHOT_FRAMES']=len(demi['holds'])
    for j,part in enumerate(demi['parts']):
        key='DEMI_IMPACTER_SHOT' if len(demi['parts'])==1 else f'DEMI_IMPACTER_SHOT_PART{j+1}'
        sh=Image.open(party_attack_dir/part['file']).convert('RGB');add(f'{key}_PIXELS',sheet_obj_bytes(sh,part['canvas'],part['frames']))
        defs[f'{key}_W']=part['canvas'][0];defs[f'{key}_H']=part['canvas'][1];defs[f'{key}_CENTER_X']=part['center_offset'][0]+128;defs[f'{key}_CENTER_Y']=part['center_offset'][1]+128
    add('DEMI_IMPACTER_SHOT_HOLDS',bytes(demi['holds']))

# Source technique BattleObj sheets (Foi + Res-family production slice).
def pack_tech_anim(name,a):
    base=macro_name(name);defs[f'{base}_FRAMES']=len(a['holds'])
    add(f'{base}_HOLDS',bytes(a['holds']))
    if 'hidden' in a:add(f'{base}_HIDDEN',bytes(1 if x else 0 for x in a['hidden']))
    for j,part in enumerate(a['parts']):
        key=base if len(a['parts'])==1 else f'{base}_PART{j+1}'
        sh=Image.open(technique_dir/part['file']).convert('RGB');add(f'{key}_PIXELS',sheet_obj_bytes(sh,part['canvas'],part['frames']))
        defs[f'{key}_W']=part['canvas'][0];defs[f'{key}_H']=part['canvas'][1];defs[f'{key}_CENTER_X']=part['center_offset'][0]+128;defs[f'{key}_CENTER_Y']=part['center_offset'][1]+128
if technique_manifest:
    foi=technique_manifest['foi'];defs['TECH_FOI_ID']=foi['tech_ids'][0];defs['TECH_GIFOI_ID']=foi['tech_ids'][1]
    for an in ('cast','orbit','release','impact'):pack_tech_anim('foi_'+an,foi[an])
    nafoi=technique_manifest.get('nafoi')
    if nafoi:
        defs['TECH_NAFOI_ID']=nafoi['tech_ids'][0]
        for an in ('cast','charge','attack'):pack_tech_anim('nafoi_'+an,nafoi[an])
    wat=technique_manifest.get('wat')
    if wat:
        defs['TECH_WAT_ID']=wat['tech_ids'][0]
        for an in ('cast','orbit','release','impact'):pack_tech_anim('wat_'+an,wat[an])
    tsu=technique_manifest.get('tsu')
    if tsu:
        defs['TECH_TSU_ID']=tsu['tech_ids'][0]
        for an in ('cast','attack'):pack_tech_anim('tsu_'+an,tsu[an])
    githu=technique_manifest.get('githu')
    if githu:
        defs['TECH_GITHU_ID']=githu['tech_ids'][0]
        for an in ('cast','child','parent'):pack_tech_anim('githu_'+an,githu[an])
    nathu=technique_manifest.get('nathu')
    if nathu:
        defs['TECH_NATHU_ID']=nathu['tech_ids'][0]
        for an in ('cast','child','parent'):pack_tech_anim('nathu_'+an,nathu[an])
    for variant in technique_manifest['healing']['variants']:
        base=macro_name(variant['name']);defs[f'TECH_{base}_ID']=variant['tech_ids'][0];defs[f'TECH_{base}_MULTI_ID']=variant['tech_ids'][1]
        pack_tech_anim(variant['name']+'_heal_cast',variant['cast']);pack_tech_anim(variant['name']+'_heal_effect',variant['effect'])

# Source rear-view party battle frames. The authored visible body is the 48x48
# crop inside the generator's 64x64 diagnostic canvas. Project that crop ONCE
# at build time to the same target geometry as the GBA battle scene: 3/4 X and
# 5/7 Y -> 36x34. Runtime therefore performs only a packed indexed blit; it no
# longer scales character pixels while the battle is running.
PARTY_TARGET_W=36
PARTY_TARGET_H=34
def party_frame_bytes(sheet,frames):
    if sheet.size!=(64*frames,64): raise ValueError((sheet.size,frames))
    out=bytearray()
    for i in range(frames):
        frame=sheet.crop((i*64+8,8,i*64+56,56)).convert('RGB')
        frame=frame.resize((PARTY_TARGET_W,PARTY_TARGET_H),Image.Resampling.NEAREST)
        # `lut` is the authoritative sparse shared battle-palette mapping.
        # UI-owned slots are never allocated to party art.
        out.extend(lut[c] for c in frame.getdata())
    return bytes(out)
if party_manifest:
    for name,ch in party_manifest['characters'].items():
        base=macro_name('party_'+name)
        poses=ch['poses'];sh=Image.open(party_dir/ch['pose_file']).convert('RGB')
        add(f'{base}_POSES_PIXELS',party_frame_bytes(sh,len(poses)))
        defs[f'{base}_POSES_FRAMES']=len(poses);defs[f'{base}_W']=PARTY_TARGET_W;defs[f'{base}_H']=PARTY_TARGET_H;defs[f'{base}_ID']=ch['id']
        for i,pn in enumerate(poses): defs[f'{base}_POSE_{macro_name(pn)}_INDEX']=i
        if 'attack' in ch:
            atk=ch['attack'];holds=atk['holds'];ash=Image.open(party_dir/atk['file']).convert('RGB')
            add(f'{base}_ATTACK_PIXELS',party_frame_bytes(ash,len(holds)))
            add(f'{base}_ATTACK_HOLDS',bytes(holds))
            defs[f'{base}_ATTACK_FRAMES']=len(holds)

# Fixed Mode-0 dialogue-window OBJ atlas.  The original 256x64 layout consumed
# all 256 reserved tiles even though PSIV's source dialogue window only occupies
# about 204x34 GBA pixels.  Pack the main window into 160 tiles, retain a
# 64x32 choice window at tiles 192..223, and leave tiles 160..184 for the
# source F4 portrait.  This keeps every dialogue asset above tile 768 while
# making portraits coexist with field/NPC OBJ art rather than borrowing it.
import re
_window_hdr=(ROOT/'gba'/'ps4_window_assets.h').read_text()
_window_arr=re.search(r'ps4_window_tiles_4bpp\[128\]\s*=\s*\{(.*?)\};',_window_hdr,re.S)
if not _window_arr: raise RuntimeError('ps4_window_assets.h missing generated window tiles')
_window_bytes=bytes(int(x,16) for x in re.findall(r'0x([0-9A-Fa-f]{2})',_window_arr.group(1)))
if len(_window_bytes)!=128: raise RuntimeError(f'window tile bytes {len(_window_bytes)}')
_font_arr=re.search(r'ps4_window_font_4bpp\[2784\]\s*=\s*\{(.*?)\};',_window_hdr,re.S)
if not _font_arr: raise RuntimeError('ps4_window_assets.h missing generated window font')
_font_bytes=bytes(int(x,16) for x in re.findall(r'0x([0-9A-Fa-f]{2})',_font_arr.group(1)))
if len(_font_bytes)!=87*32: raise RuntimeError(f'window font bytes {len(_font_bytes)}')

# Battle UI presentation assets are projected once at build time. Border tiles
# preserve source palette nibbles; glyphs are emitted as a 6x8 binary mask so
# runtime can tint them without scaling or re-reading packed source art.
def _tile_ci(raw,x,y):
    q=raw[y*4+(x>>1)]
    return (q&15) if (x&1) else (q>>4)
def _project_tile(raw,w=6,h=6):
    out=bytearray(w*h)
    for y in range(h):
        sy=(y*8)//h
        for x in range(w):
            sx=(x*8)//w
            out[y*w+x]=_tile_ci(raw,sx,sy)
    return bytes(out)
add('BATTLE_WINDOW_TILE6_PIXELS',b''.join(_project_tile(_window_bytes[i*32:(i+1)*32],6,6) for i in range(4)))
defs['BATTLE_WINDOW_TILE6_W']=6;defs['BATTLE_WINDOW_TILE6_H']=6;defs['BATTLE_WINDOW_TILE6_COUNT']=4
# Battle windows project each 8x8 source text cell into the same 6x~6 GBA
# cell used by the projected Window_Draw geometry. v0.9.10.4 kept 7x8 literal
# strokes: readable, but wider/taller than the projected cell, so battle text
# could escape or overlap its source-shaped chrome. Preserve strokes while
# preserving geometry: OR-fold the two interior column/row pairs that a naive
# nearest-neighbour 8->6 projection would drop, retaining both outer edges.
# This is immutable build-time lowering; the runtime performs no font scalar.
_font6_bins=((0,),(1,2),(3,),(4,),(5,6),(7,))
font6=bytearray()
for gi in range(87):
    raw=_font_bytes[gi*32:(gi+1)*32]
    for rows in _font6_bins:
        for cols in _font6_bins:
            font6.append(1 if any(_tile_ci(raw,x,y)==15 for y in rows for x in cols) else 0)
add('BATTLE_FONT6X6_MASK',bytes(font6))
defs['BATTLE_FONT6X6_W']=6;defs['BATTLE_FONT6X6_H']=6;defs['BATTLE_FONT6X6_COUNT']=87
# Red/blue cursor tiles ($6E7/$6E8) were already lowered from WindowTiles.
# Project them once to 6x6 and retain source color nibbles for runtime tinting.
cursor6=bytearray()
for ti in range(2):
    raw=window_tiles[cursor_off+ti*32:cursor_off+(ti+1)*32]
    cursor6.extend(_project_tile(raw,6,6))
add('BATTLE_CURSOR6_PIXELS',bytes(cursor6))
defs['BATTLE_CURSOR6_W']=6;defs['BATTLE_CURSOR6_H']=6;defs['BATTLE_CURSOR6_FRAMES']=2
# Local OBJ palette mapping.  Palette line 15 is reserved by the runtime.
# 0 transparent, 1 source interior($E), 2 source text($F),
# 3 source secondary edge($3), 4 source primary edge($2).
_dialogue_cmap={14:1,15:2,3:3,2:4}
dialogue=bytearray(8192)
def _set_nibble(buf,off,x,c):
    v=buf[off]
    if x&1: v=(v&0x0F)|((c&15)<<4)
    else: v=(v&0xF0)|(c&15)
    buf[off]=v
def dialogue_pixel(x,y,c):
    # Main 256x40 logical canvas. y=0..31 uses four 64x32 sprites; the final
    # 8 scanlines use eight 32x8 sprites.  Tiles 0..159 are contiguous.
    if not (0<=x<256 and 0<=y<40): return
    if y<32:
        seg=x>>6; lx=x&63; ly=y
        tile=seg*32+(ly>>3)*8+(lx>>3)
    else:
        seg=x>>5; lx=x&31; ly=y-32
        tile=128+seg*4+(lx>>3)
    off=tile*32+(ly&7)*4+((lx&7)>>1)
    _set_nibble(dialogue,off,lx,c)
def choice_pixel(x,y,c):
    # Source Yes/No window lives in a dedicated 64x32 sprite at tile 192.
    if not (0<=x<64 and 0<=y<32): return
    tile=192+(y>>3)*8+(x>>3)
    off=tile*32+(y&7)*4+((x&7)>>1)
    _set_nibble(dialogue,off,x,c)
def source_window_px(tile,x,y,hflip=False,vflip=False):
    if hflip: x=7-x
    if vflip: y=7-y
    q=_window_bytes[tile*32+y*4+(x>>1)]
    return (q&15) if (x&1) else (q>>4)
def make_source_window(wt,ht):
    out=bytearray(wt*8*ht*8)
    def put(x,y,c): out[y*(wt*8)+x]=c
    def stamp(tile,sx,sy,hflip=False,vflip=False):
        for yy in range(8):
            for xx in range(8):
                ci=source_window_px(tile,xx,yy,hflip,vflip)
                put(sx+xx,sy+yy,_dialogue_cmap.get(ci,1))
    for ty in range(1,ht-1):
        for tx in range(1,wt-1): stamp(0,tx*8,ty*8)
    for tx in range(1,wt-1):
        stamp(2,tx*8,0); stamp(2,tx*8,(ht-1)*8,vflip=True)
    for ty in range(1,ht-1):
        stamp(3,0,ty*8); stamp(3,(wt-1)*8,ty*8,hflip=True)
    stamp(1,0,0); stamp(1,(wt-1)*8,0,hflip=True)
    stamp(1,0,(ht-1)*8,vflip=True); stamp(1,(wt-1)*8,(ht-1)*8,hflip=True,vflip=True)
    return out
# Event_RunDialogue opens WinGroup_Dialogue entry 1.  US source record
# width=$21,height=$05,x=$03,y=$14; Window_Draw adds one cell -> 34x6 tiles.
# Source screen top-left (24,160) maps to GBA (18,114).  The packed main OAM
# canvas starts at y=112, hence local (18,2). TextBufferToPlane source (32,168)
# maps to GBA (24,120), hence runtime glyph local y=8.
SWT,SHT=34,6
src_dialogue=make_source_window(SWT,SHT)
DW=(SWT*8*3)//4; DH=(SHT*8*5)//7; WX=18; WY=2
for dy in range(DH):
    sy=(dy*7)//5
    for dx in range(DW):
        sx=(dx*4)//3
        dialogue_pixel(WX+dx,WY+dy,src_dialogue[sy*(SWT*8)+sx])
# RunText $F5 uses WinGroup_Dialogue entry 3: source 7x5 tiles after the
# Window_Draw +1 expansion at source (216,112).  Global scaling yields a 42x28
# window at GBA (162,80), represented by one 64x32 OBJ at (160,80).
CWT,CHT=7,5
src_choice=make_source_window(CWT,CHT)
CDW=(CWT*8*3)//4; CDH=(CHT*8*5)//7
for dy in range(CDH):
    sy=(dy*7)//5
    for dx in range(CDW):
        sx=(dx*4)//3
        choice_pixel(2+dx,dy,src_choice[sy*(CWT*8)+sx])
add('DIALOGUE_TEMPLATE',bytes(dialogue))

# RunText $F4 dialogue portraits. DialoguePortraitArtPtrs is the authoritative
# 40-entry table at ROM $6A4B0 (entry 0 means destroy portrait).  Every source
# portrait is exactly 36 Nemesis tiles arranged by loc_2A2B36 as a 6x6 48x48
# image.  Pre-decode and globally scale each to 36x34, then pack it into the
# runtime's four-OBJ 25-tile portrait atlas.  Runtime therefore never performs
# Nemesis decompression during text and only DMA-copies 800 bytes per change.
def portrait_frame(pid):
    ptr=int.from_bytes(rom[0x6A4B0+pid*4:0x6A4B4+pid*4],'big')
    art=nemesis_decompress(rom[ptr:])
    if len(art)!=36*32:
        raise ValueError(f'dialogue portrait {pid}: expected 1152 bytes, got {len(art)}')
    src=bytearray(48*48)
    for y in range(48):
        for x in range(48):
            tile=(y>>3)*6+(x>>3); q=art[tile*32+(y&7)*4+((x&7)>>1)]
            src[y*48+x]=(q&15) if (x&1) else (q>>4)
    out=bytearray(25*32)
    for dy in range(34):
        sy=(dy*7)//5
        for dx in range(36):
            sx=(dx*4)//3; c=src[sy*48+sx]
            if dx<32 and dy<32: tile=(dy>>3)*4+(dx>>3); lx=dx; ly=dy
            elif dx>=32 and dy<32: tile=16+(dy>>3); lx=dx-32; ly=dy
            elif dx<32: tile=20+(dx>>3); lx=dx; ly=dy-32
            else: tile=24; lx=dx-32; ly=dy-32
            off=tile*32+(ly&7)*4+((lx&7)>>1)
            _set_nibble(out,off,lx,c)
    return bytes(out)
_portraits=b''.join(portrait_frame(i) for i in range(1,40))
add('DIALOGUE_PORTRAIT_PIXELS',_portraits)
defs['DIALOGUE_PORTRAIT_COUNT']=39
defs['DIALOGUE_PORTRAIT_FRAME_SIZE']=25*32

defs['XANAF_ANIM_STATES']=len(x_frames)
defs['IGG_ANIM_STATES']=len(i_frames)
defs['XANAF_ANIM_PERIOD']=(anim_dir/'xanaf_schedule.bin').stat().st_size
defs['IGG_ANIM_PERIOD']=(anim_dir/'igg_schedule.bin').stat().st_size
defs['BATTLE_BG_COUNT']=len(battle_bgs)
defs['BATTLE_BG_FRAME_SIZE']=240*160
OUT.write_bytes(blob)

lines=['#ifndef PS4_PRECOMP_ASSETS_H','#define PS4_PRECOMP_ASSETS_H','#include <stdint.h>',
       'extern const uint8_t ps4_precomp_assets[];',f'#define PS4_PRECOMP_SIZE {len(blob)}u']
for k,v in defs.items():
    is_value=k.endswith(('STATES','PERIOD','FRAMES','LEAD_IN','HIT_FRAME','SFX','_W','_H','CENTER_X','CENTER_Y','_INDEX','_ID','WINDUP','DELAY','COORD_COUNT','BG_COUNT','FRAME_SIZE','_COUNT','_STRIDE'))
    suffix='' if is_value else '_OFF'
    lines.append(f'#define PS4_PRECOMP_{k}{suffix} {v}u')
lines += ['#define PS4_PRECOMP_PRINCIPAL_PIXELS_SIZE (240u*160u)',
          '#define PS4_PRECOMP_PALETTE_SIZE 512u',
          '#define PS4_PRECOMP_BATTLE_BG_PIXELS_SIZE (PS4_PRECOMP_BATTLE_BG_COUNT*PS4_PRECOMP_BATTLE_BG_FRAME_SIZE)',
          '#define PS4_PRECOMP_ENEMY_PIXELS_SIZE (64u*64u)',
          '#endif']
HDR.write_text('\n'.join(lines)+'\n')
print(f'wrote {OUT} {len(blob)} bytes, battle colors={len(colors)}')
for k,v in defs.items(): print(f'{k}: {v}')
