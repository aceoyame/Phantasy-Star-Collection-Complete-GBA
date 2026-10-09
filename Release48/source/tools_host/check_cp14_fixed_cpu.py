#!/usr/bin/env python3
"""Complete CPU, bus, EEPROM and RAM states against sealed CP13; no hashes."""
from pathlib import Path
import concurrent.futures,json,subprocess,tempfile,os,struct
R=Path(__file__).resolve().parents[2];A=R/'source/additional';REF=R/'source/references/cp13_final'
games=os.environ.get('CP14_CPU_GAMES','Anne,Rudger,Kinds,Shilka,Amia,Huey,Eusis,Nei').split(',')
with tempfile.TemporaryDirectory(prefix='cp13-cpu-') as tmp:
 t=Path(tmp);h=t/'harness.c'
 h.write_text('''#define main legacy_mdhost_main
#include "'''+str(R/'source/tools_host/md_host/mdhost.c')+'''"
#undef main
extern u32 aot_rgen[128];
void mdsnd_ym_68k(u32 a,u32 v){}
void mdsnd_psg_68k(u32 v){}
void mdsnd_busreq(u32 v){}
void mdsnd_reset(u32 v){}
int main(int argc,char **argv){
 boot(argv[1]);FILE *out=fopen(argv[2],"wb");if(!out)return 2;
 for(u32 f=0;f<1050;f++){
  u32 keys=(f>=180&&f<184)?K_START:(f>=400&&f<404)?K_B:(f>=500&&f<504)?K_START:(f>=600&&f%80<4)?K_A:0;
  md_begin_frame_input(keys);md_run(0);
#define SAVE(v) if(fwrite(&(v),sizeof(v),1,out)!=1)return 3
  M68K cpu=C;MD bus=M;
  cpu.spin_tgt=cpu.spin_fx=cpu.spin_cyc=cpu.spin_valid=cpu.spin_lrd=0;
  cpu.idle_ok=cpu.rcp_per=cpu.rcp=cpu.spin_wait=cpu.spin_back=0;
  memset(cpu.spin_regs,0,sizeof cpu.spin_regs);cpu.spin_skips=cpu.spin_cycles=0;
  memset(cpu.ring,0,sizeof cpu.ring);cpu.ring_pos=0;
  memset(bus.evctx,0,sizeof bus.evctx);
  SAVE(cpu);SAVE(bus);SAVE(md_ee);SAVE(md_ram);SAVE(md_vram);
  SAVE(md_cram);SAVE(md_vsram);SAVE(md_zram);SAVE(aot_rgen);
#undef SAVE
 }
 return fclose(out)!=0;
}
''')
 for tag,src in [('before',REF),('after',A)]:
  files=[h,src/'md.c',src/'m68k.c',A/'m68k_ref.c',A/'mdrender.c',src/'aot/aotdisp.c',src/'aot/generated/aot_code.c',src/'aot/generated/aot_tabs.c']
  if not (src/'aot/generated/aot_tabs.c').exists():files.append(A/'aot/generated/aot_tabs.c')
  p=subprocess.run(['gcc','-O2','-w','-DMD_HOST','-DAOT_ON','-ffunction-sections','-fdata-sections','-I'+str(src/'aot'),'-I'+str(A/'aot'),'-I'+str(A),*map(str,files),'-Wl,--gc-sections','-o',str(t/tag)],capture_output=True,text=True)
  if p.returncode:raise RuntimeError(p.stderr)
 def run(game):
  paths=[]
  for tag in ['before','after']:
   p=t/(game+'_'+tag+'.state');paths.append(p)
   subprocess.run([str(t/tag),str(R/f'additional_roms/{game}_Adventure_-_English.bin'),str(p)],check=True,capture_output=True)
  same=paths[0].stat().st_size==paths[1].stat().st_size
  with paths[0].open('rb') as a,paths[1].open('rb') as b:
   pos=0
   while x:=a.read(1048576):
    y=b.read(len(x))
    if x!=y:
     same=False;off=pos+next(i for i,(v,w) in enumerate(zip(x,y)) if v!=w);stride=paths[0].stat().st_size//1050
     print(game,'first difference frame',off//stride,'state byte',off%stride)
     for tag,p in zip(['before','after'],paths):
      with p.open('rb') as f:f.seek((off//stride)*stride);state=f.read(stride)
      (R.parent/f'validation_cp14/{game}_{tag}_first.state').write_bytes(state)
      cpu=struct.unpack('<20I',state[:80]);print(tag,'regs/usp/ssp/pc/cycles',*[hex(v) for v in cpu], 'SR',hex(struct.unpack('<H',state[80:82])[0]))
     break
    pos+=len(x)
  row={'game':game,'frames':1050,'bytes_compared':paths[0].stat().st_size,'identical':same}
  for p in paths:p.unlink()
  return row
 with concurrent.futures.ThreadPoolExecutor(max_workers=4) as ex:rows=list(ex.map(run,games))
 result={'excluded_snapshot_fields':'spin optimization state/counters, branch history and event branch context; CPU registers, SR, PC, cycles, bus counters, EEPROM and all memories compared exactly','all_pass':all(row['identical'] for row in rows),'cases':rows}
 (R.parent/'validation_cp14/CPU_FIXED_SCRIPT_EXACT.json').write_text(json.dumps(result,indent=2)+'\n')
 for row in rows:print(row['game'],'full-state exact' if row['identical'] else 'FAIL')
 assert result['all_pass']
