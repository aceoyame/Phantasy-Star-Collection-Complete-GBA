from pathlib import Path
import re,subprocess,json,sys
W=Path(__file__).resolve().parents[1];A=W/'Release48/source/additional';D=W/'native57';B=D/'expansion_test';B.mkdir(exist_ok=True)
sys.path.insert(0,str(A/'aot'));from aot68k import Tx
code=(A/'aot/generated/aot_code.c').read_text();tabs=(A/'aot/generated/aot_tabs.c').read_text();oldtabs=(W/'cp56_base/aot_tabs.c').read_text()
targets={(e['fn'],e['index'],int(e['pc'],16)) for e in json.loads((D/'AOT_ENTRY_EXPOSURE.json').read_text())['entries']}
common=re.search(r'rom_common\[\]=\{(.*?)\};',tabs,re.S)[1]
known={int(x,16) for x in re.findall(r'\{0x([0-9a-f]+)u,',oldtabs)}
roms={g:(W/'Release48/additional_roms'/f'{g}_Adventure_-_English.bin').read_bytes() for g in ('Anne','Nei')}
units=[];meta=[]
for fn in [534]:
 body=re.search(r'u32 aotf'+str(fn)+r'\(u32 entry\)\{(.*?)(?=\n__attribute__|\Z)',code,re.S)[1]
 game='Anne' if re.search(r'\{0x[0-9a-f]+u,'+str(fn)+r',',common) else 'Nei'
 tx=Tx(roms[game]);pcs=[int(x,16) for x in re.findall(r'^B([0-9A-F]+):',body,re.M)]
 entries=[(int(i),int(pc,16)) for i,pc in re.findall(r'case (\d+): goto B([0-9A-F]+);',body)]
 edges=[]
 for pm in re.finditer(r'^B([0-9A-F]+):(.*?)(?=^B[0-9A-F]+:|^ex:)',body,re.M|re.S):
  edges.extend((int(pm[1],16),int(t,16)) for t in re.findall(r'npc=hi\|0x([0-9A-Fa-f]+)u; goto out;',pm[2]))
 units.append('static const u32 exits%d[][2]={%s};'%(fn,','.join('{%du,%du}'%e for e in edges) if edges else '{0,0}'))
 maps=','.join('{%du,%du}'%(p,tx.insn(p).length) for p in pcs)
 units.append('extern u32 aotf%d(u32);\nstatic const u32 pcs%d[][2]={%s};'%(fn,fn,maps))
 for index,pc in entries:
  if (fn,index,pc) in targets:meta.append((fn,index,pc,game,len(pcs),len(edges)))
