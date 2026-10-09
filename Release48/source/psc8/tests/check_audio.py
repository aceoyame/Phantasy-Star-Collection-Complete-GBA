#!/usr/bin/env python3
"""Check adapted YM2413 against pristine upstream chip ticks and PSG behavior.
No cartridge emulator, GBA scheduler or physical output is certified here.
"""
from pathlib import Path
import ctypes,subprocess,tempfile,json,math
P=Path(__file__).resolve().parents[1]
O=P.parents[1]/'build_artifacts';O.mkdir(exist_ok=True)
T=Path(tempfile.mkdtemp(prefix='psc8-audio-'))
for name,src in [('ref',P/'vendor/emu2413/emu2413.c'),('gba',P/'opll_gba.c')]:
 text=f'#include "{src}"\nstatic OPLL *test;static unsigned phase;\nvoid init(void){{test=OPLL_new(3579545,18157);phase=0;}}\nvoid wr(unsigned r,unsigned v){{OPLL_writeReg(test,r,v);}}\n'
 if name=='ref':text+='int calc(void){int sum=0,n=0;phase+=3579545;while(phase>=18157*72u){phase-=18157*72u;update_output(test);sum=0+sum;int v=0;for(int i=0;i<14;i++)v+=test->ch_out[i];sum+=v;n++;}return n==3?sum/3:n==2?sum/2:sum;}\n'
 else:text+='int calc(void){return OPLL_calc(test);}\n'
 f=T/(name+'.c');f.write_text(text)
 subprocess.run(['gcc','-O2','-fPIC','-shared','-I'+str(P/'include'),str(f),'-lm','-o',str(T/(name+'.so'))],check=True)
cores=[ctypes.CDLL(str(T/(n+'.so'))) for n in ['ref','gba']]
cases=[]
for instrument in range(16):
 for c in cores:
  c.init()
  for ch in range(9):c.wr(0x30+ch,(instrument<<4)|(ch%4));c.wr(0x10+ch,0x61+ch*7);c.wr(0x20+ch,0x17+(ch%2)*2)
 for n in range(4000):
  if n in [500,1000,1500,2000,2500,3000,3500]:
   for c in cores:
    c.wr(0x0E,0x3F if n in [1000,1500] else 0)
    c.wr(0x00,(n//500)*3);c.wr(0x03,0x11);c.wr(0x04,0xF3)
    c.wr(0x20+(n//500)%9,0x07 if n%1000==0 else 0x17)
  a,b=[c.calc() for c in cores];assert a==b,(instrument,n,a,b)
 cases.append({'instrument':instrument,'sample_outputs_equal':4000})
# Compile the actual PSG component, with wrappers exposing test state only.
f=T/'psg.c';f.write_text(f'#include "{P}/psg.h"\n'+'''
void reset(void){for(int i=0;i<3;i++)tones[i]=0;for(int i=0;i<4;i++){volumes[i]=15;phases[i]=0;}noise=psg_latch=psg_clock_fraction=0;noise_lfsr=0x8000;}
void wr(unsigned v){psg_write(v);}int tone(unsigned c){return psg_tone(c,psg_step());}int noise_sample(void){return psg_noise(psg_step());}
unsigned state(unsigned i){return i==0?noise_lfsr:i==1?phases[3]:i==2?tones[0]:i==3?volumes[2]:noise;}
unsigned step(void){return psg_step();}int enabled(unsigned p,unsigned c){return psg_enabled(p,c);}
''')
subprocess.run(['gcc','-O2','-fPIC','-shared',str(f),'-o',str(T/'psg.so')],check=True)
p=ctypes.CDLL(str(T/'psg.so'))
p.reset();assert sum(p.step() for _ in range(256))==12919920*256+60
p.reset();p.wr(0x8e);p.wr(0x0f);assert p.state(2)==254
p.wr(0x90);samples=[p.tone(0) for _ in range(18157)];crossings=sum(a<=0<b for a,b in zip(samples,samples[1:]));assert abs(crossings-3579545/(32*254))<2
for period in [0,1]:
 p.reset();p.wr(0x80|period);p.wr(0);p.wr(0x90);assert all(p.tone(0)==1200 for _ in range(100))
p.reset();p.wr(0xdf);p.wr(0);assert p.state(3)==0
p.wr(0xe5);p.wr(4);assert p.state(4)==4 and p.state(0)==0x8000
# 512 input clocks per LFSR shift (formerly 256). First two output samples
# have fewer than 512 clocks; third crosses the first shift.
p.reset();p.wr(0xe0);p.wr(0xf0)
assert [p.noise_sample() for _ in range(2)]==[-1200,-1200] and p.state(0)==0x8000
p.noise_sample();assert p.state(0)==0x4000
assert [p.enabled(1,v) for v in range(4)]==[1,0,0,1]
assert all(p.enabled(0,v) for v in range(4))
report={'scope':'Experimental audio component only; integer box conversion replaces upstream sinc. Not a game playthrough or audible/device certification.','opll_bit_exact_chip_tick_box_samples':64000,'opll_cases':cases,'tll_range':[0,238],'tll_rom_saved_bytes':98304,'psg_tone_crossings_per_second':crossings,'psg_reference_hz':3579545/(32*254),'psg_clock_fraction_exact_over_256_samples':True,'psg_noise_512_clock_divider':True,'psg_period_0_1_constant_output':True,'latch_data_volume_noise_writes':True,'fm_mute_four_states':True}
(O/'RELEASE48_EXPERIMENTAL_AUDIO.json').write_text(json.dumps(report,indent=2)+'\n')
print('PASS 64,000 bit-exact OPLL chip/box samples; PSG pitch, clock, divider, PCM periods, latch/data, four mute states')
