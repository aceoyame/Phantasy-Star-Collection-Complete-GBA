"""Package CP58. SHA256 is computed only once, for the final ROM."""
from pathlib import Path
import json,re,zlib,hashlib,zipfile,subprocess,os
W=Path(__file__).resolve().parents[1];N=W/'native58';R=W/'Release48';A=R/'source/additional'
rom=W/'RUN_THIS_R48_CP58_NATIVE_TEXT_KERNELS.gba';data=rom.read_bytes();rows=json.loads((N/'gba_results/GBA_COMPARISON.json').read_text())['rows']
assert len(rows)==8 and all(all(r[e+'_exact'] for e in ('state','pixels','pcm')) for r in rows)
for f,n in [('TEXT_LOOP_DIFFERENTIAL.json',14896),('CHECKSUM_DIFFERENTIAL.json',37632),('NIBBLE_DIFFERENTIAL.json',40768),('SHIFT_ARM_DIFFERENTIAL.json',3600)]:assert json.loads((N/f).read_text())['cases']==n
assert 'PASS 22528' in (N/'final_renderer_checks.log').read_text()
assert json.loads((N/'SAVE_RELOAD_CHECK.json').read_text())['all_pass']
assert json.loads((N/'PRESERVATION.json').read_text())['release_ELF_byte_exact_with_measured_ELF']
assert (A/'build/md.elf').read_bytes()==(N/'armcheck/md.elf').read_bytes()
a=sum(t['cycles'] for r in rows for t in r['baseline_CP57']);b=sum(t['cycles'] for r in rows for t in r['candidate']);ratios=[100*t['guest_per_display_frame'] for r in rows for t in r['candidate']]
# Preserve the one final-ROM SHA if packaging is rerun without changing it.
crc=f'{zlib.crc32(data):08X}';old=json.loads((N/'FINAL_SUMMARY.json').read_text()) if (N/'FINAL_SUMMARY.json').exists() else {}
sha=old['SHA256'] if old.get('CRC32')==crc and old.get('ROM_bytes')==len(data) else hashlib.sha256(data).hexdigest()
maptext=(N/'armcheck/md.map').read_text();B=R/'compiler/root/usr/lib/llvm-18/bin';env=dict(os.environ,LD_LIBRARY_PATH=str(R/'compiler/root/usr/lib/x86_64-linux-gnu'))
nm=subprocess.run([str(B/'llvm-nm'),str(N/'armcheck/md.elf')],env=env,capture_output=True,text=True,check=True).stdout
symbols={p[2]:int(p[0],16) for l in nm.splitlines() if len(p:=l.split())==3};space={}
for name,sym,cap in [('IWRAM','__hot_end',0x3007980),('EWRAM','__bss_end',0x203e000),('main_ROM','__rom_end',0x8730000)]:
 end=symbols[sym];assert end<cap;space[name+'_end']=f'{end:08X}';space[name+'_spare_bytes']=cap-end
for name,cap in [('zaot',0x8078000),('aotmore',0x807c000),('aotextra',0x86e2000),('rom2',0xa000000)]:
 m=re.search(r'^\s*([0-9a-f]+)\s+[0-9a-f]+\s+([0-9a-f]+)\s+\d+\s+\.'+name+r'$',maptext,re.M);end=int(m[1],16)+int(m[2],16);assert end<=cap;space[name+'_end']=f'{end:08X}';space[name+'_spare_bytes']=cap-end
