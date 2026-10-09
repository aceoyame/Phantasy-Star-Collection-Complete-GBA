#!/usr/bin/env python3
"""Guard the dedicated PD background-map cache and its retirement boundary."""
from pathlib import Path
import argparse, json, os, subprocess

R = Path(__file__).resolve().parents[1]
O = R.parent / 'build_artifacts'
ap = argparse.ArgumentParser()
ap.add_argument('--label', default='CP29')
a = ap.parse_args()
prefix = (O / 'r45_pd_probe.c').read_text().split('int main(int argc,char**argv)')[0]
stub = 'static void dma_copy32(const void*s,void*d,unsigned n){assert(!(n&3));memcpy(d,s,n);}'
assert stub in prefix
prefix = prefix.replace(stub, 'static unsigned copies;\n' + stub.replace('memcpy(d,s,n);', 'copies++;memcpy(d,s,n);'))
main = r'''
static uint8_t expected_panel[PANEL_ART_SCRATCH_SIZE],expected_ram[65536];
int main(void){
 unsigned cases=0;
 const unsigned ids[]={0,0,1,1,2,0,2,1};
 memset(panel_art_scratch,0xA5,PANEL_ART_SCRATCH_SIZE);
 memset(work_ram,0xA5,sizeof work_ram);
 r45_pd_visual()->controller=1u;
 memcpy(expected_panel,panel_art_scratch,sizeof expected_panel);
 memcpy(expected_ram,work_ram,sizeof expected_ram);
 unsigned previous=99u,expected_copies=0;
 for(unsigned i=0;i<sizeof(ids)/sizeof(ids[0]);i++){
  unsigned id=ids[i];
  const uint16_t*source=(const uint16_t*)(const void*)(r45_pd_art_bank+r45_pd_bg_meta[id][2]);
  const uint16_t*got=r46_pd_background_map(id);
  assert(got==(const uint16_t*)(const void*)(panel_art_scratch+R46_PD_BG_CACHE_OFF));
  memcpy(expected_panel+R46_PD_BG_CACHE_OFF,source,3072u);
  uint32_t tag=0x50444200u|id;
  memcpy(expected_ram+R46_PD_BG_CACHE_TAG,&tag,4u);
  assert(!memcmp(panel_art_scratch,expected_panel,sizeof expected_panel));
  assert(!memcmp(work_ram,expected_ram,sizeof expected_ram));
  expected_copies+=id!=previous;assert(copies==expected_copies);previous=id;cases++;
 }
 r46_pd_cache_retire();
 memset(expected_ram+R46_PD_CACHE_TAG,0,4);
 memset(expected_ram+R46_PD_BG_CACHE_TAG,0,4);
 assert(!memcmp(work_ram,expected_ram,sizeof expected_ram));
 r45_pd_visual()->controller=0u;
 memcpy(expected_panel,panel_art_scratch,sizeof expected_panel);
 for(unsigned id=0;id<3;id++){
  assert(r46_pd_background_map(id)==(const uint16_t*)(const void*)(r45_pd_art_bank+r45_pd_bg_meta[id][2]));
  assert(!memcmp(panel_art_scratch,expected_panel,sizeof expected_panel));
  assert(!memcmp(work_ram,expected_ram,sizeof expected_ram));
  assert(copies==expected_copies);cases++;
 }
 puts("PASS 11 PD map-cache cases: exact source words, cache hits, whole-panel/RAM guards, retirement and inactive ownership");
 return 0;
}
'''
c = O / 'r46_pd_cache_probe.c'
c.write_text(prefix + main)
exe = O / 'r46_pd_cache_probe'
subprocess.run(['gcc', '-std=c11', '-O1', '-g', '-no-pie', '-fsanitize=address,undefined', '-I' + str(R / 'gba'), str(c), '-o', str(exe)], check=True)
p = subprocess.run([str(exe)], text=True, capture_output=True, check=True, env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0'))
result = {'result': 'PASS', 'cases': 11, 'output': p.stdout.strip(), 'scope': __doc__, 'whole_panel_and_work_RAM_guards': True, 'new_globals_or_RAM_allocation': False}
(O / ('R46_' + a.label + '_PD_CACHE_BOUNDS.json')).write_text(json.dumps(result, indent=2) + '\n')
print(p.stdout, end='')
