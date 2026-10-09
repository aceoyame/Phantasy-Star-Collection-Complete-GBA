from pathlib import Path
import subprocess,json,re
W=Path(__file__).resolve().parents[1];A=W/'Release48/source/additional';D=W/'native58';B=D/'nibble_test';B.mkdir(exist_ok=True)
names=['Anne','Rudger','Kinds','Shilka','Amia','Huey','Eusis','Nei'];tabs=(W/'cp57_base/aot_tabs.c').read_text();rows=[]
for game in names:
 group='rom_nei' if game=='Nei' else 'rom_common'
 for tab in (group,'xrom_'+game.lower()):
  body=re.search(r'static const AotEnt AOT_TAB '+tab+r'\[\]=\{(.*?)\};',tabs,re.S)[1]
  for pc,fn,ix,rr in re.findall(r'\{(0x[0-9a-f]+)u,(\d+),(\d+),(\d+)\}',body):
   if int(fn) in (33,):rows.append((int(pc,16),int(fn),int(ix),names.index(game)))
units='\n'.join('extern u32 aotf%d(u32);'%fn for fn in (33,))+'\ntypedef struct{u32 pc;u32(*fn)(u32);u32 ix,game;}Unit;\nstatic const Unit units[]={'+','.join('{%d,aotf%d,%d,%d}'%r for r in rows)+'};\n'
h='#define main old_main\n#include "'+str(W/'Release48/source/tools_host/md_host/mdhost.c')+'"\n#undef main\n'+r'''
void mdsnd_ym_68k(u32 a,u32 v){}void mdsnd_psg_68k(u32 v){}void mdsnd_busreq(u32 v){}void mdsnd_reset(u32 v){}
extern u32 aot_lim,aot_rgen[128];
'''+units+r'''
int main(int argc,char**argv){u32 cases=0;const u32 budgets[]={0,1,3,4,7,8,11,12,15,16,17,23,31,32,33,47,48,63,64,29,30,59,60,239,240,241,257,1024};
const u32 srcs[]={0,4,0xff8,0xffc,0xffe,0x3fff8,0x3fffc,0xff0000,0xff8000,0xffbffc,0xfffff8,0xfffffc,0xfffffe,0xefa002,0xc00000,0xc00004};
for(u32 u=0;u<sizeof(units)/sizeof(Unit);u++){const Unit*q=&units[u];boot(argv[q->game+1]);
for(u32 seed=0;seed<16;seed++)for(u32 addr=0;addr<1;addr++)for(u32 b=0;b<sizeof(budgets)/4;b++){
md_reset_game();aot_select_index(q->game+2);memset(aot_rgen,0,512);
for(u32 j=0;j<32768;j++){md_ram[j]=(u16)(j*271u+cases*83u);md_vram[j]=(u16)(j*17u);}
for(u32 r=0;r<8;r++){C.d[r]=seed*0x7654321u+r*0x1234567u;C.a[r]=0xff8000+r*0x100;}
C.a[0]=srcs[addr];C.a[7]=0xff9000;C.d[0]=seed&1?0xfffffff0u:seed==2?0x7fffffffu:seed==4?0x80000000u:0;
C.d[0]=0xa5a50000u|(seed%8==0?0u:seed%8==1?1u:seed%8==2?0xffffu:seed%8==3?7u:seed%8==4?8u:seed*3u);
C.d[1]=seed*0x13579u;C.d[3]=0x5a5a0000u|(seed%8==0?0u:seed%8==1?1u:seed%8==2?2u:seed%8==3?3u:seed%8==4?7u:seed%8==5?8u:seed%8==6?0x8000u:0xffffu);C.d[4]=seed*0x1234567u;
C.sr=0x2700|(seed*7&31);C.pc=q->pc;C.cycles=seed&1?0xffffffe0:0;C.spin_enable=0;aot_lim=C.cycles+budgets[b];
M.code=seed%3==0?1:seed%3==1?3:5;M.reg[15]=seed&1?2:4;
q->fn(q->ix);
#define OUT(v) fwrite(&(v),sizeof(v),1,stdout)
OUT(C);OUT(M);OUT(md_ee);OUT(md_ram);OUT(md_vram);OUT(md_cram);OUT(md_vsram);OUT(md_zram);OUT(md_vdirty);OUT(aot_rgen);
#undef OUT
cases++;
}}fprintf(stderr,"%u cases\n",cases);return 0;}
'''
(B/'harness.c').write_text(h)
for mode in ('ref','native'):
 if mode=='ref':
  (B/'md.c').write_text((W/'cp57_base/md.c').read_text());(B/'aot_code.c').write_text((W/'cp57_base/aot_code.c').read_text());(B/'aot_tabs.c').write_text((W/'cp57_base/aot_tabs.c').read_text())
 files=[B/'harness.c',B/'md.c' if mode=='ref' else A/'md.c',A/'m68k.c',A/'m68k_ref.c',A/'mdrender.c',A/'aot/aotdisp.c',B/'aot_code.c' if mode=='ref' else A/'aot/generated/aot_code.c',B/'aot_tabs.c' if mode=='ref' else A/'aot/generated/aot_tabs.c']
 subprocess.run(['gcc','-O2','-w','-DMD_HOST','-DAOT_ON','-ffunction-sections','-fdata-sections','-I'+str(A),'-I'+str(A/'aot'),*map(str,files),'-Wl,--gc-sections','-o',str(B/mode)],check=True)
args=[str(W/'Release48/additional_roms'/f'{g}_Adventure_-_English.bin') for g in names]
ps=[subprocess.Popen([str(B/m),*args],stdout=subprocess.PIPE,stderr=subprocess.PIPE) for m in ('ref','native')];total=0
while True:
 a=ps[0].stdout.read(1<<20);b=ps[1].stdout.read(len(a))
 if a!=b:
  i=next((i for i,(x,y) in enumerate(zip(a,b)) if x!=y),min(len(a),len(b)));raise AssertionError(('difference',total+i,'case',(total+i)//141116,'within',(total+i)%141116))
 if not a:break
 total+=len(a)
logs=[p.stderr.read().decode() for p in ps];assert all(p.wait()==0 for p in ps)
r={'cases':int(logs[0].split()[0]),'entry_maps':len(rows),'bytes_compared':total,'CP57_original_AOT_CPU_CCR_bus_memories_exact':True,'coverage':'All common-game decoder maps and entry phases; small/large repeat and flush counts, decoder bytes, full CCR including signed overflow, partial iteration budgets and cycle wrap.'};(D/'NIBBLE_DIFFERENTIAL.json').write_text(json.dumps(r,indent=2)+'\n');print(r)
