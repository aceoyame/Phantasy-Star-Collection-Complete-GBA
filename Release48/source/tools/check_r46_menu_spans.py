#!/usr/bin/env python3
"""Independent scalar MD tile oracle vs production menu snapshot row kernel."""
from pathlib import Path
import re,subprocess,os
R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts';s=(R/'gba/title_runtime_v06.c').read_text()
m=re.search(r'^static void ps4_menu_tile_span\([^\n]+\{',s,re.M);i=m.end();d=1
while d:d+=(s[i]=='{')-(s[i]=='}');i+=1
code=r'''
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
typedef uint16_t ps4_u16_alias __attribute__((may_alias));
static uint8_t md_vram[65536] __attribute__((aligned(4))),field_pause_fb[38400] __attribute__((aligned(4))),ref[38400];
static uint32_t seed=1923;
static unsigned rng(void){seed=seed*1664525u+1013904223u;return seed;}
static unsigned pixel(uint16_t e,unsigned x,unsigned y){if(e&0x800)x=7-x;if(e&0x1000)y=7-y;unsigned z=md_vram[(e&2047)*32+y*4+x/2];return x&1?z&15:z>>4;}
static void oracle(uint16_t a,uint16_t b,int sx,int sy){
 for(unsigned y=0;y<8;y++)for(unsigned x=0;x<8;x++){
  int dx=sx+x,dy=sy+y;if(dx<0||dx>=240||dy<0||dy>=160)continue;
  unsigned ia=pixel(a,x,y),ib=pixel(b,x,y),pa=a>>13&3,pb=b>>13&3,out=0;
  if(ia&&((a&0x8000)||!ib||!(b&0x8000)))out=pa*16+ia;else if(ib)out=pb*16+ib;else if(ia)out=pa*16+ia;
  ref[dy*240+dx]=out;
 }
}
'''+s[m.start():i]+r'''
int main(void){
 for(unsigned i=0;i<65536;i++)md_vram[i]=rng()>>24;
 unsigned cases=0;int xs[]={-7,-1,0,1,2,232,233,239},ys[]={-7,-1,0,1,152,153,159};
 for(unsigned af=0;af<32;af++)for(unsigned bf=0;bf<32;bf++)for(unsigned pos=0;pos<56;pos++){
  uint16_t a=(af<<11)|(rng()&2047),b=(bf<<11)|(rng()&2047);int x=xs[pos%8],y=ys[pos/8];
  memset(field_pause_fb,0xA5,sizeof field_pause_fb);memset(ref,0xA5,sizeof ref);
  oracle(a,b,x,y);ps4_menu_tile_span(a,b,x,y);assert(!memcmp(ref,field_pause_fb,sizeof ref));cases++;
 }
 printf("PASS %u full-frame menu comparisons: all palette/priority/flip combinations and clipped/odd/even rows\n",cases);
}
'''
p=O/'r46_menu_span_probe.c';p.write_text(code);exe=O/'r46_menu_span_probe'
subprocess.run(['gcc','-O2','-g','-fsanitize=address,undefined','-no-pie',str(p),'-o',str(exe)],check=True)
subprocess.run([str(exe)],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