units.append('typedef struct{u32 (*fn)(u32);u32 index,pc,game,n,nexit;const u32 (*scope)[2],(*exits)[2];} Unit;')
units.append('static const Unit units[]={'+','.join('{aotf%d,%d,%d,%d,%d,%d,pcs%d,exits%d}'%(fn,index,pc,0 if game=='Anne' else 1,n,ne,fn,fn) for fn,index,pc,game,n,ne in meta)+'};')
units.append('static const u32 known[]={'+','.join(map(str,sorted(known)))+'};')
h='#define main old_main\n#include "'+str((W/'Release48/source/tools_host/md_host/mdhost.c').resolve())+'"\n#undef main\n'+r'''
void mdsnd_ym_68k(u32 a,u32 v){}void mdsnd_psg_68k(u32 v){}void mdsnd_busreq(u32 v){}void mdsnd_reset(u32 v){}
extern u32 aot_lim,aot_rgen[128];
'''+ '\n'.join(units)+r'''
int main(int argc,char**argv){u32 cases=0;
 const u32 budgets[]={0,1,3,4,7,8,11,12,15,16,23,31,64,257};
 for(u32 u=0;u<sizeof(units)/sizeof(Unit);u++){const Unit*q=&units[u];boot(argv[q->game+2]);
 for(u32 seed=0;seed<8;seed++)for(u32 b=0;b<sizeof(budgets)/4;b++){
 md_reset_game();aot_select_index(q->game?9:2);memset(aot_rgen,0,512);
 for(u32 j=0;j<32768;j++){md_ram[j]=(u16)(j*271u+cases*83u);md_vram[j]=(u16)(j*17u);}
 for(u32 r=0;r<8;r++){C.d[r]=(seed*0x7654321u+r*0x1234567u);C.a[r]=0xff8000+r*0x100+(seed&1)*2;}
 C.d[0]=(C.d[0]&0xffff0000)|(seed*3);C.d[1]=(C.d[1]&0xffff0000)|(seed*5);C.d[7]=(C.d[7]&0xffff0000)|(seed*7);
 if(seed==2)C.a[0]=0x1000;if(seed==3){C.a[6]=0xc00004;C.a[0]=0x1000;}if(seed==4)C.a[6]=0xc00004;
 C.a[7]=0xff9000;M.code=seed%3==0?1:seed%3==1?3:5;M.reg[15]=seed&1?2:4;
 C.sr=0x2700|(seed*7&31);C.pc=q->pc;C.cycles=seed&1?0xffffffe0:0;C.spin_enable=0;aot_lim=C.cycles+budgets[b];
 if(argv[1][0]=='n')q->fn(q->index);
 else while((s32)(C.cycles-aot_lim)<0){
  u32 prior=C.pc,len=0;for(u32 k=0;k<q->n;k++)if(q->scope[k][0]==prior){len=q->scope[k][1];break;}
  if(!len)break;m68k_exec_interp(C.cycles+1u);
  u32 hit=0;for(u32 k=0;k<q->nexit;k++)if(q->exits[k][0]==prior&&q->exits[k][1]==C.pc){hit=1;break;}if(hit)break;
 }
 memset(M.evctx,0,sizeof M.evctx);
 C.spin_tgt=C.spin_fx=C.spin_cyc=C.spin_valid=C.spin_lrd=C.rcp_per=C.rcp=C.spin_wait=C.spin_back=0;memset(C.spin_regs,0,sizeof C.spin_regs);memset(C.ring,0,sizeof C.ring);C.ring_pos=0;
#define OUT(v) fwrite(&(v),sizeof(v),1,stdout)
 OUT(C);OUT(M);OUT(md_ee);OUT(md_ram);OUT(md_vram);OUT(md_cram);OUT(md_vsram);OUT(md_zram);OUT(md_vdirty);OUT(aot_rgen);
#undef OUT
 cases++;
 }}fprintf(stderr,"%u cases\n",cases);return 0;}
'''
(B/'harness.c').write_text(h)
files=[B/'harness.c']+[A/n for n in ('md.c','m68k.c','m68k_ref.c','mdrender.c','aot/aotdisp.c','aot/generated/aot_code.c','aot/generated/aot_tabs.c')]
subprocess.run(['gcc','-O2','-w','-DMD_HOST','-DAOT_ON','-ffunction-sections','-fdata-sections','-I'+str(A),'-I'+str(A/'aot'),*map(str,files),'-Wl,--gc-sections','-o',str(B/'check')],check=True)
args=[str(W/'Release48/additional_roms'/f'{g}_Adventure_-_English.bin') for g in ('Anne','Nei')]
ps=[subprocess.Popen([str(B/'check'),m,*args],stdout=subprocess.PIPE,stderr=subprocess.PIPE) for m in ('ref','native')];total=0
while True:
 a=ps[0].stdout.read(1<<20);b=ps[1].stdout.read(len(a))
 if a!=b:
  ix=next((i for i,(x,y) in enumerate(zip(a,b)) if x!=y),min(len(a),len(b)));raise AssertionError(('difference byte',total+ix,'case',(total+ix)//141116,'within',(total+ix)%141116))
 if not a:break
 total+=len(a)
logs=[p.stderr.read().decode() for p in ps];assert all(p.wait()==0 for p in ps)
r={'cases':int(logs[0].split()[0]),'function_entry_points':len(meta),'new_functions':0,'bytes_compared':total,'CP56_single_instruction_CPU_bus_memories_exact':True,'coverage':'Every new native/private entry, fourteen short/long budgets, seeded registers/CCR, RAM/ROM/MMIO operands and cycle wrap. CP56 fast-interpreter single steps stop at the same emitted region exits.'}
(D/'EXPANSION_DIFFERENTIAL.json').write_text(json.dumps(r,indent=2)+'\n');print(r)