parent_min=min(t['guest_per_display_frame']*100 for r in rows for t in r['baseline_CP57'])
summary={'checkpoint':'CP58','parent':'CP57 4CA93430','weighted_cycle_reduction_percent':100*(a-b)/a,'throughput_increase_percent':100*(a/b-1),'baseline_cycles':a,'candidate_cycles':b,'cycles_saved':a-b,'all_24_windows_faster':False,'state_pixels_PCM_exact':True,'tested_full_speed_percent_range':[min(ratios),max(ratios)],'parent_minimum_percent':parent_min,'minimum_throughput_increase_percent':100*(min(ratios)/parent_min-1),'new_minimum_window':{'game':'Anne','start':450,'end':750,'final_percent':min(ratios)},'maximum_window_cycle_regression_percent':max(-v for r in rows for v in r['cycle_reduction_percent']),'memory':space,'ROM_bytes':len(data),'CRC32':crc,'SHA256':sha}
(N/'FINAL_SUMMARY.json').write_text(json.dumps(summary,indent=2)+'\n')
names=['Anne','Rudger','Kinds','Shilka','Amia','Huey','Eusis','Nei'];table='\n'.join('| '+name+' | '+' | '.join(f"{100*t['guest_per_display_frame']:.4f}%" for t in row['candidate'])+' |' for name,row in zip(names,rows))
profile=json.loads((N/'CP58_FINAL_2_450_750.json').read_text());prose=', '.join(f"{f['name']} {f['percent']:.2f}%" for f in profile['functions'][:10])
text=f'''# CP58 — native text and decoder kernels

Authoritative continuation of CP57 (CRC32 4CA93430). Final ROM: `{rom.name}`.
Size: {len(data):,} bytes. CRC32: **{crc}**. SHA256: `{sha}`.

## Result
The minimum across the same 24 matched measurement windows rises from **{parent_min:.4f}% to {min(ratios):.4f}%** of full speed, a **{summary['minimum_throughput_increase_percent']:.3f}%** floor throughput gain. The minimum returns to Anne's 450–750 help-screen window. That original CP56 37.3257% window is now **{min(ratios):.4f}%**. Anne's opening window rises from 40.5730% to **{ratios[0]:.4f}%**. Across all measured windows, throughput rises **{summary['throughput_increase_percent']:.4f}%** over CP57.

This remains below full speed. Results are actual ARM7 execution in mGBA under a fixed input script, not physical GBA timings or a guarantee about every scene. Seven later windows have small regressions, at most **{summary['maximum_window_cycle_regression_percent']:.4f}%** additional cycles; all opening and middle windows improve. Avoid claiming that all 24 windows are faster.

## Accepted implementation
1. Shared text output: one native Thumb function (aotf597) implements the byte-identical 50-byte, 19-instruction routine at seven relocated adventure addresses. 133 entry maps are enabled. The original character reads, newline handling, glyph call, VRAM writes, stack restore and full CCR remain intact. Relative entry offsets reconstruct each game's PC/return address. Nei differs and retains its existing path. The bitmap pool remains 34 live rows.
2. Shared checksum: six originally identical AOT bodies (86/87/88/89/90/92) share one implementation through function aliases. md_native_checksum batches only complete 16-cycle ADD.L/DBRA iterations over aligned ordinary ROM/RAM. ROM batching stops at a 4 KiB page/end boundary; RAM at its 64 KiB boundary. Partial budgets, halfword alignment, wrap and MMIO retain the original reads. The final addition restores every CCR bit, including overflow. No writes/events are elided. Alias reuse recovers main ROM space.
3. Packed-nibble decoder: md_native_repeat_nibble batches complete 30-cycle iterations of aotf33's register-only repeat loop. It stops before the zero-count flush branch or the current cycle limit. Eight or more repeats have an exact closed form; shorter runs execute the same recurrence. Terminal counter, full CCR, signed-overflow and partial-instruction boundaries are preserved.
4. Cold shift placement: do_shift moves from IWRAM into ordinary ROM, freeing 424 bytes. The body and ABI are retained. It is rare in measured profiles; this trades a small later-window cost for room to speed opaque text rendering.
5. Renderer: a guarded ARM single320_opaque emitter packs the exact six source pixels used for each 8-pixel cell into three pixel pairs. Eligibility requires the existing full-width, opaque, high-priority window proof and absence of overlapping sprites. The generic compositor resolves high-priority sprite/window pixels before decoding plane B. Transparency, mixed priority, sprites and other modes keep their existing composition rules.

The sound sources, wave bank, guest dispatcher, scheduler, PS1/Gaiden/SMS-GG engine, PSIII panel and upper original content bank are preserved. No sound quality, frame budget, stack boundary or memory cap was relaxed.

## Measurements
Eight games × 1,050 guest frames; identical scripted inputs and audio calls. Windows are 300 guest frames. 100% means one guest frame per GBA display frame. Overall cycles saved: {a-b:,}/{a:,} ({100*(a-b)/a:.4f}%).

| Game | 150–450 | 450–750 | 750–1050 |
|---|---:|---:|---:|
{table}

## Verification
- All eight adventures: exact captured CPU/game/memory state, pixels/palette and stereo PCM versus CP57.
- Shared text output: 14,896 differential cases, all 133 relocated entry maps, partial budgets, cycle wrap, newline/NUL/byte values, RAM/ROM/wrap operands, VRAM writes and stack overlap.
- Checksum: 37,632 differential cases, seven game maps, both entry phases, ROM page/end, RAM wrap/alignment, MMIO fallback, carry/overflow/sign/zero and cycle wrap.
- Packed-nibble loop: 40,768 differential cases, all 91 common-game entry maps, repeat/flush counts, decoder bytes, full CCR and partial cycle budgets. Each compares the complete function against CP57's original AOT.
- 3,600 actual ARM calls verify relocated do_shift's output, full CCR and stack ABI.
- 22,528 original CP07 renderer comparisons include window modes, priorities, transparency, flips, scroll, sprites and both framebuffer pages; 672 directed lines exercise the opaque cover guard.
- All eight EEPROM save slots preserve a non-default saved word through power-cycle/reload and leave unrelated save regions untouched.
- Full canonical build ELF is byte-identical to the measured ELF. Candidate/release ROM differences are confined to unused section tails. Preserved ROM banks are byte-identical to CP57.

## Space and report correction
IWRAM end {space['IWRAM_end']}: **{space['IWRAM_spare_bytes']} bytes spare**.
EWRAM end {space['EWRAM_end']}: {space['EWRAM_spare_bytes']} bytes spare.
Main ROM end {space['main_ROM_end']}: {space['main_ROM_spare_bytes']} bytes spare.
ZAOT spare {space['zaot_spare_bytes']}; AOTMORE spare {space['aotmore_spare_bytes']}; AOTEXTRA spare {space['aotextra_spare_bytes']}; ROM2 spare {space['rom2_spare_bytes']}.

CP57's packaging report misread the first linker-map column for __rom_end and reported an EWRAM address as the main ROM end. Its actual main ROM end was 0872FF24, only 220 bytes spare. CP58 reads the absolute symbol from llvm-nm. This corrects reporting; actual linked blobs and ROM section caps were enforced throughout.

## Rejected experiments
- Packing dispatcher cache index/round-robin into one word gave inconsistent gains and worsened the target window. Final aotdisp.c is byte-identical to CP57.
- Tracking opaque windows to suppress background-only dirty lines passed host renderer comparisons but slowed the target ARM measurement. It is rejected. The final renderer has no hidden_window_lines/window_tiles metadata. Rejected source, checks and stage reports are retained only as research.

## Continue from here
The slow help-screen window remains dominated by graphics. Its final profile begins: {prose}. Background dirty suppression did not pay for its bookkeeping. Profile the actual remaining generic render_line paths, tile fetching and clipping before adding more metadata. Opening-window checksum and repeat-decoder loops are now native transactions; more gains require other measured guest leaves or less host compositor work. Existing PS4 speed is not evidence these Genesis CPU/VDP paths have the same costs.

Final source is authoritative. Do not rerun exploratory installers over it. `native58/measure_batch.py` uses CP57/cp57_base as reference and native58/internal_candidate.gba with native58/armcheck/md.elf as candidate. Clear every candidate capture after any change; CP46_REUSE otherwise silently reuses previous results. Always stop after a link failure. The complete checkpoint omits raw state/pixels/PCM captures. Recreate the internal candidate with `python3 native58/link_candidate.py` using the included final objects, then regenerate captures. CP46_REUSE requires a complete set of capture files and JSON; incomplete cached runs are automatically regenerated.

Rebuild offline after extraction:
```bash
tar -xzf SUPPORT/R48_LLVM18_1_3_MINIMAL_REBUILD_TOOLCHAIN.tar.gz -C Release48
python3 Release48/source/build_cp58.py --out {rom.name}
```
For host ARM measurements, build mGBA with source/tools_host/r46_mgba/build_minimal.py and source/tools_host/cp08_mgba/build_probe2.py. Set LLVM PATH/LD_LIBRARY_PATH and MD_ELF to the matching ELF. Included source snapshots, reference ROM/ELF, tests, profiles, compiler archive and final objects permit continuation without earlier chat state. Source delta applies only to CP57; the complete checkpoint is self-contained.

SHA256 was computed once for the final ROM only.
'''
handoff=W/'R48_CP58_HANDOFF_2026-10-09.md';handoff.write_text(text)
files={
 'source/additional/md.c':(A/'md.c',W/'cp57_base/md.c'),
 'source/additional/m68k.c':(A/'m68k.c',W/'cp57_base/m68k.c'),
 'source/additional/mdrender.c':(A/'mdrender.c',W/'cp57_base/mdrender.c'),
 'source/additional/aot/generated/aot_code.c':(A/'aot/generated/aot_code.c',W/'cp57_base/aot_code.c'),
 'source/additional/aot/generated/aot_tabs.c':(A/'aot/generated/aot_tabs.c',W/'cp57_base/aot_tabs.c'),
 'source/build_cp58.py':(R/'source/build_cp58.py',None)}
