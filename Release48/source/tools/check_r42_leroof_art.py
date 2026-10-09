#!/usr/bin/env python3
"""Production Nemesis/Enigma full-screen LeRoof art and native scene compositor.
Independent source tile sampling compares all38400 scaled pixels. No display
hardware or dialogue/stacked-panel lifecycle certification.
"""
from pathlib import Path
import re,subprocess,os,json
from md_compression import nemesis_decompress,enigma_decompress
from PIL import Image
R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts';src=(R/'gba/title_runtime_v06.c').read_text();rom=(R/'references/roms/ps4built.bin').read_bytes()
def function(name,text=src):
 m=re.search(r'^static[^\n]*\b'+name+r'\([^;]*?\)\s*\{',text,re.M);assert m,name
 p=m.end();d=1
 while d:d+=(text[p]=='{')-(text[p]=='}');p+=1
 return text[m.start():p]+'\n'
s=(O/'r42_panel_probe.c').read_text().split('int main(')[0]
s=s.replace(function('load_nem_vram_auto',s),'#define PS4_FASTGFX_NEM 1u\nstatic void field_gba_ready_bind(unsigned kind,uint32_t a,uint16_t t,unsigned n){(void)kind;(void)a;(void)t;(void)n;}\nstatic unsigned load_nem_vram_auto(uint32_t,uint16_t);\n')
s=s.replace('static int load_nem_vram_auto(unsigned,unsigned);','static unsigned load_nem_vram_auto(uint32_t,uint16_t);')
s+=function('load_nem_vram_auto')
s+=r'''
int main(int argc,char**argv){assert(argc==3);FILE*f=fopen(argv[1],"rb");assert(f&&fread(rom,1,sizeof rom,f)==sizeof rom);fclose(f);fresh42();r42_leroof_art();f=fopen(argv[2],"wb");assert(f&&fwrite(field_pause_fb,1,38400,f)==38400);fclose(f);puts("PASS original LeRoof production Nemesis/Enigma and native full-screen compositor under ASan/UBSan");}
'''
c=O/'r42_leroof_art_probe.c';c.write_text(s);subprocess.run(['gcc','-std=c11','-O1','-g','-no-pie','-fsanitize=address,undefined','-I'+str(R/'gba'),str(c),'-o',str(O/'r42_leroof_art_probe')],check=True)
p=O/'R42_LEROOF_PIXELS.bin';subprocess.run([str(O/'r42_leroof_art_probe'),str(R/'references/roms/ps4built.bin'),str(p)],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
art=nemesis_decompress(rom[0x1DB2C8:]);mapping=enigma_decompress(rom[0x1DDE0A:],0x260);fb=bytearray(38400)
for y in range(160):
 sy=y*224//160
 for x in range(240):
  sx=x*320//240;e=mapping[(sy//8)*40+sx//8];ix=sx%8;iy=sy%8
  if e&0x800:ix=7-ix
  if e&0x1000:iy=7-iy
  n=((e&2047)-0x260)*32+iy*4+ix//2;assert 0<=n<len(art);q=(art[n]>>(0 if ix&1 else 4))&15
  fb[y*240+x]=((e>>13)&3)*16+q if q else 0
assert p.read_bytes()==fb
u16=lambda p:int.from_bytes(rom[p:p+2],'big');palette=[u16(0x1DE258+i*2) for i in range(16)]+[0]*48;palette[2:11]=[u16(0x70626+i*2) for i in range(9)]
im=Image.new('RGB',(240,160));im.putdata([tuple(((palette[v]>>s)&14)*255//14 for s in [0,4,8]) for v in fb]);im.save(O/'R42_LEROOF_PREVIEW.png')
(O/'R42_LEROOF_ART_PROBE.json').write_text(json.dumps({'scope':__doc__,'art_source':'1DB2C8','mapping_source':'1DDE0A','source_dimensions':[40,28],'pixels_compared':38400,'all_pixels_match':True,'art_bytes':len(art)},indent=2)+'\n');print('PASS independent38400 original LeRoof pixels')
