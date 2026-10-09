#!/usr/bin/env python3
"""Direct per-note YM captures; allocate 256/512-point zones by measured error.

All chip emulation, Fourier fitting and ROM allocation is offline. The native
player keeps six snapshots and performs the same per-voice sample lookup.
"""
import argparse,json,multiprocessing,re,zlib
from pathlib import Path
from collections import defaultdict,Counter
import numpy as np
from generate_dense_fm_multisamples import parse
from reference_fm_capture import render,source_hz

R=Path(__file__).resolve().parents[1]
TIMES=(.010,.045,.100,.200,.400,.800)
RATE=192000;MIX_RATE=13379;SMALL=0x40000000;DOUBLE=0x80000000
def capture(task):
 bank,inst,note,patch=task
 # MULT=0 is a half-frequency operator. A two-period loop preserves its
 # subharmonics instead of forcing them into a one-period wavetable.
 periods=2 if any((v&15)==0 for v in patch[1:5]) else 1
 hz=source_hz(note);period=periods/hz
 pcm=render(patch,note,RATE,max(1.1,.8+period))
 waves=[];previous=None
 for time in TIMES:
  start=max(0,time-period/2)
  positions=(start+np.arange(2048)*period/2048)*RATE
  raw=np.interp(positions,np.arange(len(pcm)),pcm)
  bins=np.fft.rfft(raw);bins[0]=0
  # Preserve source amplitudes below the physical mixer Nyquist; do not
  # fabricate high harmonics by enlarging a quantized 128-point anchor.
  cutoff=(MIX_RATE/2)/(hz/periods)
  harmonics=np.arange(len(bins));factors=np.ones(len(bins))
  factors[harmonics>=cutoff]=0
  taper=(harmonics>cutoff-1)&(harmonics<cutoff)
  factors[taper]=(1+np.cos(np.pi*(harmonics[taper]-cutoff+1)))/2
  bins*=factors;bins[256:]=0
  wave=np.fft.irfft(bins[:257],n=512)/4
  if previous is None:
   crossings=np.flatnonzero((wave<=0)&(np.roll(wave,-1)>0))
   origin=min(crossings,key=lambda i:abs(wave[i])) if len(crossings) else int(np.argmin(abs(wave)))
   wave=np.roll(wave,-origin)
  else:
   score=np.fft.irfft(np.conj(np.fft.rfft(previous))*np.fft.rfft(wave),n=512)
   wave=np.roll(wave,-int(np.argmax(score)))
  previous=wave;waves.append(wave.astype(np.float32))
 return np.array(waves),periods
