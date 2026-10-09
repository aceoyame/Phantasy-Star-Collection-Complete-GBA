from pathlib import Path
import json,sys
D=Path(__file__).resolve().parent;W=D.parent;sys.path.insert(0,str(W/'Release48/source/additional/aot'));from aot68k import Tx
s=(D/'check_exposure_template.py').read_text().replace("D=W/'native57'","D=W/'native58'").replace('cp56_base','cp57_base')
a=s.index('units=[];meta=[]');b=s.index("h='#define main old_main",a)
m=json.loads((D/'TEXT_LOOP_ENTRIES.json').read_text());meta=m['entries'];names=['Anne','Rudger','Kinds','Shilka','Amia','Huey','Eusis','Nei'];fn=m['function'];tx=Tx((W/'Release48/additional_roms/Anne_Adventure_-_English.bin').read_bytes());first=int(meta[0]['root'],16)
units=['extern u32 aotf%d(u32);'%fn,'typedef struct{u32 (*fn)(u32);u32 index,pc,game,n,nexit;const u32 (*scope)[2],(*exits)[2];} Unit;','static const u32 exits0[][2]={{0,0}};']
for j,game in enumerate(names[:-1]):
 root=int(next(e['root'] for e in meta if e['game']==game),16)
 units.append('static const u32 pcs%d[][2]={%s};'%(j,','.join('{%d,%d}'%(root+o,tx.insn(first+o).length) for o in m['offsets'])))
units.append('static const Unit units[]={'+','.join('{aotf%d,%d,%d,%d,19,0,pcs%d,exits0}'%(fn,e['index'],int(e['pc'],16),names.index(e['game']),names.index(e['game'])) for e in meta)+'};')
s=s[:a]+'units='+repr(units)+'\nmeta='+repr(meta)+'\n'+s[b:]
s=s.replace('aot_select_index(q->game?9:2)','aot_select_index(q->game+2)').replace("('Anne','Nei')",repr(tuple(names))).replace("'AOT_ENTRY_EXPOSURE.json'","'TEXT_LOOP_ENTRIES.json'").replace("'EXPANSION_DIFFERENTIAL.json'","'TEXT_LOOP_DIFFERENTIAL.json'").replace("'new_functions':0","'new_functions':1")
s=s.replace('if(seed==2)C.a[0]=0x1000;if(seed==3){C.a[6]=0xc00004;C.a[0]=0x1000;}if(seed==4)C.a[6]=0xc00004;', 'if(seed==2)C.a[0]=0x1000;if(seed==3)C.a[6]=0xc00004;if(seed==4)C.a[6]=0xc00004;if(seed==5)C.a[0]=0xfffffc;if(seed==6)C.a[0]=0xff9000;if(seed==7)C.a[6]=0xff9004;')
needle='C.a[7]=0xff9000;'
s=s.replace(needle,needle+'''\n if(seed!=2){static const u8 text[8][8]={{0},{10,10,0},{65,0},{255,0},{65,10,66,0},{10,65,0},{1,0},{127,10,255,0}};for(u32 k=0;k<8;k++)((u8*)md_ram)[((C.a[0]+k)&65535u)^1u]=text[seed][k];}\n''')
s=s.replace('cases==','cases==').replace('CP56','CP57')
(D/'check_text_loop.py').write_text(s)
