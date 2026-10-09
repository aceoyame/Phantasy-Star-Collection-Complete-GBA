#!/usr/bin/env python3
"""Actual linked party ARM rows and PD odd-word copies, independent oracles."""
from pathlib import Path
import argparse,json,re,sys
R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts'
ap=argparse.ArgumentParser();ap.add_argument('--label',default='CP17');ap.add_argument('--copy-only',action='store_true');a=ap.parse_args();prefix='R46_'+a.label
sys.path.insert(0,str(R/'tools_host/r46_mgba'));from mgba import GBA,L,run_until_break
s={}
for line in (R/'build-psc-module/ps4_psc_module.map').read_text().splitlines():
 p=line.split()
 if len(p)>=5:
  try:s[p[-1]]=int(p[0],16)
  except ValueError:pass
g=GBA(R.parent/'binaries/RUN_THIS_RELEASE48_PSC.gba');g.load(O/(prefix+'_MGBA_DEBUG.bin'))
def call(name,args=()):
 L.mg_break(0xffffffff);L.mg_cpsr(0x3f if s[name]&1 else 0x1f)
 for i,v in enumerate(args):L.mg_set_reg(i,v)
 for i,v in [(13,0x03007d80),(14,0x08800000),(15,s[name]&~1)]:L.mg_set_reg(i,v)
 assert run_until_break(g,0x08800000,100),name
fb=0x0202a000;party_cases=0;copy_cases=0;poses=0
defines={k:int(v) for k,v in re.findall(r'#define (PS4_PRECOMP_PARTY_\w+) (\d+)u',(R/'gba/precomp_assets.h').read_text())}
for key,off in ([] if a.copy_only else defines.items()):
 if not key.endswith(('_POSES_PIXELS_OFF','_ATTACK_PIXELS_OFF')):continue
 for frame in range(defines[key.replace('_PIXELS_OFF','_FRAMES')]):
  px=s['ps4_precomp_assets']+off+frame*1224;art=g.mem(px,1224);poses+=1
  # The compiler specializes this private wrapper to its sole native Y=91
  # callsite. Exercise that actual contract, including clipped row subsets.
  for left,sy,clip in [(102,91,None),(38,91,(0,0,240,125)),(204,91,(205,92,239,124)),(2,91,(3,92,37,124)),(102,91,(103,92,103,124)),(102,91,(120,100,121,101)),(102,91,(0,0,240,91)),(102,91,(0,125,240,160)),(3,91,None),(-2,91,None),(206,91,None)]:
   expected=bytearray([55])*38400
   if left>=0 and left+36<=240 and not left&1 and sy+34<=160:
    for y in range(34):
     if clip and (sy+y<clip[1] or sy+y>=clip[3]):continue
     for q in range(18):
      x=left+q*2
      # Exact retained source-pair clip semantics, including odd boundaries.
      if clip and (x+2<=clip[0] or x>=clip[2]):continue
      for k in [0,1]:
       c=art[y*36+q*2+k]
       if c:expected[(sy+y)*240+x+k]=c
   g.wr8(s['battle_compose_clip_active'],int(clip is not None))
   if clip:
    for name,v in zip(['x0','y0','x1','y1'],clip):g.wr32(s['battle_compose_clip_'+name],v)
   g.put(fb-8,b'\xa5'*8+bytes([55])*38400+b'\xa5'*8)
   call('battle_blit_party_projected',(px,left,sy))
   actual=g.mem(fb,38400)
   assert actual==expected,(key,frame,left,sy,clip,[(i,actual[i],expected[i]) for i in range(38400) if actual[i]!=expected[i]][:12])
   assert g.mem(fb-8,8)+g.mem(fb+38400,8)==b'\xa5'*16,'party framebuffer guard changed'
   party_cases+=1
g.put(0x02033600,bytes((i*73+(i>>3)*19)&255 for i in range(1024)))
for source in [0x02033600,s['ps4_precomp_assets']+defines['PS4_PRECOMP_PARTY_CHAZ_POSES_PIXELS_OFF']+128]:
 for so in range(8):
  for do in range(8):
   for n in [0,1,2,3,4,5,6,7,31,63,239,240]:
    expected=bytearray([55])*272;expected[8+do:8+do+n]=g.mem(source+so,n)
    g.put(fb-8,bytes([55])*272);call('r46_pd_copy_span',(fb+do,source+so,n))
    assert g.mem(fb-8,272)==expected,('copy',hex(source),so,do,n)
    copy_cases+=1
result={'core':'upstream mGBA 0.10.5','actual_linked_ARM':True,'copy_only':a.copy_only,'party_poses_and_attack_frames':poses,'party_cases':party_cases,'party_full_frame_pixel_comparisons':party_cases*38400,'party_framebuffer_guards_unchanged':True,'odd_copy_cases':copy_cases,'odd_copy_scope':'EWRAM and cartridge source, every 0–7 source/destination alignment, zero/short/long/odd lengths; exact bytes and surrounding guards','all_match':True,'original_VDP_timing_or_audio_quality_certified':False}
(O/(prefix+'_MGBA_ROWS.json')).write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result),flush=True)
