#!/usr/bin/env python3
"""Generate PSIV source SFX descriptors for the existing host chip sequencer.
The descriptors are inert in the GBA build unless an SFX ID is explicitly passed to
chip_audio::play(); R20 uses them offline to rerender the PCM SFX bank.
"""
from pathlib import Path
import argparse, json

def be16(b,o,signed=False): return int.from_bytes(b[o:o+2],'big',signed=signed)
def be32(b,o): return int.from_bytes(b[o:o+4],'big')

def parse_one(rom, sid, addr):
    inst=addr+be16(rom,addr,True)
    tick=rom[addr+2]; n=rom[addr+3]; p=addr+4
    tracks=[]
    for _ in range(n):
        state=be16(rom,p); rel=be16(rom,p+2,True); init=be16(rom,p+4); p+=6
        ch=state&0xff
        kind=2 if ch&0x80 else (0 if ch==6 else 1)
        tracks.append(dict(kind=kind,address=addr+rel,transpose=(init>>8)&0xff,volume=init&0xff,channel=ch))
    return dict(id=sid,address=addr,instrument=inst,tick=tick,tracks=tracks)

def main():
    ap=argparse.ArgumentParser(); ap.add_argument('--rom',type=Path,required=True); ap.add_argument('--out',type=Path,default=Path('gba/audio_p134r/src/ps4_sfx_chip_data_generated.inc')); a=ap.parse_args()
    r=a.rom.read_bytes(); rec=[]
    # Normal SFX B5-F7.
    t=0x0DE4B6
    for sid in range(0xB5,0xF8): rec.append(parse_one(r,sid,be32(r,t+4*(sid-0xB5))))
    # Special SFX F8-FA have a separate pointer table but the same header layout.
    t=0x0DE5C2
    for sid in range(0xF8,0xFB): rec.append(parse_one(r,sid,be32(r,t+4*(sid-0xF8))))
    lines=['// Generated from retail PSIV SFX headers. Do not hand-edit.', 'static constexpr song_header generated_sfx_songs[] = {']
    for x in rec:
        tr=[]
        for z in x['tracks']:
            trans=z['transpose']; trans=trans-256 if trans>=128 else trans
            tr.append(f'{{{z["kind"]},0,0,0x{z["address"]:06X}u,{trans},{z["volume"]},0,0}}')
        while len(tr)<9: tr.append('{0,0,0,0,0,0,0,0}')
        fm=sum(z['kind']==1 for z in x['tracks']); psg=sum(z['kind']==2 for z in x['tracks']); dac=sum(z['kind']==0 for z in x['tracks'])
        lines.append('  {'+f'0x{x["id"]:02X},{fm},{psg},{x["tick"]},0,{len(x["tracks"])},{1 if dac else 0},0,0x{x["instrument"]:06X}u,'+'{'+','.join(tr)+'}'+'},')
    lines.append('};')
    a.out.parent.mkdir(parents=True,exist_ok=True); a.out.write_text('\n'.join(lines)+'\n')
    meta=a.out.with_suffix('.json'); meta.write_text(json.dumps(rec,indent=2)+'\n')
    print('wrote',a.out,'descriptors',len(rec))
if __name__=='__main__': main()
