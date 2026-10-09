from pathlib import Path
import subprocess,re,zlib,json
W=Path(__file__).resolve().parents[1];D=W/'native58';A=W/'Release48/source/additional';B=W/'Release48/compiler/root/usr/lib/llvm-18/bin'
# Final release and measured engine must match byte for byte.
assert (A/'build/md.elf').read_bytes()==(D/'armcheck/md.elf').read_bytes()
m=(D/'armcheck/md.map').read_text();nm=subprocess.run([str(B/'llvm-nm'),str(D/'armcheck/md.elf')],capture_output=True,text=True,check=True).stdout
symbols={p[2]:int(p[0],16) for l in nm.splitlines() if len(p:=l.split())==3}
ends={'rom':symbols['__rom_end']-0x8000000}
for sec in ('zaot','aotmore','aotextra','rom2'):
 x=re.search(r'^\s*([0-9a-f]+)\s+[0-9a-f]+\s+([0-9a-f]+)\s+\d+\s+\.'+sec+r'$',m,re.M);ends[sec]=int(x[1],16)+int(x[2],16)-0x8000000
limits={'rom':0x730000,'zaot':0x78000,'aotmore':0x7c000,'aotextra':0x6e2000,'rom2':0x2000000}
padding=[(ends[k],limits[k]) for k in ends];release=(W/'RUN_THIS_R48_CP58_NATIVE_TEXT_KERNELS.gba').read_bytes();candidate=(D/'internal_candidate.gba').read_bytes();parent=(W/'RUN_THIS_R48_CP57_NATIVE_WINDOW_LOOPS.gba').read_bytes()
assert len(release)==len(candidate)==len(parent)==33554432
diffs=[i for i,(a,b) in enumerate(zip(release,candidate)) if a!=b]
assert all(any(lo<=i<hi for lo,hi in padding) for i in diffs)
regions={'launcher_and_wave_bank':(0,0x60000),'PSIII_panel':(0x78000,0x79000),'PS1_Gaiden_and_SMS_GG_engine':(0x7c000,0x6dbef8),'upper_original_content_bank':(0x730000,0x1fe6750)}
checks={name:release[lo:hi]==parent[lo:hi] for name,(lo,hi) in regions.items()};assert all(checks.values())
unchanged=['mdsnd.c','mdsndmix.c','aotdisp.c'];assert all((A/('aot/'+f if f=='aotdisp.c' else f)).read_bytes()==(W/'cp57_base'/f).read_bytes() for f in unchanged)
r={'release_ELF_byte_exact_with_measured_ELF':True,'unused_padding_differences':len(diffs),'padding_difference_regions':padding,'ROM_size':len(release),'ROM_crc32':f'{zlib.crc32(release):08X}','byte_exact_regions':checks,'sound_and_dispatch_sources_byte_exact_parent':True}
(D/'PRESERVATION.json').write_text(json.dumps(r,indent=2)+'\n');print(r)
