#!/usr/bin/env python3
"""Refresh only changed presentation assets; preserve R28 audio and asset offsets."""
import os,re,struct,subprocess,sys
from pathlib import Path
from PIL import Image,ImageDraw
ROOT=Path(__file__).resolve().parent.parent
os.environ['PS4_NO_HASH']='1'
subprocess.run([sys.executable,str(ROOT/'tools/generate_party_battle_frames.py'),'--rom',str(ROOT/'references/roms/ps4built.bin')],check=True,cwd=ROOT)
h=(ROOT/'gba/precomp_assets.h').read_text();defs={n:int(v) for n,v in re.findall(r'#define (\w+) (\d+)u',h)}
p=ROOT/'gba/precomp_assets.bin';blob=bytearray(p.read_bytes());po=defs['PS4_PRECOMP_BATTLE_PALETTE_OFF'];pal=struct.unpack_from('<256H',blob,po)
avail=[*range(1,80),*range(85,96),*range(112,192)]
def gba(c):return (c[0]>>3)|((c[1]>>3)<<5)|((c[2]>>3)<<10)
lut={}
for i in avail:lut.setdefault(pal[i],i)
def pix(c):return 0 if c==(248,0,248) else lut[gba(c)]
changed=[];contact=Image.new('RGB',(576,11*82),(20,24,32));draw=ImageDraw.Draw(contact)
import json
manifest=json.loads((ROOT/'generated_precomp/party_battle/manifest.json').read_text())
for row,(name,ch) in enumerate(manifest['characters'].items()):
 sh=Image.open(ROOT/'generated_precomp/party_battle'/ch['pose_file']).convert('RGB');base='PS4_PRECOMP_PARTY_'+name.upper()
 contact.paste(sh,(0,row*82+18));draw.text((2,row*82+2),name+' : '+', '.join(ch['poses']),fill='white')
 for kind,n in [('POSES',len(ch['poses']))]+([('ATTACK',len(ch['attack']['holds']))] if 'attack' in ch else []):
  sheet=sh if kind=='POSES' else Image.open(ROOT/'generated_precomp/party_battle'/ch['attack']['file']).convert('RGB')
  off=defs[base+'_'+kind+'_PIXELS_OFF'];w=defs[base+'_W'];hh=defs[base+'_H'];sz=w*hh
  for f in range(n):
   wide=name=='wren' and kind=='POSES' and ch['poses'][f]=='load_gun'
   im=sheet.crop((f*64+(4 if wide else 8),8,f*64+(60 if wide else 56),56)).resize((w,hh),Image.Resampling.NEAREST)
   data=bytes(pix(c) for c in im.getdata());old=bytes(blob[off+f*sz:off+(f+1)*sz])
   if data!=old:
    assert name in ('demi','wren') and kind=='POSES' and f in (3,4),(name,kind,f)
    blob[off+f*sz:off+(f+1)*sz]=data;changed.append((name,ch['poses'][f]))
assert set(changed)<=set([('demi','load_gun'),('demi','fire_gun'),('wren','load_gun'),('wren','fire_gun')]),changed
p.write_bytes(blob)
wide=Image.open(ROOT/'generated_precomp/party_battle/wren_poses.png').crop((3*64+4,8,3*64+60,56)).resize((42,34),Image.Resampling.NEAREST)
wide_bytes=bytes(pix(c) for c in wide.getdata())
(ROOT/'gba/r29_wren_load.h').write_text('/* Wren Load Gun: source 7x6 map, projected to 42x34 without dropping a column. */\nstatic const uint8_t r29_wren_load[1428]={\n'+''.join('    '+','.join(str(v) for v in wide_bytes[i:i+42])+',\n' for i in range(0,len(wide_bytes),42))+'};\n')
out=ROOT.parent/'build_artifacts';out.mkdir(exist_ok=True);contact.save(out/'R29_PARTY_COMBAT_POSES.png')
# Use the accepted readable menu letterforms for dialogue. Preserve all
# 128 encoded characters; rare symbols use coverage reduction of source art.
a=(ROOT/'gba/r28_readable_font.h').read_text();cols=[tuple(int(x,16) for x in re.findall(r'0x([0-9A-Fa-f]{2})',row)) for row in re.findall(r'\{([^{}]+)\}',a)]
assert len(cols)==87
rom=(ROOT/'references/roms/ps4built.bin').read_bytes();punct={0x35:0x53,0x36:0x54,0x37:0x55,0x39:0x34,0x3a:0x32,0x3b:0x33,0x3c:0x31}
rows=[]
for code in range(128):
 wi=(code if 1<=code<=26 else code+30 if 27<=code<=52 else code-37 if 64<=code<=73 else punct.get(code))
 if code==0:bits=[0]*11
 elif wi is not None:
  bits=[sum((0x80>>x) for x in range(5) if cols[wi][x]&(1<<(y*7//10))) for y in range(10)]+[0]
 else:
  bits=[]
  for y in range(11):
   v=0
   for x in range(5):
    if any(rom[0x2A3542+code*16+sy]&(0x80>>sx) for sy in range(y*16//11,(y+1)*16//11) for sx in range(x*8//5,(x+1)*8//5)):v|=0x80>>x
   bits.append(v)
 rows.append(bits)
(ROOT/'gba/r29_dialogue_font.h').write_text('/* R29 hinted 6x11 dialogue glyphs; original encoded charset and 32 columns. */\nstatic const uint8_t r29_dialogue_font[128][11]={\n'+''.join('    {'+','.join('0x%02X'%b for b in r)+'},\n' for r in rows)+'};\n')
# Visual check at the actual display size.
preview=Image.new('RGB',(240,74),(4,7,24));d=ImageDraw.Draw(preview)
for yi,txt in enumerate(['Chaz: Is everyone ready?','Demi and Wren: Load gun!','0123456789  I l i . , ! ?']):
 for xi,ch in enumerate(txt):
  cc=0 if ch==' ' else ord(ch)-64 if 'A'<=ch<='Z' else ord(ch)-ord('a')+27 if 'a'<=ch<='z' else ord(ch)-ord('0')+64 if ch.isdigit() else {'.':53,',':55,':':57,'!':58,'?':59}.get(ch,0)
  for yy,b in enumerate(rows[cc]):
   for xx in range(6):
    if b&(0x80>>xx):preview.putpixel((12+xi*6+xx,8+yi*18+yy),(255,255,255))
preview.save(out/'R29_DIALOGUE_FONT.png')
print('PASS all 11 party sheets / 43 body poses / 11 close-range frames; corrected',changed)
