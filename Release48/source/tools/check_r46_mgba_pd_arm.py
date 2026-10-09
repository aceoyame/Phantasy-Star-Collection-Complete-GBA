#!/usr/bin/env python3
"""Actual ARM/Thumb PD fast body versus independent original-map sampling."""
from pathlib import Path
import sys,struct,json,argparse
R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts'
sys.path.insert(0,str(R/'tools_host/r46_mgba'));from mgba import GBA,L,run_until_break
from md_compression import enigma_decompress
s={}
for line in (R/'build-psc-module/ps4_psc_module.map').read_text().splitlines():
 p=line.split()
 if len(p)>=5:
  try:s[p[-1]]=int(p[0],16)
  except ValueError:pass
ap=argparse.ArgumentParser();ap.add_argument('--label',default='CP16');ap.add_argument('--all-phases',action='store_true');args=ap.parse_args();prefix='R46_'+args.label
g=GBA(R.parent/'binaries/RUN_THIS_RELEASE48_PSC.gba');g.load(O/(prefix+'_MGBA_DEBUG.bin'))
rom=(R/'references/roms/ps4built.bin').read_bytes()
def mem(p,n):return g.mem(p,n)
def put(p,data):g.put(p,data)
def call(name,args=(),thumb=False):
 L.mg_break(0xffffffff);L.mg_cpsr(0x3f if thumb else 0x1f)
 for i,v in enumerate(args):L.mg_set_reg(i,v)
 for i,v in [(13,0x03007d80),(14,0x08800000),(15,s[name]&~1)]:L.mg_set_reg(i,v)
 assert run_until_break(g,0x08800000,100),name
visual=0x0203b000;fb=0x0202a000;cases=0
for id in [0x85,0x86,0x87]:
 ee=0x02033800;call('battle_load_enemy',(id,ee));g.wr32(s['battle_enemy_count'],1);call('battle_anim_reset');call('r45_pd_visual_reset',(ee,));call('r45_pd_body_snapshot',(id,))
 art=mem(visual+7296,g.rd16(visual+16514));rec=0x27f3ae+id*20;ptr=int.from_bytes(rom[rec+14:rec+18],'big');mapping=enigma_decompress(rom[ptr:])[:600]
 g.wr8(visual+16523,1);g.wr32(0x0200dffc,0)
 for phase in (range(512) if args.all_phases else [0,1,2,3,4,5,6,7,63,127,191,255,319,383,447,511]):
  shifts=[phase+y*7-320 for y in range(192)];put(visual+128,struct.pack('<192h',*shifts))
  for clipped in [False,True]:
   y0,y1=(phase%19,160-phase%17) if clipped else (0,160)
   g.wr8(s['battle_compose_clip_active'],int(clipped));g.wr32(s['battle_compose_clip_x0'],0);g.wr32(s['battle_compose_clip_x1'],240);g.wr32(s['battle_compose_clip_y0'],y0);g.wr32(s['battle_compose_clip_y1'],y1)
   expected=bytearray([55])*38400;put(fb,expected)
   for y in range(y0,min(y1,86)):
    sy=y*7//5
    for x in range(240):
     sx=(x*4//3-shifts[sy])&511
     if sx>=320:continue
     e=mapping[(sy//8)*40+sx//8];lx=(sx&7)^(7 if e&0x800 else 0);ly=(sy&7)^(7 if e&0x1000 else 0);at=(e&2047)*32+ly*4+lx//2
     c=((art[at]>>(0 if lx&1 else 4))&15) if at<len(art) else 0
     if c:expected[y*240+x]=192+c
   call('r46_pd_body_fast',thumb=True);actual=mem(fb,38400)
   assert actual==expected,(id,phase,clipped,next(i for i in range(38400) if actual[i]!=expected[i]))
   assert mem(0x0200da40,1200)==struct.pack('<600H',*mapping),'cached map differs from original Enigma map'
   cases+=1
 print('PASS actual ARM/Thumb PD body',hex(id),flush=True)
 # Ordinary PD redraws can overlap other battle controllers. They must use
 # the cartridge map without touching the shared battle-object arena.
 g.wr8(visual+16523,0);put(0x0200d000,bytes([0xa5])*4096)
 call('r46_pd_body_fast',thumb=True)
 assert mem(0x0200d000,4096)==bytes([0xa5])*4096,'inactive PD renderer wrote another controller arena'
(O/(prefix+'_MGBA_PD_ARM_PIXELS.json')).write_text(json.dumps({'core':'upstream mGBA 0.10.5','cases':cases,'full_frame_pixel_comparisons':cases*38400,'all_match':True,'scope':'actual linked ARM row and Thumb outer loop; original Enigma map and independent scalar nibble projection; native art snapshot input; three forms, full width and vertical clips','scroll_phases_per_form':512 if args.all_phases else 16,'original_animation_art_or_VDP_parity':False},indent=2)+'\n')
