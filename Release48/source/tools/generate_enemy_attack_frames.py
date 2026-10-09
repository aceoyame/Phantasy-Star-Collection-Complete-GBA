#!/usr/bin/env python3
"""Generate PSIV enemy BattleObj overlays for the freestanding static port.

All graphics are decoded directly from the verified US PSIV ROM.  No retired framework
asset or runtime code is consumed.  Output RGB PNG sheets + a JSON descriptor
are subsequently packed by generate_precompiled_visuals.py for the static GBA
runtime.
"""
from __future__ import annotations
import argparse, hashlib, json
from dataclasses import dataclass
from pathlib import Path
from PIL import Image
from md_compression import nemesis_decompress
from generate_enemy_animation_frames import Rom, enemy_record

ROOT=Path(__file__).resolve().parent.parent
EXPECTED_SHA='511f35cc11f88316f8b8940e28ab298bd75a4da193672a80172884d6eb913b6a'
TRANS=(248,0,248)

# Source addresses from the byte-perfect PSIV listing.
A={
 'sl':0x11B3E,'w0':0x11B66,'w1':0x11B8E,'w2':0x11BB6,'w3':0x11B7A,'w4':0x11BA2,'w5':0x11BCA,
 'jf0':0x378C6,'jf1':0x378CD,'jf2':0x378D4,'jf3':0x378E1,'jf4':0x378EE,
 'mp':0x37C1E,'mb0':0x37C25,'mb1':0x37C32,'mb2':0x37C3F,'mb3':0x37C52,'mb4':0x37C65,'mb5':0x37C78,'mm':0x37C8B,
 'mi0':0x25AF7E,'mi1':0x25AF86,'mi2':0x25AFA0,'mi3':0x25AFBA,'mi4':0x25AFD4,'mi5':0x25AFEE,
 'sa':0x20E3FE,'wa':0x20E534,'jfa':0x2133AC,'ma0':0x213766,'ma1':0x213802,'ma2':0x2139B4,'ma3':0x244054,
}

