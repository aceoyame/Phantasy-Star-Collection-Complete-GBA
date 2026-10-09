#!/usr/bin/env python3
"""Compile the real high-ID panel fallback/compositor with original ROM data.
Nemesis/Enigma output fixtures are decoded from the original source ROM;
precompiled lookup is stubbed as absent for $140. No UI/hardware playthrough.
"""
from pathlib import Path
import re,json,os,subprocess
from md_compression import nemesis_decompress,enigma_decompress
from PIL import Image
R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts';rom=(R/'references/roms/ps4built.bin').read_bytes();src=(R/'gba/title_runtime_v06.c').read_text()
u16=lambda p:int.from_bytes(rom[p:p+2],'big');u32=lambda p:int.from_bytes(rom[p:p+4],'big')
p=u32(0x6B2B6+5*4);dest=u16(p);art_addr=u32(p+2);w,h=rom[p+6:p+8];art=nemesis_decompress(rom[art_addr:]);assert len(art)<=16384 and w*h<=2048
layers=[]
for off in [20,10]:
 base=u16(p+off);pal=u32(p+off+2);addr=u32(p+off+6)
 if base!=0xFFFF and addr:layers.append((base,pal,addr,enigma_decompress(rom[addr:],base)))
for base,pal,addr,data in layers:assert len(data)>=w*h and len(data)<=2048
s=(O/'r38_scene_probe.c').read_text().split('int main(')[0]
# The retained source fixture functions get the real panel's art/maps.
pos=s.index('static unsigned nemesis_fast(');s=s[:pos]+'static const uint8_t panel140_art[]={'+','.join(map(str,art))+'};\n'+s[pos:]
pos=s.index('default:assert(0);}',s.index('static unsigned nemesis_fast('));s=s[:pos]+f'case {art_addr}u:p=panel140_art;n=sizeof panel140_art;break;\n'+s[pos:]
for base,pal,addr,data in layers:
 pos=s.index('static unsigned enigma_decode(');s=s[:pos]+f'static const uint16_t panel140_map_{addr:x}[]={{'+','.join(map(str,data))+'};\n'+s[pos:]
 pos=s.index('default:assert(0);}',s.index('static unsigned enigma_decode('));s=s[:pos]+f'case {addr}u:assert(base=={base});a=panel140_map_{addr:x};n=sizeof panel140_map_{addr:x}/2;break;\n'+s[pos:]
# This old fixture intentionally discarded base for its ship maps; keep it here.
s+='#define PANEL_ART_SCRATCH_SIZE 16384u\n'
s+='static int panel_render_precompiled(unsigned id){assert(id==0x140);return 0;}\n'
def function(name):
 m=re.search(r'^static[^\n]*\b'+name+r'\([^;]*?\)\s*\{',src,re.M);assert m,name
 a=m.end();d=1
 while d:d+=(src[a]=='{')-(src[a]=='}');a+=1
 return src[m.start():a]+'\n'
for name in ['panel_art_px','panel_render_layer_scaled','panel_panel_geometry','panel_render_scaled']:s+=function(name)
s+=r'''
int main(int argc,char**argv){assert(argc==3);FILE*f=fopen(argv[1],"rb");assert(f&&fread(rom,1,sizeof rom,f)==sizeof rom);fclose(f);unsigned w,h,x,y;panel_panel_geometry(0x140,&w,&h,&x,&y);assert(w&&h&&w*h<=2048);memset(field_pause_fb,0,sizeof field_pause_fb);memset(field_pause_pal,0,sizeof field_pause_pal);panel_render_scaled(0x140);f=fopen(argv[2],"wb");assert(f&&fwrite(field_pause_fb,1,sizeof field_pause_fb,f)==sizeof field_pause_fb);fclose(f);puts("PASS original panel $140 native fallback/compositor and scratch bounds under ASan/UBSan");}
'''
c=O/'r38_panel140_probe.c';c.write_text(s);subprocess.run(['gcc','-std=c11','-O1','-g','-fsanitize=address,undefined','-I'+str(R/'gba'),str(c),'-o',str(O/'r38_panel140_probe')],check=True)
path=O/'R38_panel140_pixels.bin';subprocess.run([str(O/'r38_panel140_probe'),str(R/'references/roms/ps4built.bin'),str(path)],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
# Independent source tile sampling at the native global pixel coordinates.
fb=bytearray(240*160);palette=[(0,0,0)]*64;px,py=rom[p+8]*8,rom[p+9]*8
for base,pal,addr,mapping in layers:
 for n in range(16):
  color=u16(pal+n*2);palette[((base>>13)&3)*16+n]=tuple(((color>>s)&14)*255//14 for s in [0,4,8])
 for y in range(160):
  sy=(y*224)//160
  if not py<=sy<py+h*8:continue
  for x in range(240):
   sx=(x*320)//240
   if not px<=sx<px+w*8:continue
   lx,ly=sx-px,sy-py;e=mapping[(ly//8)*w+lx//8];ix,iy=lx%8,ly%8
   if e&0x800:ix=7-ix
   if e&0x1000:iy=7-iy
   tile=(e&0x7FF)-dest
   if tile<0:continue
   q=tile*32+iy*4+ix//2
   if q>=len(art):continue
   color=(art[q]>>(0 if ix&1 else 4))&15
   if color:fb[y*240+x]=((e>>13)&3)*16+color
assert path.read_bytes()==fb
im=Image.new('RGB',(240,160));im.putdata([palette[n] for n in fb]);im.resize((720,480),Image.Resampling.NEAREST).save(O/'R38_panel140_preview.png')
report={'scope':__doc__,'panel_id':0x140,'art_source':hex(art_addr),'art_bytes':len(art),'art_capacity':16384,'map_dimensions':[w,h],'maps':[{'address':hex(addr),'cells':len(m)} for b,pal,addr,m in layers],'map_capacity_cells':2048,'pixels_compared':38400,'native_compositor_matches_independent_reference':True}
(O/'R38_PANEL140_PROBE.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS independent original panel140 pixels:',len(fb),'pixels, art',len(art),'bytes')
