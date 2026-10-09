#!/usr/bin/env python3
"""Native panel fallback for the original R43 cave/Elsydeon/reunion panels, against independent pixels.
Production Nemesis/Enigma decoders and panel art/layer/geometry/compositor
functions execute unchanged. Precompiled lookups are absent in this fixture.
Independent Python decoding supplies the reference. No hardware/display or
stacked-panel UI playthrough.
"""
from pathlib import Path
import re,json,os,subprocess
from md_compression import nemesis_decompress,enigma_decompress
from PIL import Image,ImageDraw
R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts';rom=(R/'references/roms/ps4built.bin').read_bytes();src=(R/'gba/title_runtime_v06.c').read_text()
u16=lambda p:int.from_bytes(rom[p:p+2],'big');u32=lambda p:int.from_bytes(rom[p:p+4],'big')
ids=[0xA7,*range(0x128,0x140),0x18C,0x18E];panels={};arts={};maps={}
for id in ids:
 p=u32(0x6B2B6+(id>>6)*4)+(id&63)*30;dest=u16(p);a=u32(p+2);w,h=rom[p+6:p+8];art=nemesis_decompress(rom[a:]);assert len(art)<=20480 and w*h<=2048
 arts[a]=art;layers=[]
 for off in [20,10]:
  base=u16(p+off);pal=u32(p+off+2);addr=u32(p+off+6)
  if base!=65535 and addr:
   data=enigma_decompress(rom[addr:],base);assert w*h<=len(data)<=2048;maps[(addr,base)]=data;layers.append((base,pal,addr,data))
 panels[id]=(p,dest,w,h,art,layers)
def function(name,text=src):
 m=re.search(r'^static[^\n]*\b'+name+r'\([^;]*?\)\s*\{',text,re.M);assert m,name
 p=m.end();d=1
 while d:d+=(text[p]=='{')-(text[p]=='}');p+=1
 return text[m.start():p]+'\n'
s=(O/'r43_scene_probe.c').read_text().split('int main(')[0]
s=s.replace('panel_art_scratch[16384]','panel_art_scratch[20480]')
for name in ['nemesis_fast','enigma_decode']:
 old=function(name,s);s=s.replace(old,old[:old.index('{')].rstrip()+';\n')
s+='#define PS4_PREDECODE_NEM 0u\nstatic int psc_predecode_copy(unsigned kind,unsigned src,void*out,unsigned cap,unsigned*n){(void)kind;(void)src;(void)out;(void)cap;(void)n;return 0;}\n'
s+=function('nemesis_fast_fallback')+function('nemesis_fast')
s+=re.search(r'struct BitReader \{[^}]+\};',src)[0]+'\n'
for name in ['bit_get','bit_read','enigma_inline','enigma_decode']:s+=function(name)
s+='#define PANEL_ART_SCRATCH_SIZE 20480u\nstatic int panel_render_precompiled(unsigned id){(void)id;return 0;}\n'
for name in ['panel_art_px','panel_render_layer_scaled','panel_panel_geometry','panel_render_scaled']:s+=function(name)
s+='static const unsigned panels[]={'+','.join(map(str,ids))+'};\n'
s+=r'''
int main(int argc,char**argv){assert(argc==3);FILE*f=fopen(argv[1],"rb");assert(f&&fread(rom,1,sizeof rom,f)==sizeof rom);fclose(f);for(unsigned i=0;i<sizeof panels/sizeof panels[0];i++){unsigned id=panels[i],w,h,x,y;panel_panel_geometry(id,&w,&h,&x,&y);assert(w&&h&&w*h<=2048);memset(field_pause_fb,0,sizeof field_pause_fb);memset(field_pause_pal,0,sizeof field_pause_pal);panel_render_scaled(id);char path[512];snprintf(path,sizeof path,"%s/R43_panel_%03X_pixels.bin",argv[2],id);f=fopen(path,"wb");assert(f&&fwrite(field_pause_fb,1,sizeof field_pause_fb,f)==sizeof field_pause_fb);fclose(f);}puts("PASS original R43 native panel fallback/compositors and scratch bounds under ASan/UBSan");}
'''
c=O/'r43_panel_probe.c';c.write_text(s);subprocess.run(['gcc','-std=c11','-O1','-g','-no-pie','-fsanitize=address,undefined','-I'+str(R/'gba'),str(c),'-o',str(O/'r43_panel_probe')],check=True)
subprocess.run([str(O/'r43_panel_probe'),str(R/'references/roms/ps4built.bin'),str(O)],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
sheet=Image.new('RGB',(960,1260),(20,24,32));draw=ImageDraw.Draw(sheet);report=[]
for index,id in enumerate(ids):
 p,dest,w,h,art,layers=panels[id];fb=bytearray(38400);palette=[(0,0,0)]*64;px,py=rom[p+8]*8,rom[p+9]*8
 for base,pal,addr,mapping in layers:
  for n in range(16):
   color=u16(pal+n*2);palette[((base>>13)&3)*16+n]=tuple(((color>>s)&14)*255//14 for s in [0,4,8])
  for y in range(160):
   sy=y*224//160
   if not py<=sy<py+h*8:continue
   for x in range(240):
    sx=x*320//240
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
 assert (O/f'R43_panel_{id:03X}_pixels.bin').read_bytes()==fb,hex(id)
 im=Image.new('RGB',(240,160));im.putdata([palette[n] for n in fb]);x=(index%4)*240;y=(index//4)*180;sheet.paste(im,(x,y+20));draw.text((x+6,y+4),f'Panel ${id:03X}',fill='white')
 report.append({'id':hex(id),'art_bytes':len(art),'map_dimensions':[w,h],'pixels_compared':38400})
sheet.save(O/'R43_PANEL_PREVIEW.png');(O/'R43_PANEL_PROBE.json').write_text(json.dumps({'scope':__doc__,'panels':report,'total_pixels_compared':len(ids)*38400,'all_pixels_match_independent_reference':True},indent=2)+'\n');print('PASS independent original scene panel pixels:',len(ids)*38400)
