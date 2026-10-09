#!/usr/bin/env python3
"""Measure the actual mixer pre-limiter output, with song trim disabled."""
import argparse,json,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts'
ap=argparse.ArgumentParser();ap.add_argument('--frames',type=int,default=3600);ap.add_argument('--label',default='RELEASE05');a=ap.parse_args()
temp=Path(tempfile.mkdtemp(prefix='r46_wave_headroom_'))
source=(R/'gba/audio_p134r/src/chip_audio.cpp').read_text()
marker='mono[sample] = static_cast<int8_t>(v19_fast_limit(mixed));'
assert source.count(marker)==1
source='int measured_prelimit[224];\n'+source.replace(marker,'measured_prelimit[sample]=mixed; '+marker)
(temp/'chip_audio.cpp').write_text(source)
cpp=r'''
#include "chip_audio.cpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
unsigned histogram[65536];
int main(int argc,char**argv) {
 using namespace ps4gba::chip_audio;
 const unsigned frames=std::strtoul(argv[1],nullptr,10);
 for(unsigned id=0x81;id<=0xb4;++id) {
  assert(play(id,224,true));set_mute_mask(0);std::memset(histogram,0,sizeof(histogram));int8_t out[224];unsigned maximum=0;
  for(unsigned frame=0;frame<frames;++frame) {
   std::memset(measured_prelimit,0,sizeof(measured_prelimit));mix_psc_mono(out,224);
   for(int x:measured_prelimit) { unsigned v=unsigned(std::abs(x));assert(v<65536);++histogram[v];if(v>maximum)maximum=v; }
  }
  unsigned cumulative=0,p999=0,threshold=(frames*224*999+999)/1000;
  for(unsigned i=0;i<65536;++i) { cumulative+=histogram[i];if(cumulative>=threshold) {p999=i;break;} }
  std::printf("{\"id\":%u,\"frames\":%u,\"p99_9\":%u,\"max\":%u}\n",id,frames,p999,maximum);std::fflush(stdout);
 }
}
'''
(temp/'check.cpp').write_text(cpp)
flags=['-D'+x+'=1' for x in ['MD2GBA_PS4_MAXMOD_MODE_A','MD2GBA_PS4_MAXMOD_CHANNEL_MAJOR','MD2GBA_PS4_V19_FULL_MIX','MD2GBA_PS4_M4A_NATIVE_ONLY']]
subprocess.run(['g++','-O2','-std=c++17','-DPS4_P134R_SONG_TRIM=0',*flags,'-I'+str(R/'gba/audio_p134r/include'),'-I'+str(R/'gba/audio_p134r/src'),str(temp/'check.cpp'),str(R/'gba/audio_p134r/src/ps4_chip_audio_data.cpp'),str(R/'gba/audio_p134r/src/ps4_fm_multisample_data.cpp'),'-o',str(temp/'check')],check=True)
p=subprocess.run([str(temp/'check'),str(a.frames)],text=True,capture_output=True,check=True)
rows=[json.loads(x) for x in p.stdout.splitlines()];assert len(rows)==52
result={f"{x['id']:02X}":x for x in rows}
report='R46_'+a.label+'_PRELIMIT_HEADROOM.json'
(O/report).write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({'songs':52,'frames_each':a.frames,'p99_9_max':max(x['p99_9'] for x in rows),'report':report}),flush=True)
