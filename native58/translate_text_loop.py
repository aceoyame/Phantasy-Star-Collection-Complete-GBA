"""One full-CCR native body for verified relocated text-output loops."""
from pathlib import Path
import re,sys,json
W=Path(__file__).resolve().parents[1];D=W/'native58';A=W/'Release48/source/additional';sys.path.insert(0,str(A/'aot'));import aot68k as tx
code=(W/'cp57_base/aot_code.c').read_text();tabs=(W/'cp57_base/aot_tabs.c').read_text();names=['Anne','Rudger','Kinds','Shilka','Amia','Huey','Eusis']
roms={g:(W/'Release48/additional_roms'/f'{g}_Adventure_-_English.bin').read_bytes() for g in names};pattern=roms['Anne'][0x2e168:0x2e19a]
roots={g:[m.start() for m in re.finditer(re.escape(pattern),r[:0x40000]) if m.start()%2==0] for g,r in roms.items()};assert all(len(v)==1 for v in roots.values());print('verified roots',roots)
tx.flag_liveness=lambda insns,members:{pc:31 for pc in members};tx.memwr=lambda address,size,value:'W%d(%s,%s)'%(size*8,address,value)
root=roots['Anne'][0];dec=tx.Tx(roms['Anne'],0);insns={};p=root
while p<root+len(pattern):
 ins=dec.insn(p);assert not ins.interp;insns[p]=ins;p+=ins.length
text,ents=tx.emit_region(insns,sorted(insns),'@F@');assert len(ents)==len(insns)
fn=len(re.findall(r'^u32 aotf\d+\(u32\);',tabs,re.M));text=text.replace('@F@','aotf'+str(fn))
old=re.search(r'static const u32 xpc\[\] AOT_TAB=\{([^}]+)\};',text);values=[int(v[:-1],0) for v in old[1].split(',')];assert all(root<=v<root+len(pattern) for v in values)
offsets=[v-root for v in values];text=text.replace(old[0],'static const u32 xpc[] AOT_TAB={'+','.join(str(v)+'u' for v in offsets)+'};')
text=text.replace('hi=C.pc&0xFF000000u','root=C.pc-(entry<'+str(len(values))+'u?xpc[entry]:0u)').replace('npc=hi|xpc[i]','npc=root+xpc[i]')
# Return addresses pushed by JSR and instruction-stop PCs retain their
# original absolute value at each verified relocated routine.
def relative(m):
 pc=int(m[1],16);assert root<=pc<=root+len(pattern),hex(pc)
 return '(root+%du)'%(pc-root)
text=re.sub(r'hi\|0x([0-9a-fA-F]+)u',relative,text);assert not re.search(r'\bhi\b',text)
code+='\n/* CP58: exact relocated text loop, including newline and full CCR. */\n__attribute__((section(".zaot"))) '+text+'\n'
entries=[]
for game in names:
 key='xrom_'+game.lower();m=re.search(r'(static const AotEnt AOT_TAB '+key+r'\[\]=)\{(.*?)\};',tabs,re.S)
 rows=[tuple(int(x,0) for x in r) for r in re.findall(r'\{(0x[0-9a-f]+)u,(\d+),(\d+),(\d+)\}',m[2])];known={r[0] for r in rows}
 for base in roots[game]:
  assert roms[game][base:base+len(pattern)]==pattern
  for pc,index in ents:
   offset=pc-root;guest=base+offset;assert guest not in known;rows.append((guest,fn,index,65535));entries.append({'game':game,'pc':f'{guest:06x}','fn':fn,'index':index,'root':f'{base:06x}'})
 rows.sort();tabs=tabs[:m.start()]+m[1]+'{\n'+''.join('{0x%06xu,%d,%d,%d},\n'%r for r in rows)+'};'+tabs[m.end():];tabs=re.sub(r'('+key+r',)\d+(\})',lambda m:m[1]+str(len(rows))+m[2],tabs)
p=re.search(r'const u32 AOT_TAB aot_pool\[\]\[64\]=\{(.*?)\};',tabs,re.S);pool=[tuple(int(v[:-1],0) for v in r.split(',')) for r in re.findall(r'\{([^{}]+)\}',p[1])];lookup={row:i for i,row in enumerate(pool)}
for game in names:
 m=re.search(r'(static const u16 AOT_TAB pg_'+game.lower()+r'\[64\]=)\{([^}]+)\}',tabs);pages=[int(v) for v in m[2].split(',')];es=[e for e in entries if e['game']==game]
 for page in {int(e['pc'],16)>>12 for e in es}:
  row=list(pool[pages[page]]) if pages[page]!=65535 else [0]*64
  for e in es:
   pc=int(e['pc'],16)
   if pc>>12==page:row[(pc&4095)>>6]|=1<<((pc>>1)&31)
  key=tuple(row)
  if key not in lookup:lookup[key]=len(pool);pool.append(key)
  pages[page]=lookup[key]
 tabs=tabs[:m.start()]+m[1]+'{'+','.join(map(str,pages))+'}'+tabs[m.end():]
used=sorted({int(x) for v in re.findall(r'static const u16 AOT_TAB r?pg_\w+\[\d+\]=\{([^}]+)\}',tabs) for x in v.split(',') if int(x)!=65535});remap={v:i for i,v in enumerate(used)}
tabs=re.sub(r'(static const u16 AOT_TAB r?pg_\w+\[\d+\]=)\{([^}]+)\}',lambda m:m[1]+'{'+','.join(str(remap.get(int(v),65535)) for v in m[2].split(','))+'}',tabs)
tabs=re.sub(r'(const u32 AOT_TAB aot_pool\[\]\[64\]=)\{.*?\};',lambda m:m[1]+'{'+','.join('{'+','.join('0x%xu'%v for v in pool[p])+'}' for p in used)+'};',tabs,flags=re.S)
tabs=tabs.replace('const aot_fn AOT_TAB aot_fns[]=',f'u32 aotf{fn}(u32);\nconst aot_fn AOT_TAB aot_fns[]=');tabs=re.sub(r'(const aot_fn AOT_TAB aot_fns\[\]=\{)([^}]*)(\})',lambda m:m[1]+m[2]+',aotf'+str(fn)+m[3],tabs)
(A/'aot/generated/aot_code.c').write_text(code);(A/'aot/generated/aot_tabs.c').write_text(tabs);(D/'TEXT_LOOP_ENTRIES.json').write_text(json.dumps({'function':fn,'entries':entries,'pattern':pattern.hex(),'offsets':offsets,'full_CCR':True,'shared_verified_roots':roots},indent=2)+'\n')
print('translated',len(insns),'instructions in one function; entry maps',len(entries))
