#!/usr/bin/env python3
"""Compare every authored PD scene update with the preceding release in mGBA.
Transient baseline pages are compressed in memory, not imported into the ROM.
"""
from pathlib import Path
import argparse,json,sys,zlib
p=argparse.ArgumentParser(add_help=False);p.add_argument('--baseline-dir',type=Path,required=True);p.add_argument('--baseline-name',default='release08');p.add_argument('--baseline-crc',default='e6c74f61');opt,rest=p.parse_known_args();sys.argv=[sys.argv[0]]+rest
h=Path(__file__).with_name('check_r46_special_recovery_mgba.py')
exec(compile(h.read_text().split('\ndef special_fixture(')[0],str(h),'exec'),globals())
cur_symbols=symbols.copy();cur_state=state;cur_rom=rom;records={};rows=[]
def parse_map(path):
 d={}
 for line in path.read_text().splitlines():
  q=line.split()
  if len(q)>=5:
   try:d[q[-1]]=int(q[0],16)
   except ValueError:pass
 return d
for which in (opt.baseline_name,'current'):
 if which==opt.baseline_name:
  symbols=parse_map(opt.baseline_dir/(opt.baseline_name+'.map'));g=GBA(opt.baseline_dir/(opt.baseline_name+'.gba'));st=opt.baseline_dir/(opt.baseline_name+'-debug.bin')
  assert f'{zlib.crc32((opt.baseline_dir/(opt.baseline_name+".gba")).read_bytes()):08x}'==opt.baseline_crc
 else:symbols=cur_symbols;g=GBA(cur_rom);st=cur_state
 for id,skill,ticks in ((0x85,0,192),(0x85,0x67,519),(0x86,0x68,595)):
  g.load(st);g.wr16(symbols['native_main_frame_count'],0);call('ps4_debug_level60_party');ids=list(g.mem(md+0xf40a,5))
  for i,cid in enumerate(ids):call('battle_load_char',(cid,pp+i*62))
  call('battle_load_enemy',(id,ee));g.wr32(symbols['battle_enemy_count'],1);g.wr8(symbols['battle_event_index'],1);g.wr8(symbols['battle_enemy_placement'],20)
  call('battle_anim_reset');call('battle_ui_bind',(pp,5,ee,0));call('r45_pd_visual_reset',(ee,));call('r45_pd_body_snapshot',(id,));call('battle_compose_igg',(pp,5,ee,0,0));call('battle_present_page0')
  begin('r45_pd_transform' if skill else 'r45_pd_opening',(pp,5,ee,0,skill) if skill else (pp,5,ee))
  short=0
  for tick in range(ticks):
   assert run_until_break(g,symbols['battle_present_band'],30),(which,id,tick,'missing authored scene')
   # Complete scene shadow (including resident rows and untouched HUD), live
   # palette and original controller/art state. New cache fields excluded.
   snap=g.mem(0x0202a000,38400)+g.mem(0x02023400,512)+g.mem(0x0203b000,16568)+g.mem(pp,310)+g.mem(ee,62)
   key=(id,skill,tick)
   if which==opt.baseline_name:records[key]=zlib.compress(snap,1)
   else:
    old=zlib.decompress(records.pop(key));assert snap==old,(id,skill,tick,next(i for i in range(len(snap)) if snap[i]!=old[i]))
   if g.reg(1)<125:short+=1
   ret=g.reg(14)&~1;assert run_until_break(g,ret,30)
   assert g.mem(0x06000000,38400)==g.mem(0x0202a000,38400),'published full page differs'
  assert run_until_break(g,0x08800000,30),'controller return'
  row={'ROM':which,'form':id,'skill':skill,'source_updates':ticks,'resident_party_band_updates':short,'all_scene_palette_art_actor_bytes_match':which=='current'};rows.append(row);print('PASS',row,flush=True)
assert not records
(O/(prefix+'_PD_CONTROLLER_AB.json')).write_text(json.dumps({'baseline_crc32':opt.baseline_crc,'current_ROM':rom_id,'core':'mGBA 0.10.5','cases':rows,'source_scene_comparisons':1306,'scope':'all authored opening/transformation updates, full framebuffer/palette/controller art/state and actor bytes; no normal playthrough or hardware certification'},indent=2)+'\n')