def md_color(w:int):
    return (((w>>1)&7)*255//7, ((w>>5)&7)*255//7, ((w>>9)&7)*255//7)

def palette_words(rom:Rom,eid:int):
    src=[int.from_bytes(rom.read(0x27FFA2+eid*22+i,2),'big') for i in range(0,22,2)]
    return [0x0000,0x0EEE,0x0000,*src,0x0000,0x0000]

def palette(rom:Rom,eid:int,overrides=()):
    w=palette_words(rom,eid)
    for i,v in overrides:w[i]=v
    return [md_color(x) for x in w]

def map_len(rom:Rom,off:int)->int:
    n=rom.read(off,1)[0]+1
    return 1+n*6

def mapping(rom:Rom,off:int)->bytes:
    return rom.read(off,map_len(rom,off))

def records(m:bytes):
    n=m[0]+1;out=[]
    for i in range(n):
        r=m[1+i*6:7+i*6]
        y=r[0]-256 if r[0]&128 else r[0]
        x=r[4]-256 if r[4]&128 else r[4]
        size=r[1];w=((size>>2)&3)+1;h=(size&3)+1
        props=(r[2]<<8)|r[3]
        out.append((x,y,w,h,props))
    return out

def bounds(m:bytes):
    r=records(m)
    return min(x for x,_,_,_,_ in r),min(y for _,y,_,_,_ in r),max(x+w*8 for x,_,w,_,_ in r),max(y+h*8 for _,y,_,h,_ in r)

def render(art:bytes,m:bytes,cols,reg):
    x0,y0,x1,y1=reg
    im=Image.new('RGB',(x1-x0,y1-y0),TRANS)
    tc=len(art)//32
    for x,y,w,h,p in records(m):
        bt=p&0x7FF;hf=bool(p&0x0800);vf=bool(p&0x1000)
        for tx in range(w):
            for ty in range(h):
                stx=w-1-tx if hf else tx; sty=h-1-ty if vf else ty
                ti=bt+stx*h+sty
                if ti>=tc:raise ValueError(f'mapping tile {ti} exceeds {tc}')
                tile=art[ti*32:(ti+1)*32]
                for py in range(8):
                    sy=7-py if vf else py;wy=y+ty*8+py
                    if wy<y0 or wy>=y1:continue
                    for px in range(8):
                        sx=7-px if hf else px;wx=x+tx*8+px
                        if wx<x0 or wx>=x1:continue
                        q=tile[sy*4+(sx>>1)];ci=(q>>4) if not(sx&1) else (q&15)
                        if ci:im.putpixel((wx-x0,wy-y0),cols[ci])
    return im

LEGAL=((8,8),(16,8),(32,8),(16,16),(32,16),(64,32),(8,16),(8,32),(16,32),(32,32),(32,64),(64,64))
def legal(w,h):
    q=sorted((a*b,a,b) for a,b in LEGAL if a>=w and b>=h)
    if not q:raise ValueError((w,h))
    return q[0][1],q[0][2]

def split(reg):
    x0,y0,x1,y1=reg
    return [(x,y,min(x+64,x1),min(y+64,y1)) for y in range(y0,y1,64) for x in range(x0,x1,64)]

def sheet(frames,reg,path):
    x0,y0,x1,y1=reg;sw,sh=x1-x0,y1-y0;cw,ch=legal(sw,sh)
    out=Image.new('RGB',(cw*len(frames),ch),TRANS);px=(cw-sw)//2;py=(ch-sh)//2
    for i,im in enumerate(frames):out.paste(im,(i*cw+px,py))
    out.save(path)
    return {'file':path.name,'canvas':[cw,ch],'source_region':[x0,y0,x1,y1],'center_offset':[(x0+x1)//2,(y0+y1)//2],'frames':len(frames)}

def art_enemy_action(rom,eid):
    _,src,_,_,_,_=enemy_record(rom,eid);return nemesis_decompress(rom.tail(src))

def art(rom,offs):return b''.join(nemesis_decompress(rom.tail(x)) for x in offs)

@dataclass
class Seq:
    name:str;enemy:int;ability:int;obj:int;art_offs:tuple[int,...]|None;maps:tuple[int,...];holds:tuple[int,...];lead:int;sfx:int;hit:int;overrides:tuple[tuple[int,int],...]=()

# Do not synthesize a Xanafalgue attack from $119E6-$11A1C. Those mappings
# are proven by the PSIV source to belong to BattleObj_IgglanovaAtk2. Until
# BattleObj_XanafalgueAtk itself is lowered, generic Xanafalgue attacks use
# the enemy's own EnemySprites_Animate state instead of a mislabeled overlay.
SEQS=(
 Seq('sweeper_laser',8,4,0x6C,(A['sa'],),(A['sl'],)*8,(2,)*8,0,0xC5,0,((9,0x086E),)),
 Seq('sweeper_lightning',8,5,0x70,(A['wa'],),(A['w0'],A['w1'],A['w2'],A['w3'],A['w4'],A['w1'],A['w0'],A['w4'],A['w5'],A['w2']),(2,)*10,0,0xCE,0,((8,0x0EC8),(9,0x0E82))),
 Seq('jurafa_flame_launch',0x1D,0x0F,0xE4,(A['jfa'],),(A['jf0'],A['jf1'],A['jf2'],A['jf3'],A['jf2'],A['jf3'],A['jf4']),(2,2,10,6,10,8,5),20,0xC6,2,((10,0x06CE),(11,0x006E),(12,0x002C))),
)

def emit_parts(out,rom,name,artdata,maps,cols):
    bs=[bounds(m) for m in maps];u=(min(x[0] for x in bs),min(x[1] for x in bs),max(x[2] for x in bs),max(x[3] for x in bs));parts=[]
    for j,r in enumerate(split(u)):
        ims=[render(artdata,m,cols,r) for m in maps]
        suffix='' if len(split(u))==1 else ('_top' if j==0 else f'_part{j+1}')
        parts.append(sheet(ims,r,out/f'{name}{suffix}.png'))
    return parts,list(u),[list(x) for x in bs]

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--rom',required=True,type=Path);a=ap.parse_args()
    raw=a.rom.read_bytes();sha=hashlib.sha256(raw).hexdigest()
    if len(raw)!=0x300000 or sha!=EXPECTED_SHA:raise SystemExit(f'wrong PSIV ROM {len(raw)} {sha}')
    rom=Rom(raw);out=ROOT/'generated_precomp'/'enemy_attack';out.mkdir(parents=True,exist_ok=True)
    for p in out.glob('*'):p.unlink()
    desc={'source_rom_sha256':sha,'sequences':[]}
    for s in SEQS:
        ad=art_enemy_action(rom,s.enemy) if s.art_offs is None else art(rom,s.art_offs)
        ms=[mapping(rom,x) for x in s.maps];cols=palette(rom,s.enemy,s.overrides)
        parts,u,bs=emit_parts(out,rom,s.name,ad,ms,cols)
        desc['sequences'].append({'name':s.name,'enemy_id':s.enemy,'ability_id':s.ability,'object_id':s.obj,'holds':list(s.holds),'lead_in':s.lead,'sfx_id':s.sfx,'hit_frame':s.hit,'parts':parts,'source_union':u,'frame_bounds':bs})
        print(s.name,len(ms),'frames',parts)
    # Jurafa Micro Missile uses four PLC9 streams in one contiguous source VRAM pool.
    chunks=[nemesis_decompress(rom.tail(A[k])) for k in ('ma0','ma1','ma2','ma3')];pool=b''.join(chunks)
    cols=palette(rom,0x1D,((10,0x06CE),(11,0x028E),(12,0x024C),(13,0x0008)))
    parent_maps=[mapping(rom,A['mp'])]
    burst_maps=[mapping(rom,A[k]) for k in ('mb0','mb1','mb2','mb3','mb4','mb5')]
    missile_maps=[mapping(rom,A['mm'])]
    ipayload=[rom.read(A[k],map_len(rom,A[k])+1) for k in ('mi0','mi1','mi2','mi3','mi4','mi5')]
    # These records begin with Battle_AnimateSprite duration, followed by mapping.
    impact_holds=[x[0] for x in ipayload];impact_maps=[x[1:] for x in ipayload]
    if sum(len(c) for c in chunks[:3])!=0x7C0:raise ValueError('PLC9 impact bank is not +$7C0')
    pp,pu,_=emit_parts(out,rom,'jurafa_micro_parent',pool,parent_maps,cols)
    bp,bu,_=emit_parts(out,rom,'jurafa_micro_burst',pool,burst_maps,cols)
    mp,mu,_=emit_parts(out,rom,'jurafa_micro_missile',pool,missile_maps,cols)
    ip,iu,_=emit_parts(out,rom,'jurafa_micro_impact',chunks[3],impact_maps,cols)
    desc['micro_missile']={
      'enemy_id':0x1D,'ability_id':0x0E,'object_id':0xE8,'plc_tile_counts':[len(c)//32 for c in chunks],
      'parent':{'parts':pp,'source_union':pu,'approach_updates':15,'approach_x_step':-1,'launcher_count':2,'center_x_offsets':[-28,28],'first_burst_update':23},
      'burst':{'parts':bp,'source_union':bu,'holds':[4,4,2,2,3,4],'spawn_updates':[23,33,44,54],'variant_flags':[0,1,0,1],'variant_x_adjust':6,'center_x_offsets':[-40,40],'sfx_id':0xD0},
      'missile':{'parts':mp,'source_union':mu,'spawn_update':75,'delays':[0,10,20,30],'x_positions':[0xB0,0xD0,0xF0,0x110],'mirrored_x_positions':[0x190,0x170,0x150,0x130],'y_start':0x68,'y_step':0x18,'priority_y':0xA8,'hit_y':0x110,'initial_sfx_id':0xB7,'impact_sfx_id':0xBE,'target_pair_table':[4,4,4,2,2,2,1,2,1,3,3,3,3,5,5,5]},
      'impact':{'parts':ip,'source_union':iu,'holds':impact_holds,'plc_byte_offset':0x7C0}
    }
    (out/'manifest.json').write_text(json.dumps(desc,indent=2)+'\n')
    print('wrote',out/'manifest.json')
if __name__=='__main__':main()
