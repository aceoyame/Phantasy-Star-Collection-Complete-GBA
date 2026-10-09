#!/usr/bin/env python3
"""R26 source-clock pitch tables and 256-point, DC-free FM instrument waves.

Input dense bank is regenerated from pre-R18 YM anchors first. All expensive
Fourier work happens here; ARM playback still uses one indexed sample lookup.
"""
from pathlib import Path
import json
import re
import numpy as np
from generate_dense_fm_multisamples import parse

ROOT=Path(__file__).resolve().parents[1]
DATA=ROOT/'gba/audio_p134r/src/ps4_chip_audio_data_generated.inc'
WAVES=ROOT/'gba/audio_p134r/src/ps4_fm_multisample_data_generated.inc'
FNUM=(606,644,683,723,766,813,860,911,965,1023,1084,1148)
YM_CLOCK=7670454
PSG_CLOCK=3579545
RATE=13379

def main():
    rom=(ROOT/'references/roms/ps4built.bin').read_bytes()
    fm=[round((FNUM[n%12] << ((n//12)&7))*YM_CLOCK*(1<<11)/(144*RATE)) for n in range(128)]
    periods=[int.from_bytes(rom[0xD0FA2+2*n:0xD0FA4+2*n],'big') for n in range(128)]
    # The source table has 70 ordinary periods and a zero sentinel. Preserve
    # the source 10-bit PSG divisor; undefined note indices are silent.
    psg=[round(PSG_CLOCK*(1<<32)/(32*RATE*p))&0xFFFFFFFF if 0<p<=1023 else 0 for p in periods]
    txt=DATA.read_text()
    for name,vals in (('fm',fm),('psg',psg)):
        body='\n'.join('    '+', '.join(str(x)+'u' for x in vals[i:i+8])+',' for i in range(0,128,8))
        txt=re.sub(r'(const unsigned int generated_'+name+r'_phase_inc\[128\] = \{).*?(\n\};)',lambda m:m[1]+'\n'+body+m[2],txt,flags=re.S)
    txt=txt.replace('// P93 PSC/M4A mixer-rate retarget: phase increments are for 13379u.',
                    '// R26: phase increments derived once from source YM/PSG clocks at 13379 Hz.')
    DATA.write_text(txt)

    sr,pts,blob,zones=parse(WAVES)
    assert pts==128 and len(zones)==3977
    x=np.array(blob,dtype=np.float64)
    bank=[]; cache={}; changed=0
    for z in zones:
        hz=fm[z['note']]*RATE/(1<<32)
        previous=None; offsets=[]; gains=[]
        for off,gain in zip(z['offs'],z['gains']):
            w=x[off:off+128].copy(); w-=w.mean()
            if previous is None:
                # A zero crossing supplies a quieter phase origin on key-on.
                crossings=[i for i in range(128) if w[i]<=0<w[(i+1)%128]]
                origin=min(crossings,key=lambda i:abs(w[i])) if crossings else int(np.argmin(np.abs(w)))
                w=np.roll(w,-origin)
            else:
                # Snapshots are separate reference cycles. Align their phase
                # before crossfading so envelope motion does not become a jump.
                score=np.fft.irfft(np.conj(np.fft.rfft(previous))*np.fft.rfft(w),n=128)
                w=np.roll(w,-int(np.argmax(score)))
            previous=w
            bins=np.fft.rfft(w)
            cutoff=(RATE/2)/hz if hz else 65
            for h in range(1,65):
                if h>=cutoff: bins[h]=0
                elif h>cutoff-1: bins[h]*=(1+np.cos(np.pi*(h-cutoff+1)))/2
            # 128->256 periodic sinc interpolation: keep harmonic levels.
            bins[-1]*=0.5
            expanded=np.fft.irfft(bins,n=256)*2
            peak=float(np.max(np.abs(expanded)))
            if peak>120:
                expanded*=120/peak; gain=round(gain*peak/120)
            q=tuple(np.rint(expanded).clip(-127,127).astype(np.int16).tolist())
            if q not in cache: cache[q]=len(bank);bank.extend(q)
            offsets.append(cache[q]);gains.append(max(1,min(255,gain)))
        z['offs']=offsets;z['gains']=gains
    out=['// R26: original YM anchors, dense semitone zones, phase-aligned 256-point waves.\n',
         f'constexpr unsigned int generated_fm_wave_sample_rate = {sr}u;\n',
         'constexpr unsigned int generated_fm_wave_points = 256u;\n',
         f'constexpr unsigned int generated_fm_wave_blob_size = {len(bank)}u;\n',
         'const signed char generated_fm_wave_blob[] = {\n']
    for i in range(0,len(bank),20):out.append('    '+', '.join(map(str,bank[i:i+20]))+',\n')
    out+=['};\n',f'constexpr unsigned int generated_fm_wave_zone_count = {len(zones)}u;\n','const fm_wave_zone generated_fm_wave_zones[] = {\n']
    for z in zones:
        offsets=', '.join(str(v)+'u' for v in z['offs']); gains=', '.join(map(str,z['gains']))
        out.append(f"    {{ 0x{z['bank']:06X}u, {{ {offsets} }}, {z['inst']}, {z['note']}, {{ {gains} }} }},\n")
    out.append('};\n');WAVES.write_text(''.join(out))
    report={'fm_note36_hz':round(fm[36]*RATE/(1<<32),6),
            'psg_note0_hz':round(psg[0]*RATE/(1<<32),6),
            'wave_points':256,'zones':len(zones),'wave_blob_bytes':len(bank),
            'source':'pre-R18 original YM anchors; intermediate notes remain interpolated',
            'processing':'source-clock pitch, remove DC, align stage phases, periodic sinc interpolation, per-note Nyquist cutoff'}
    # Preserve the source-chip SFX bodies and timing. A very short edge ramp
    # removes the instantaneous PCM cut at stop/retrigger/loop boundaries.
    sfx=ROOT/'gba/pcm_sfx'
    original=sfx/'ps4_sfx_bank.r25.pcm'
    if not original.exists():original.write_bytes((sfx/'ps4_sfx_bank.pcm').read_bytes())
    bank=bytearray(original.read_bytes())
    manifest=json.loads((sfx/'ps4_sfx_runtime_manifest.json').read_text())
    for row in manifest['records']:
        if row['expected_silent']:continue
        start=row['offset'];length=row['length']
        y=np.frombuffer(bank[start:start+length],dtype=np.int8).astype(np.float64)
        head=min(16,length//2);tail=min(48,length//2)
        y[:head]*=np.linspace(0,1,head)
        y[-tail:]*=np.linspace(1,0,tail)
        bank[start:start+length]=np.rint(y).clip(-128,127).astype(np.int8).tobytes()
    (sfx/'ps4_sfx_bank.pcm').write_bytes(bank)
    manifest['edge_processing']='R26: 16-sample attack and 48-sample stop/loop edge ramps; source bodies and lengths retained'
    (sfx/'ps4_sfx_runtime_manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    report['sfx_edge_ramps_samples']={'attack':16,'stop':48}
    (ROOT/'R26_AUDIO_ASSET_REPORT.json').write_text(json.dumps(report,indent=2)+'\n')
    print(report)

if __name__=='__main__':main()
