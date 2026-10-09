#!/usr/bin/env python3
"""Reproduce the ROM-only 4/3 horizontal, 7/5 vertical PD projection plans.

Default verifies the checked-in header; --write regenerates only this header.
"""
from pathlib import Path
import argparse
p=Path(__file__).resolve().parents[1]/'gba/r46_pd_span_plans.h'
ap=argparse.ArgumentParser();ap.add_argument('--write',action='store_true');args=ap.parse_args()
lx=[];runs=[]
for phase in range(8):
 sx=[x*4//3+phase for x in range(240)]
 lx.append([((v&7)^1)*4 for v in sx]);rr=[];x=0
 while x<240:
  end=x+1
  while end<240 and sx[end]//8==sx[x]//8:end+=1
  assert 1<=end-x<=6, 'ARM row suffix decoder requires runs of 1..6 pixels'
  rr.append((x,end-x,sx[x]//8));x=end
 runs.append(rr)
text='''/* Exact floor(x*4/3) tile runs for the eight source scroll phases.
 * ROM-only plans feed the existing IWRAM enemy span kernel. */
struct R46PDRun {uint8_t x,n,tx;uint32_t shifts;};
'''
text+='static const uint8_t r46_pd_ymap[86]={'+','.join(str(y*7//5) for y in range(86))+'};\n'
text+='static const uint8_t r46_pd_lx[8][240]={\n'+',\n'.join('{'+','.join(map(str,row))+'}' for row in lx)+'\n};\n'
text+='static const uint8_t r46_pd_nruns[8]={'+','.join(str(len(row)) for row in runs)+'};\n'
text+='static const struct R46PDRun r46_pd_runs[8][41]={\n'+',\n'.join('{'+','.join('{%d,%d,%d,0x%08Xu}'%(x,n,tx,sum(lx[phase][x+k]<<(k*5) for k in range(n))) for x,n,tx in row)+'}' for phase,row in enumerate(runs))+'\n};\n'
if args.write:p.write_text(text)
else:assert p.read_text()==text,'Projection plans differ; inspect before --write'
print('PASS exact PD projection plan reproduction')
