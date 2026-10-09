#!/usr/bin/env python3
"""Debug-only canonical hardware events, using actual local guest clocks.

No instrumentation is linked into the production ROM. Native VDP batches
are expanded into their original word writes/timestamps for comparison.
The reference interpreter is used independently, rather than treating the
fast CPU's cached C.cycles/C.pc as an instruction-level reference clock.
"""
from pathlib import Path
import subprocess,tempfile,shutil,json,struct,os

R=Path(__file__).resolve().parents[2];A=R/'source/additional';V=R.parent/'validation_cp30'
games=os.environ.get('CP30_TRACE_GAMES','Anne,Nei').split(',')
with tempfile.TemporaryDirectory(prefix='cp30-oracle-') as td:
 t=Path(td)
 decl='extern u32 *trace_cyc; void trace_event(u32,u32,u32,u32,u32); u32 trace_r(u32,u32,u32);\n'
 def instrument(src,out):
  out.mkdir();shutil.copytree(src/'aot',out/'aot')
  s=(src/'md.c').read_text();s=s.replace('#include "md.h"','#include "md.h"\n'+decl+'extern u32 trace_offset;\n#define MD_NATIVE_WORD_CLOCK(o) (trace_offset=(o))\n')
  s=s.replace('#define MD_ZTRACE(a,v) ((void)0)', '#define MD_ZTRACE(a,v) trace_event(1,((a)&0x1000000u)?16u:8u,(a)&0xffffffu,v,0)')
  s=s.replace('static void vdp_data_write(u32 v){','static void vdp_data_write(u32 v){ trace_event(1,16,0xC00000u,v,0);')
  s=s.replace('static void vdp_ctrl_write(u16 v){','static void vdp_ctrl_write(u16 v){ trace_event(1,16,0xC00004u,v,0);')
  s=s.replace('static void vdp_ctrl_write(u32 v){','static void vdp_ctrl_write(u32 v){ trace_event(1,16,0xC00004u,v,0);')
  # Two optimized VDP paths bypass vdp_data_write altogether.
  s=s.replace('u32 a=M.addr, inc=M.reg[15];','trace_event(1,16,0xC00000u,v>>16,0);trace_event(1,16,0xC00000u,v&65535u,0);\n    u32 a=M.addr, inc=M.reg[15];')
  s=s.replace('M.vgen+=vram_native_words(s,d,n);','for(u32 j=0;j<n;j++){u32 stamp=(*trace_cyc)+20u*(j+1u); trace_event(1,16,0xC00000u,md_ram[(s>>1)+2u*j],stamp);trace_event(1,16,0xC00000u,md_ram[(s>>1)+2u*j+1u],stamp);}\n  M.vgen+=vram_native_words(s,d,n);')
  s=s.replace('return v;\n}\nstatic void vdp_reg_write','return trace_r(16,0xC00000u,v);\n}\nstatic void vdp_reg_write')
  s=s.replace('return (u16)md_vdp_status();','return (u16)trace_r(16,0xC00004u,md_vdp_status());')
  s=s.replace('return (u16)((((M.line>0xEA?M.line-6:M.line)&0xFF)<<8));','return (u16)trace_r(16,0xC00008u,(((M.line>0xEA?M.line-6:M.line)&0xFF)<<8));')
  s=s.replace('return md_zram[a&0x1FFFu];','return trace_r(8,a,md_zram[a&0x1FFFu]);')
  s=s.replace('return (u8)io_read(a);','return (u8)trace_r(8,a,io_read(a));')
  s=s.replace('return (a&1u)?(u8)ee_read():0u;','return (u8)trace_r(8,a,(a&1u)?ee_read():0u);')
  s=s.replace('return 0x00;','return (u8)trace_r(8,a,0);')
  s=s.replace('if(a==0x200001u){ ee_write(v);','if(a==0x200001u){ trace_event(1,8,a,v,0);ee_write(v);')
  s=s.replace('u32 r=a&0x1Fu; M.n_io_wr++;','trace_event(1,8,a,v,0);u32 r=a&0x1Fu; M.n_io_wr++;')
  s=s.replace('if(a==0xA11100u){ mdsnd_busreq(v);','if(a==0xA11100u){ trace_event(1,8,a,v,0);mdsnd_busreq(v);')
  s=s.replace('if(a==0xA11200u){ mdsnd_reset(v);','if(a==0xA11200u){ trace_event(1,8,a,v,0);mdsnd_reset(v);')
  s=s.replace('m68k_interrupt(6);','trace_event(2,0,6,0,0);m68k_interrupt(6);').replace('m68k_interrupt(4);','trace_event(2,0,4,0,0);m68k_interrupt(4);')
  assert 'vdp_data_write(u32 v){ trace_event' in s and 'trace_event(1,16,0xC00004u,v,0)' in s
  (out/'md.c').write_text(s)
  s=(src/'m68k.c').read_text().replace('#include "md.h"','#include "md.h"\n'+decl)
  # m68k.c includes m68k.h instead on some checkpoints.
  if decl not in s:s=s.replace('#include "m68k.h"','#include "m68k.h"\n'+decl)
  s=s.replace('u32 pc=C.pc,cyc=C.cycles,sr=C.sr;','u32 pc=C.pc,cyc=C.cycles,sr=C.sr; trace_cyc=&cyc;')
  s=s.replace('C.pc=pc; C.cycles=cyc; C.sr=(u16)sr;','C.pc=pc; C.cycles=cyc; C.sr=(u16)sr;trace_cyc=0;')
  (out/'m68k.c').write_text(s)
  s=(out/'aot/generated/aot_code.c').read_text().replace('#include "aotrt.h"','#include "aotrt.h"\n'+decl)
  s=rewrite_aot_clock(s)
  (out/'aot/generated/aot_code.c').write_text(s)
 def rewrite_aot_clock(s):
  import re
  s=re.sub(r'(\n switch\(entry\))',r'\n trace_cyc=&cyc;\1',s)
  s=s.replace('C.pc=npc; return rc;','C.pc=npc;trace_cyc=0; return rc;')
  return s
 h=t/'h.c'
 h.write_text('#define main oldmain\n#include "'+str(R/'source/tools_host/md_host/mdhost.c')+'"\n#undef main\n'+r'''
u32 *trace_cyc;u32 trace_offset;static FILE *events,*frames;
void trace_event(u32 kind,u32 width,u32 addr,u32 value,u32 stamp){
 if(!events)return;u32 row[6]={M.frame,stamp?stamp:(trace_cyc?*trace_cyc+trace_offset:C.cycles),kind,width,addr,value};fwrite(row,sizeof row,1,events);
}
u32 trace_r(u32 width,u32 a,u32 v){trace_event(0,width,a,v,0);return v;}
void ref_step_real(void);
void ref_step(void){u32 *saved=trace_cyc;trace_cyc=0;ref_step_real();trace_cyc=saved;}
void mdsnd_ym_68k(u32 a,u32 v){}void mdsnd_psg_68k(u32 v){}void mdsnd_busreq(u32 v){}void mdsnd_reset(u32 v){}
int main(int argc,char **argv){events=fopen(argv[2],"wb");frames=fopen(argv[3],"wb");boot(argv[1]);
for(u32 f=0;f<1050;f++){u32 keys=(f>=180&&f<184)?K_START:(f>=400&&f<404)?K_B:(f>=500&&f<504)?K_START:(f>=600&&f%80<4)?K_A:0;md_begin_frame_input(keys);md_run(0);
#define SAVE(x) fwrite(&(x),sizeof(x),1,frames)
 SAVE(md_ram);SAVE(md_vram);SAVE(md_cram);SAVE(md_vsram);SAVE(md_zram);SAVE(md_ee);SAVE(M.reg);SAVE(C.cycles);SAVE(M.n_vint);SAVE(M.n_hint);
#undef SAVE
}return fclose(events)||fclose(frames);}
''')
 ref=t/'ref.o'
 subprocess.run(['gcc','-O2','-w','-DMD_HOST','-DAOT_ON','-Dref_step=ref_step_real','-I'+str(A),'-c',str(A/'m68k_ref.c'),'-o',str(ref)],check=True)
 for tag,src in [('before',R.parent/'cp29_base'),('after',A)]:
  out=t/tag;instrument(src,out)
  files=[h,out/'md.c',out/'m68k.c',A/'mdrender.c',out/'aot/aotdisp.c',out/'aot/generated/aot_code.c',out/'aot/generated/aot_tabs.c',ref]
  p=subprocess.run(['gcc','-O2','-w','-DMD_HOST','-DAOT_ON','-I'+str(out/'aot'),'-I'+str(A/'aot'),'-I'+str(A),*map(str,files),'-o',str(t/(tag+'.exe'))],capture_output=True,text=True);assert p.returncode==0,p.stderr
 out=t/'before';f=t/'fast_ref.o'
 subprocess.run(['gcc','-O2','-w','-DMD_HOST','-DAOT_ON','-Dm68k_exec=m68k_exec_fast','-I'+str(A),'-c',str(out/'m68k.c'),'-o',str(f)],check=True)
 # Reference scheduler and AOT dispatcher would both define m68k_exec; omit
 # dispatcher and use small selection/RGEN stubs for the independent run.
 stub=t/'stub.c';stub.write_text('#include "md.h"\nu32 aot_rgen[128],aot_ncalls,aot_ninterp;void aot_select_index(u32 i){} int aot_has(u32 pc){return 0;}\n')
 p=subprocess.run(['gcc','-O2','-w','-DMD_HOST','-DAOT_ON','-DUSE_REF_EXEC','-I'+str(A),str(h),str(out/'md.c'),str(A/'mdrender.c'),str(f),str(ref),str(stub),'-o',str(t/'reference.exe')],capture_output=True,text=True);assert p.returncode==0,p.stderr
 def compare(a,b,stride):
  off=0
  with a.open('rb') as x,b.open('rb') as y:
   while chunk:=x.read(1<<20):
    other=y.read(len(chunk))
    if chunk!=other:
     n=off+next((i for i,(u,v) in enumerate(zip(chunk,other)) if u!=v),min(len(chunk),len(other)))
     if stride:
      with a.open('rb') as aa,b.open('rb') as bb:aa.seek(n//stride*stride);bb.seek(n//stride*stride);detail=[list(struct.unpack('<6I',z.read(stride))) for z in [aa,bb]]
     else:detail={'frame':n//(a.stat().st_size//1050),'byte':n%(a.stat().st_size//1050)}
     return {'identical':False,'first_byte':n,'detail':detail}
    off+=len(chunk)
   return {'identical':not y.read(1),'bytes_compared':off}
 rows=[]
 for game in games:
  for tag in ['reference','repeat','before','after']:
   exe='reference' if tag=='repeat' else tag
   subprocess.run([str(t/(exe+'.exe')),str(R/f'additional_roms/{game}_Adventure_-_English.bin'),str(t/(game+tag+'.events')),str(t/(game+tag+'.frames'))],check=True)
  row={'game':game}
  for label,x,y in [('determinism','reference','repeat'),('CP29_reference','reference','before'),('CP30_reference','reference','after'),('CP30_CP29','before','after')]:
   row[label]={kind:compare(t/(game+x+'.'+kind),t/(game+y+'.'+kind),24 if kind=='events' else 0) for kind in ['events','frames']}
  rows.append(row);print(json.dumps(row),flush=True)
 report={'production_instrumentation':False,'guest_frames':1050,'clock':'local interpreter/AOT cycle accumulator; native VDP batches expanded at original MOVE write clocks','event_columns':['frame','guest_cycle','kind','width','address','value'],'cases':rows}
 (V/'HARDWARE_TRANSCRIPT.json').write_text(json.dumps(report,indent=2)+'\n')
 assert all(all(v['identical'] for v in row['determinism'].values()) and all(v['identical'] for v in row['CP30_CP29'].values()) and row['CP30_reference']['events']['identical'] for row in rows)
