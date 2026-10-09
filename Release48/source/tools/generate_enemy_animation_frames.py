#!/usr/bin/env python3
from __future__ import annotations
import argparse, hashlib, sys
from pathlib import Path
from PIL import Image
from md_compression import nemesis_decompress,enigma_decompress

ROOT=Path(__file__).resolve().parent.parent
EXPECTED_SHA='511f35cc11f88316f8b8940e28ab298bd75a4da193672a80172884d6eb913b6a'
TRANS=(248,0,248)

class Rom:
    def __init__(self,data:bytes): self.data=data; self.logical_size=len(data)
    def read(self,off:int,n:int)->bytes:
        if off<0 or off+n>len(self.data): raise ValueError(f'ROM read ${off:06X}+{n} out of range')
        return self.data[off:off+n]
    def tail(self,off:int)->bytes: return self.read(off,len(self.data)-off)

def md_colors(words):
    return [((((w>>1)&7)*255//7),(((w>>5)&7)*255//7),(((w>>9)&7)*255//7)) for w in words]

def words(data): return [int.from_bytes(data[i:i+2],'big') for i in range(0,len(data),2)]

def enemy_record(rom:Rom,eid:int):
    r=rom.read(0x27F3AE+eid*20,20)
    ptrs=tuple(int.from_bytes(r[o:o+4],'big') for o in (2,6,10,14))
    if any(p>=rom.logical_size for p in ptrs): raise ValueError(f'bad enemy ${eid:02X} pointer')
    return (*ptrs,r[18],r[19])

def enemy_palette(rom:Rom,eid:int):
    src=words(rom.read(0x27FFA2+eid*22,22))
    return md_colors([0x0000,0x0EEE,0x0000,*src,0x0000,0x0000])

def components(rom:Rom,eid:int):
    table=0x10FCE
    rel=int.from_bytes(rom.read(table+eid*2,2),'big',signed=True)
    p=table+rel
    n=int.from_bytes(rom.read(p,2),'big')+1;p+=2
    out=[]
    for _ in range(n):
        e=rom.read(p,6); dest,count=e[0],e[1]
        indir=int.from_bytes(e[2:6],'big')&0x7fffffff
        seqptr=int.from_bytes(rom.read(indir,4),'big')
        first=rom.read(seqptr,1)[0]
        if first&0x80:
            frames=first&0x7f
            delays=list(rom.read(seqptr+1,frames)); offsets=list(rom.read(seqptr+1+frames,frames))
        else:
            frames=first; delay=rom.read(seqptr+1,1)[0]
            delays=[delay]*frames; offsets=list(rom.read(seqptr+2,frames))
        out.append((dest,count,offsets,delays)); p+=6
    return out

def assembled_enemy_tiles(rom:Rom,eid:int):
    _,source_ptr,base_ptr,map_ptr,half_w,h=enemy_record(rom,eid)
    source=nemesis_decompress(rom.tail(source_ptr)); base=bytearray(nemesis_decompress(rom.tail(base_ptr)))
    for dest,count,offsets,delays in components(rom,eid):
        # The initial static frame uses the first source selector exactly as
        # Battle_EnemySpritesLoad/loc_7CCC does before animation advances.
        src=offsets[0]; a=src*32;b=a+count*32;d=dest*32;e=d+count*32
        if b>len(source): raise ValueError('enemy mapping exceeds source art')
        if e>len(base): base.extend(b'\0'*(e-len(base)))
        base[d:e]=source[a:b]
    return bytes(base),map_ptr,half_w,h

def render_enemy_frame(tiles:bytes,cells:list[int],w:int,h:int,colors):
    if len(cells)!=w*h: raise ValueError('enemy map geometry mismatch')
    tc=len(tiles)//32; im=Image.new('P',(w*8,h*8),0)
    pal=[ch for c in colors for ch in c]; im.putpalette(pal+[0]*(768-len(pal)))
    for ty in range(h):
        for tx in range(w):
            cell=cells[ty*w+tx]; ti=cell&0x7ff
            if ti>=tc: raise ValueError(f'enemy map tile {ti} >= {tc}')
            tile=tiles[ti*32:(ti+1)*32]; hf=bool(cell&0x0800); vf=bool(cell&0x1000)
            for py in range(8):
                sy=7-py if vf else py
                for px in range(8):
                    sx=7-px if hf else px; q=tile[sy*4+(sx>>1)]; ci=(q>>4) if not (sx&1) else (q&15)
                    if ci: im.putpixel((tx*8+px,ty*8+py),ci)
    return im

def compact_sprite_palette(im:Image.Image):
    pal=im.getpalette(); opaque=sorted(set(im.getdata())-{0})
    if len(opaque)>15: raise ValueError(f'{len(opaque)} opaque colors')
    remap={0:0};remap.update({v:i+1 for i,v in enumerate(opaque)})
    used=[0]+opaque; out=Image.new('P',im.size,0)
    out.putpalette([ch for src in used for ch in pal[src*3:src*3+3]]+[0]*(768-len(used)*3))
    out.putdata([remap[v] for v in im.getdata()]); out.info['transparency']=0
    return out

def separate_transparent_palette_color(im:Image.Image):
    pal=im.getpalette(); used=sorted(i for _,i in im.getcolors(maxcolors=im.width*im.height))
    opaque={tuple(pal[i*3:i*3+3]) for i in used if i!=0}
    if TRANS in opaque: raise ValueError('reserved transparent sentinel used opaquely')
    pal[:3]=TRANS; im.putpalette(pal); return im

def simulate(comps,limit=10000):
    idx=[0]*len(comps); timers=[0]*len(comps); init=(tuple(idx),tuple(timers)); seen={init:0}; states=[]
    for frame in range(limit):
        states.append(tuple(idx))
        for j,c in enumerate(comps):
            timers[j]-=1
            if timers[j]<0:
                idx[j]=(idx[j]+1)%len(c[2]); timers[j]=c[3][idx[j]]
        st=(tuple(idx),tuple(timers))
        if st in seen: return states[seen[st]:]
        seen[st]=frame+1
    raise RuntimeError('animation period not found')

def frame(rom:Rom,eid:int,state):
    _,source_ptr,base_ptr,map_ptr,half_w,h=enemy_record(rom,eid)
    source=nemesis_decompress(rom.tail(source_ptr)); base0,_,_,_=assembled_enemy_tiles(rom,eid); tiles=bytearray(base0)
    comps=components(rom,eid)
    for ci,idx in enumerate(state):
        dest,count,offs,_=comps[ci];src=offs[idx];a=src*32;b=a+count*32;d=dest*32;e=d+count*32
        if e>len(tiles): tiles.extend(b'\0'*(e-len(tiles)))
        tiles[d:e]=source[a:b]
    cells=enigma_decompress(rom.tail(map_ptr))[:half_w*2*h]
    src_im=render_enemy_frame(bytes(tiles),cells,half_w*2,h,enemy_palette(rom,eid))
    ratio=min(64/src_im.width,64/src_im.height,1.0)
    scaled=src_im.resize((max(1,round(src_im.width*ratio)),max(1,round(src_im.height*ratio))),Image.Resampling.NEAREST)
    spr=Image.new('P',(64,64),0);spr.putpalette(src_im.getpalette());spr.paste(scaled,((64-scaled.width)//2,64-scaled.height))
    return separate_transparent_palette_color(compact_sprite_palette(spr)).convert('RGB')

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--rom',required=True,type=Path);a=ap.parse_args()
    data=a.rom.read_bytes();digest=hashlib.sha256(data).hexdigest()
    if len(data)!=0x300000 or digest!=EXPECTED_SHA: raise SystemExit(f'wrong PSIV source: size={len(data)} sha256={digest}')
    rom=Rom(data);out=ROOT/'generated_precomp'/'enemy_anim';out.mkdir(parents=True,exist_ok=True)
    for old in out.glob('*'): old.unlink()
    for name,eid in [('xanaf',9),('igg',12)]:
        comps=components(rom,eid);timeline=simulate(comps);unique=[]
        for st in timeline:
            if st not in unique: unique.append(st)
        idxmap={st:i for i,st in enumerate(unique)};sched=[idxmap[st] for st in timeline]
        for i,st in enumerate(unique): frame(rom,eid,st).save(out/f'{name}_{i:02d}.png')
        (out/f'{name}_states.txt').write_text('\n'.join(','.join(map(str,s)) for s in unique)+'\n')
        (out/f'{name}_schedule.bin').write_bytes(bytes(sched))
        print(name,'components',len(comps),'states',len(unique),'period',len(sched),unique)

if __name__=='__main__': main()