manifest={'parent':'CP57 4CA93430','files':[{'path':rel,'parent_crc32':f'{zlib.crc32(old.read_bytes()):08X}' if old else None,'new_crc32':f'{zlib.crc32(src.read_bytes()):08X}'} for rel,(src,old) in files.items()]}
apply='''from pathlib import Path
import sys,json,zlib,shutil
D=Path(__file__).resolve().parent;R=Path(sys.argv[1]);m=json.loads((D/'manifest.json').read_text())
for e in m['files']:
 p=R/e['path']
 if p.exists():
  c=f'{zlib.crc32(p.read_bytes()):08X}'
  if c==e['new_crc32']:continue
  if e['parent_crc32'] is None or c!=e['parent_crc32']:raise SystemExit('Parent mismatch: '+e['path'])
 elif e['parent_crc32'] is not None:raise SystemExit('Missing parent: '+e['path'])
for e in m['files']:
 p=R/e['path'];p.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(D/'files'/e['path'],p)
print('Applied CP58; run source/build_cp58.py')
'''
delta=W/'R48_CP58_NATIVE_SOURCE_DELTA_2026-10-09.zip'
with zipfile.ZipFile(delta,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
 for rel,(src,_) in files.items():z.write(src,'CP58/files/'+rel)
 z.writestr('CP58/manifest.json',json.dumps(manifest,indent=2)+'\n');z.writestr('CP58/apply_cp58.py',apply);z.write(handoff,'CP58/'+handoff.name)
 for p in N.rglob('*.json'):
  if p.parent==N or p.parent==N/'gba_results':z.write(p,'CP58/reports/'+str(p.relative_to(N)))
checkpoint=W/'R48_CP58_NATIVE_COMPLETE_CHECKPOINT_2026-10-09.zip'
def excluded(p):return '__pycache__' in p.parts or p.suffix in ('.pyc','.tmp') or '.tmp' in p.name
with zipfile.ZipFile(checkpoint,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
 for folder in (R/'source',R/'additional_roms',R/'build_artifacts',R/'binaries',W/'SUPPORT',W/'cp57_base'):
  for p in sorted(folder.rglob('*')):
   if p.is_file() and not excluded(p):z.write(p,str(p.relative_to(W)))
 for p in sorted(R.glob('*')):
  if p.is_file():z.write(p,str(p.relative_to(W)))
 for p in sorted(N.rglob('*')):
  if not p.is_file() or excluded(p):continue
  include=p.parent==N and p.suffix in ('.py','.json','.log','.c','.inc','.txt','.png')
  include|=p.parent==N/'gba_results' and p.suffix=='.json'
  include|=p.parent==N/'armcheck'
  if include:z.write(p,str(p.relative_to(W)))
 for p in (rom,W/'RUN_THIS_R48_CP57_NATIVE_WINDOW_LOOPS.gba',handoff,delta):z.write(p,p.name)
 z.write(A/'build/md.elf','DEBUG/CP58_md.elf');z.writestr('README_FIRST.md',text)
for p in (delta,checkpoint):
 with zipfile.ZipFile(p) as z:assert z.testzip() is None
(N/'PACKAGE_RESULT.json').write_text(json.dumps({'ROM':str(rom.resolve()),'handoff':str(handoff.resolve()),'delta':str(delta.resolve()),'checkpoint':str(checkpoint.resolve()),'checkpoint_bytes':checkpoint.stat().st_size,'delta_bytes':delta.stat().st_size},indent=2)+'\n')
print(json.dumps(summary,indent=2));print('Checkpoint',checkpoint.stat().st_size,'bytes')
