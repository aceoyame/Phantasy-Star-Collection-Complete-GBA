#!/usr/bin/env python3
"""Inspect queued FIFO words at actual Timer1 publication; test host only."""
import argparse, ctypes, json, sys
from pathlib import Path
R=Path(__file__).resolve().parents[2]
ap=argparse.ArgumentParser();ap.add_argument('--label',required=True);ap.add_argument('--commits',type=int,default=100);ap.add_argument('--expect-continuity',action='store_true');a=ap.parse_args()
sys.path.insert(0,str(R/'source/tools_host/r46_mgba'));from mgba import GBA,L,run_until_break
L.mg_fifo_state.restype=ctypes.c_uint32
s={}
for line in (R/'source/build-psc-module/ps4_psc_module.map').read_text().splitlines():
 p=line.split()
 if len(p)>=5:
  try:s[p[-1]]=int(p[0],16)
  except ValueError:pass
def symbol(part):
 matches=[v for k,v in s.items() if k==part or k.endswith('::'+part)]
 assert len(matches)==1,(part,matches);return matches[0]
g=GBA(R/'binaries/RUN_THIS_RELEASE48_PSC.gba');rows=[]
def snapshot(after=False):
 row=dict(cycles=g.cycles(),timer=g.rd16(0x04000104),fifo_read=L.mg_fifo_state(0),
             fifo_write=L.mg_fifo_state(1),internal_remaining=L.mg_fifo_state(2),
             internal_word=hex(L.mg_fifo_state(3)),next_source=hex(L.mg_fifo_state(4)),
             dma_source=hex(L.mg_fifo_state(5)),fifo_words=[hex(L.mg_fifo_state(i)) for i in range(6,14)],
             ready=g.rd8(symbol('irq_ready')),playing=g.rd8(symbol('irq_play')))
 remaining=row['internal_remaining'];queued=list(int(row['internal_word'],16).to_bytes(4,'little')[:remaining])
 i=row['fifo_read']
 while i!=row['fifo_write']:
  queued.extend(int(row['fifo_words'][i],16).to_bytes(4,'little'));i=(i+1)&7
 offset=(row['timer']-(65536-528))&65535
 slot=symbol('mix_buffer')+1584*(row['playing'] if after else row['playing']^1)
 expected=g.mem(slot+offset,len(queued)) if offset<528 else bytes(queued)
 row.update(offset=offset,pending_bytes=queued,pending_expected=list(expected),
            pending_mismatches=sum(x!=y for x,y in zip(queued,expected)))
 return row
for i in range(a.commits):
 assert run_until_break(g,symbol('audio_irq_commit'),300)
 before=snapshot();ret=g.reg(14)&~1
 assert run_until_break(g,ret,10)
 after=snapshot(True);rows.append(dict(before=before,after=after,cycles=after['cycles']-before['cycles']))
if a.expect_continuity:
 assert rows and all(x['before']['pending_mismatches']==x['after']['pending_mismatches']==0 and x['after']['pending_bytes'] for x in rows),'FIFO discontinuity'
(R/'build_artifacts'/('R46_'+a.label+'_FIFO_SEAMS.json')).write_text(json.dumps(rows,indent=2)+'\n')
print(json.dumps(dict(commits=len(rows),max_cycles=max(x['cycles'] for x in rows),
                    before_bad=sum(bool(x['before']['pending_mismatches']) for x in rows),
                    after_bad=sum(bool(x['after']['pending_mismatches']) for x in rows),
                    after_mismatched_bytes=sum(x['after']['pending_mismatches'] for x in rows)),indent=2))