def quantize(waves,scale,points):
 values=[];gains=[];errors=[]
 for wave in waves:
  target=wave.astype(np.float64)*scale
  bins=np.fft.rfft(target)
  if points==256:bins[128:]=0
  fitted=np.fft.irfft(bins[:points//2+1],n=points)*(points/512)
  peak=float(max(abs(fitted)))
  gain=min(255,max(0,round(peak*64/112)))
  if gain==0:q=np.zeros(points,dtype=np.int8)
  else:q=np.clip(np.rint(fitted*64/gain),-127,127).astype(np.int8)
  # Actual nearest-index GBA lookup, at 4096 phases, not an ideal interpolator.
  dense=np.fft.irfft(np.fft.rfft(target),n=4096)*8
  actual=np.repeat(q.astype(float)*gain/64,4096//points)
  errors.append(float(np.mean((actual-dense)**2)))
  values.append(q.tobytes());gains.append(gain)
 return values,gains,errors
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--out-dir',type=Path,required=True);ap.add_argument('--workers',type=int,default=4);ap.add_argument('--budget',type=int,default=9103816);ap.add_argument('--capture-cache',type=Path);ap.add_argument('--calibration',choices=['shared_reference','release04_per_patch'],default='shared_reference');a=ap.parse_args();a.out_dir.mkdir(parents=True,exist_ok=True)
 old_path=R.parent/'rollback/R46_RELEASE03_AUDIO/source/gba/audio_p134r/src/ps4_fm_multisample_data_generated.inc'
 sr,points,old,zones=parse(old_path);assert points==512 and len(zones)==3977
 rom=(R/'references/roms/ps4built.bin').read_bytes();tasks=[]
 for z in zones:
  address=z['bank']+z['inst']*25;patch=rom[address:address+25]
  tasks.append((z['bank'],z['inst'],z['note'],patch))
 cache_path=a.capture_cache or a.out_dir/'direct_capture_512.npz'
 if cache_path.exists():
  cache=np.load(cache_path);captures=cache['waves'];periods=cache['periods']
 else:
  captures=[];periods=[]
  with multiprocessing.Pool(a.workers) as pool:
   for i,(waves,nperiods) in enumerate(pool.imap(capture,tasks,chunksize=4)):
    captures.append(waves);periods.append(nperiods)
    if (i+1)%128==0:print('Captured',i+1,'/',len(zones),flush=True)
  captures=np.array(captures);periods=np.array(periods,dtype=np.uint8)
  np.savez_compressed(cache_path,waves=captures,periods=periods)
 original=parse(R/'gba/audio_p134r/src/ps4_fm_multisample_data_original.inc')[3]
 anchors={(z['bank'],z['inst'],z['note']) for z in original};ratios=defaultdict(list)
 for z,waves in zip(zones,captures):
  if (z['bank'],z['inst'],z['note']) not in anchors:continue
  for off,gain,wave in zip(z['offs'],z['gains'],waves):
   old_rms=np.std(np.array(old[off:off+512],dtype=float))*gain/64
   new_rms=np.std(wave)
   if old_rms>1 and new_rms>10:ratios[(z['bank'],z['inst'])].append(old_rms/new_rms)
 global_scale=float(np.median([v for group in ratios.values() for v in group]))
 legacy_scales={key:float(np.median(values)) for key,values in ratios.items()}
 # The old interpolated anchors do not preserve cross-patch chip amplitudes.
 # In the opening, instrument 7's calibration is 8.48x lead instrument 1.
 # One shared linear chip-to-PCM scale preserves all relative voice levels.
 scales=legacy_scales if a.calibration=='release04_per_patch' else {}
 candidates=[];quality=[]
 for z,wave,period in zip(zones,captures,periods):
  scale=scales.get((z['bank'],z['inst']),global_scale)
  small=quantize(wave,scale,256);large=quantize(wave,scale,512)
  candidates.append((small,large))
  # More ROM goes where an actual lookup error reduction is greatest.
  quality.append(sum(small[2])-sum(large[2]))
 ranked=sorted(range(len(zones)),key=lambda i:quality[i],reverse=True)
 def pack(upgrades):
  large_set=set(ranked[:upgrades]);bank=bytearray();dedupe={};records=[]
  for i,(z,period,choices) in enumerate(zip(zones,periods,candidates)):
   large=i in large_set;waves,gains,errors=choices[1 if large else 0]
   flags=(0 if large else SMALL)|(DOUBLE if period==2 else 0);offsets=[]
   for wave in waves:
    if wave not in dedupe:dedupe[wave]=len(bank);bank.extend(wave)
    offsets.append(dedupe[wave]|flags)
   records.append({**z,'offs':offsets,'gains':gains,'points':512 if large else 256,'periods':int(period),'errors':errors})
  return bank,records
 lo=0;hi=len(zones)
 while lo<hi:
  mid=(lo+hi+1)//2
  if len(pack(mid)[0])<=a.budget:lo=mid
  else:hi=mid-1
 bank,records=pack(lo);assert len(bank)<=a.budget
 output=['// R46 direct original YM patches at every dense semitone; calibration '+a.calibration+'.\n',f'constexpr unsigned int generated_fm_wave_sample_rate = {RATE}u;\n','constexpr unsigned int generated_fm_wave_points = 512u; // per-zone 256/512 flags\n',f'constexpr unsigned int generated_fm_wave_blob_size = {len(bank)}u;\n','const signed char generated_fm_wave_blob[] = {\n']
 signed=np.frombuffer(bank,dtype=np.int8)
 for i in range(0,len(signed),20):output.append('    '+', '.join(map(str,signed[i:i+20]))+',\n')
 output+=['};\n',f'constexpr unsigned int generated_fm_wave_zone_count = {len(zones)}u;\n','const fm_wave_zone generated_fm_wave_zones[] = {\n']
 for z in records:
  output.append(f"    {{ 0x{z['bank']:06X}u, {{ "+', '.join(str(x)+'u' for x in z['offs'])+f" }}, {z['inst']}, {z['note']}, {{ "+', '.join(map(str,z['gains']))+' } },\n')
 output.append('};\n');(a.out_dir/'ps4_fm_multisample_data_generated.inc').write_text(''.join(output))
 old_errors=[];new_errors=[];cos_old=[];cos_new=[]
 for z,waves,record in zip(zones,captures,records):
  scale=scales.get((z['bank'],z['inst']),global_scale)
  for stage,wave in enumerate(waves):
   target=wave.astype(float)*scale
   old_wave=np.array(old[z['offs'][stage]:z['offs'][stage]+512],dtype=float)*z['gains'][stage]/64
   if record['periods']==2:old_wave=np.tile(old_wave[::2],2)
   off=record['offs'][stage]&0x3fffffff;pts=record['points'];g=record['gains'][stage]
   q=np.frombuffer(bank[off:off+pts],dtype=np.int8).astype(float)*g/64
   new_wave=np.fft.irfft(np.fft.rfft(q),n=512)*(512/pts)
   # Harmonic magnitudes ignore the arbitrary but inaudible phase origin.
   ref=abs(np.fft.rfft(target));before=abs(np.fft.rfft(old_wave));after=abs(np.fft.rfft(new_wave))
   if np.linalg.norm(ref)>10:
    denom=float(np.linalg.norm(ref));old_errors.append(float(np.linalg.norm(before-ref)/denom));new_errors.append(float(np.linalg.norm(after-ref)/denom))
    cos_old.append(float(np.dot(before,ref)/(np.linalg.norm(before)*denom)) if np.linalg.norm(before) else 0)
    cos_new.append(float(np.dot(after,ref)/(np.linalg.norm(after)*denom)) if np.linalg.norm(after) else 0)
 report={'source':'original PSIV 25-byte YM patches, source FNUM/block, direct libgme VGM renders; no octave harmonic interpolation','reference_library':'installed libgme 0.6.3','capture_rate':RATE,'native_music_mixer_rate':MIX_RATE,'transport_output_nominal_rate':31536,'snapshots_ms':[10,45,100,200,400,800],'zones':len(zones),'instrument_groups':len({(z['bank'],z['inst']) for z in zones}),'zones_by_points':dict(Counter(x['points'] for x in records)),'two_period_zones':sum(x['periods']==2 for x in records),'wave_blob_bytes':len(bank),'old_wave_blob_bytes':len(old),'allocation_budget_bytes':a.budget,'amplitude_calibration':'per-patch median RMS against the 511 accepted original anchors; retain broad approved loudness while replacing per-note shapes and stage levels','calibration_groups':len(scales),'global_scale_fallback':global_scale,'max_gain_q6':max(g for z in records for g in z['gains']),'quality_windows':len(new_errors),'old_relative_spectral_error_median':float(np.median(old_errors)),'new_relative_spectral_error_median':float(np.median(new_errors)),'old_spectral_cosine_median':float(np.median(cos_old)),'new_spectral_cosine_median':float(np.median(cos_new)),'wave_blob_crc32':f'{zlib.crc32(bank):08x}','limitations':'single periodic cycles and six snapshots remain approximations; nearest lookup and 13379-Hz Nyquist; source chip clock/register intent, not physical-device or listening certification'}
 report.update(calibration_mode=a.calibration,amplitude_calibration='one shared linear chip-to-PCM scale across all patches/notes/stages' if a.calibration=='shared_reference' else 'Release 04 legacy per-patch calibration',calibration_groups=len(legacy_scales),shared_reference_scale=global_scale,opening_legacy_patch_scales={str(i):legacy_scales.get((0xdc23d,i)) for i in [0,1,3,5,7]},capture_cache=str(cache_path.resolve().relative_to(R.parent)))
 (a.out_dir/'REFERENCE_WAVE_REPORT.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2),flush=True)
if __name__=='__main__':main()
