#!/usr/bin/env python3
"""Append measured 68000 ROM paths while preserving existing CP13 function IDs.
The input miss logs record interpreter guest cycles by entry PC. Existing RAM
translations and their self-modification checks are retained byte for byte.
"""
from pathlib import Path
import argparse,re,sys,json
R=Path(__file__).resolve().parents[2];A=R/'source/additional';BASE=R/'source/references/cp13_final/aot/generated'
sys.path.insert(0,str(A/'aot'));import aot68k as tx
ap=argparse.ArgumentParser();ap.add_argument('--entries',type=int,default=48);ap.add_argument('--span',type=int,default=8);ap.add_argument('--skip-nei',action='store_true');a=ap.parse_args()
code=(BASE/'aot_code.c').read_text();tabs=(BASE/'aot_tabs.c').read_text()
roms={n:(R/f'additional_roms/{n}_Adventure_-_English.bin').read_bytes() for n in ['Anne','Rudger','Kinds','Shilka','Amia','Huey','Eusis','Nei']}
tables={}
for name,body in re.findall(r'static const AotEnt AOT_TAB (\w+)\[\]=\{(.*?)\};',tabs,re.S):
 tables[name]=[tuple(int(x,0) for x in row) for row in re.findall(r'\{(0x[0-9a-f]+)u,(\d+),(\d+),(\d+)\}',body)]
nextfn=len(re.findall(r'^u32 aotf\d+\(u32\);',tabs,re.M));newfn=[];summary=[]
# New paths may stop at any scanline boundary: preserve all flags, including
# ones subsequently overwritten when an entire path runs without stopping.
tx.flag_liveness=lambda insns,members:{pc:31 for pc in members}
# Constant-address stores must retain the bus helper's side-effect counter
# and both code-generation increments for longword writes.
tx.memwr=lambda address,size,value: 'W%d(%s,%s)'%(size*8,address,value)
for grp,game in [('common','Anne'),('nei','Nei')]:
 if a.skip_nei and grp=='nei':continue
 name='rom_'+grp;known={row[0] for row in tables[name]};dec=tx.Tx(roms[game],0)
 rows=[line.split() for line in (R.parent/f'validation_cp14/MISS_{game}.txt').read_text().splitlines()]
 seeds=[int(row[0],16) for row in sorted(rows,key=lambda row:-int(row[1])) if int(row[0],16)<0x40000 and int(row[0],16) not in known][:a.entries]
 selected={};work=[(pc,0) for pc in seeds]
 while work:
  pc,depth=work.pop()
  if pc in known or pc in selected or not 0<=pc<len(roms[game])-1 or depth>=a.span:continue
  ins=dec.insn(pc)
  if ins.interp:continue
  if grp=='common' and any(data[pc:pc+ins.length]!=roms[game][pc:pc+ins.length] for n,data in roms.items() if n!='Nei'):continue
  selected[pc]=ins
  work.extend((target,depth+1) for target in ins.succ)
 added=set(selected)
 # The interpreter returns to AOT only on a taken branch, not on linear
 # fall-through. Keep this new shift entry's sequential decoding tail inside
 # the new exact-flag region; dispatching halfway into the old region would
 # expose its omitted dead flags at an interrupt/cycle boundary.
 if 0x37f8 in selected:
  for pc in [0x37fa,0x37fc,0x3800,0x3802,0x3804,0x3806,0x3808]:
   ins=dec.insn(pc)
   assert not ins.interp
   if grp=='common':assert all(data[pc:pc+ins.length]==roms[game][pc:pc+ins.length] for n,data in roms.items() if n!='Nei')
   selected[pc]=ins
 for members in tx.regions_of(selected):
  text,entries=tx.emit_region(selected,members,'@F@');fn=nextfn;nextfn+=1
  code+='\n'+ '__attribute__((section(\".aotextra\"))) '+text.replace('@F@','aotf'+str(fn))+'\n';newfn.append(fn)
  tables[name].extend((pc,fn,idx,65535) for pc,idx in entries if pc in added)
 tables[name].sort()
 summary.append({'group':grp,'profile_entries':len(seeds),'added_instructions':len(added),'private_continuation_instructions':len(selected)-len(added),'pcs':[f'{pc:06x}' for pc in sorted(added)]})
# Keep the old bitmap pool so the RAM pages retain their original identifiers.
poolmatch=re.search(r'const u32 AOT_TAB aot_pool\[\]\[64\]=\{(.*?)\};',tabs,re.S)
pool=[tuple(int(v.rstrip('u'),0) for v in row.split(',')) for row in re.findall(r'\{([^{}]+)\}',poolmatch[1])];pidx={v:i for i,v in enumerate(pool)}
for game in roms:
 key=game.lower();grp='nei' if game=='Nei' else 'common';words={}
 for pc,*_ in tables['rom_'+grp]+tables.get('xrom_'+key,[]):
  words.setdefault(pc>>12,[0]*64)[(pc&4095)>>6]|=1<<((pc>>1)&31)
 pages=[]
 for pg in range(64):
  if pg not in words:pages.append(65535);continue
  values=tuple(words[pg])
  if values not in pidx:pidx[values]=len(pool);pool.append(values)
  pages.append(pidx[values])
 tabs=re.sub(r'(static const u16 AOT_TAB pg_'+key+r'\[64\]=)\{[^}]*\}',lambda m:m[1]+'{'+','.join(map(str,pages))+'}',tabs)
for name,entries in tables.items():
 if name not in ('rom_common','rom_nei'):continue
 tabs=re.sub(r'(static const AotEnt AOT_TAB '+name+r'\[\]=)\{.*?\};',lambda m:m[1]+'{\n'+''.join('{0x%06xu,%d,%d,%d},\n'%row for row in entries)+'};',tabs,flags=re.S)
 tabs=re.sub(r'('+name+r',)\d+(,)',lambda m:m[1]+str(len(entries))+m[2],tabs)
tabs=re.sub(r'(const u32 AOT_TAB aot_pool\[\]\[64\]=)\{.*?\};',lambda m:m[1]+'{'+','.join('{'+','.join('0x%xu'%v for v in row)+'}' for row in pool)+'};',tabs,flags=re.S)
tabs=tabs.replace('const aot_fn AOT_TAB aot_fns[]=', ''.join(f'u32 aotf{fn}(u32);\n' for fn in newfn)+'const aot_fn AOT_TAB aot_fns[]=')
tabs=re.sub(r'(const aot_fn AOT_TAB aot_fns\[\]=\{)([^}]*)(\})',lambda m:m[1]+m[2]+''.join(',aotf'+str(fn) for fn in newfn)+m[3],tabs)
(A/'aot/generated/aot_code.c').write_text(code);(A/'aot/generated/aot_tabs.c').write_text(tabs)
(R.parent/'validation_cp14/AOT_EXPANSION.json').write_text(json.dumps({'new_functions':len(newfn),'groups':summary},indent=2)+'\n')
print('New 68000 functions',len(newfn),'; instructions',[(row['group'],row['added_instructions']) for row in summary])
