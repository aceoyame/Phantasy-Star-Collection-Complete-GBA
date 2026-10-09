#!/usr/bin/env python3
"""Compare production border copies to the CP11 renderer for every flip/clip.

The baseline is extracted unchanged from the saved full checkpoint; full-frame
comparison catches writes outside a stamp as well as wrong projected pixels.
"""
from pathlib import Path
import argparse, os, re, subprocess, zipfile
R = Path(__file__).resolve().parents[1]
O = R.parent / 'build_artifacts'
ap = argparse.ArgumentParser()
ap.add_argument('--baseline-zip', type=Path, required=True)
a = ap.parse_args()
production = (R / 'gba/title_runtime_v06.c').read_text()
with zipfile.ZipFile(a.baseline_zip) as z:
    baseline = z.read('R46/source/gba/title_runtime_v06.c').decode()
def fn(source, name):
    m = re.search(r'^static[^\n]*\b' + name + r'\([^;]*?\)\s*\{', source, re.M)
    assert m, name
    p, depth = m.end(), 1
    while depth:
        depth += (source[p] == '{') - (source[p] == '}')
        p += 1
    return source[m.start():p]
header = (R / 'gba/precomp_assets.h').read_text()
offset = int(re.search(r'#define PS4_PRECOMP_BATTLE_WINDOW_TILE6_PIXELS_OFF\s+(\d+)', header)[1])
pixels = (R / 'gba/precomp_assets.bin').read_bytes()[offset:offset + 144]
assert len(pixels) == 144 and max(pixels) < 16
code = '''
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
typedef uint16_t battle_u16_alias __attribute__((may_alias));
#define PS4_PRECOMP_BATTLE_WINDOW_TILE6_COUNT 4u
#define PS4_PRECOMP_BATTLE_WINDOW_TILE6_PIXELS_OFF 0u
#define PS4_WINDOW_PAL_BASE 96u
static uint8_t framebuffer[38400] __attribute__((aligned(4))), reference[38400];
static uint8_t* field_pause_fb=framebuffer;
static const uint8_t ps4_precomp_assets[144]={''' + ','.join(map(str, pixels)) + '};\n'
code += fn(production, 'battle_scale6_index') + '\n'
code += fn(baseline, 'battle_window_stamp_scaled').replace('battle_window_stamp_scaled', 'reference_stamp') + '\n'
code += fn(production, 'battle_window_stamp_scaled') + '\n'
code += '''
int main(void){
 unsigned cases=0;
 const unsigned xs[]={0,1,6,233,234,237,239,240},ys[]={0,1,125,154,159,160};
 for(unsigned tile=0;tile<5;tile++)for(unsigned hf=0;hf<2;hf++)for(unsigned vf=0;vf<2;vf++)
 for(unsigned xi=0;xi<8;xi++)for(unsigned yi=0;yi<6;yi++)for(unsigned w=0;w<=9;w++)for(unsigned h=0;h<=9;h++){
  memset(framebuffer,0xA5,sizeof framebuffer);memset(reference,0xA5,sizeof reference);
  field_pause_fb=reference;reference_stamp(tile,xs[xi],ys[yi],xs[xi]+w,ys[yi]+h,hf,vf);
  field_pause_fb=framebuffer;battle_window_stamp_scaled(tile,xs[xi],ys[yi],xs[xi]+w,ys[yi]+h,hf,vf);
  assert(!memcmp(framebuffer,reference,sizeof framebuffer));cases++;
 }
 printf("PASS %u complete-frame CP11 border comparisons: all tiles/flips, normal and odd/clipped/narrow/empty stamps\\n",cases);
}
'''
c = O / 'r46_window_pairs_probe.c'
c.write_text(code)
exe = O / 'r46_window_pairs_probe'
subprocess.run(['gcc', '-O2', '-g', '-fsanitize=address,undefined', str(c), '-o', str(exe)], check=True)
subprocess.run([str(exe)], check=True, env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0'))
