#!/usr/bin/env python3
"""Complete CPU, bus, EEPROM and RAM states against CP12; no hashes."""
from pathlib import Path
import concurrent.futures,json,subprocess,tempfile
R=Path(__file__).resolve().parents[2];A=R/'source/additional';REF=R/'source/references/cp12_speed'
games=['Anne','Rudger','Kinds','Shilka','Amia','Huey','Eusis','Nei']
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
 for(u32 f=0;f<350;f++){
  u32 keys=(f%211<4)?K_START:(f%137<3)?K_A:(f%173<2)?K_DOWN:(f%251<3)?K_B:0;
  md_begin_frame_input(keys);md_run(0);
#define SAVE(v) if(fwrite(&(v),sizeof(v),1,out)!=1)return 3
  SAVE(C);SAVE(M);SAVE(md_ee);SAVE(md_ram);SAVE(md_vram);
  SAVE(md_cram);SAVE(md_vsram);SAVE(md_zram);SAVE(aot_rgen);
#undef SAVE
 }
 return fclose(out)!=0;
}
''')
 for tag,src in [('before',REF),('after',A)]:
  files=[h,src/'md.c',src/'m68k.c',A/'m68k_ref.c',A/'mdrender.c',src/'aot/aotdisp.c',*sorted((src/'aot/generated').glob('*.c'))]
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
   while x:=a.read(1048576):
    if x!=b.read(len(x)):same=False;break
  row={'game':game,'frames':350,'bytes_compared':paths[0].stat().st_size,'identical':same}
  for p in paths:p.unlink()
  return row
 with concurrent.futures.ThreadPoolExecutor(max_workers=4) as ex:rows=list(ex.map(run,games))
 result={'all_pass':all(row['identical'] for row in rows),'cases':rows}
 (R.parent/'validation_cp13/CPU_BUS_STATE_EXACT.json').write_text(json.dumps(result,indent=2)+'\n')
 for row in rows:print(row['game'],'full-state exact' if row['identical'] else 'FAIL')
 assert result['all_pass']
