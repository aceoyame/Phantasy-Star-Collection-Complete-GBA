#!/usr/bin/env python3
"""Read-only BG scroll and DMA write attribution from an R235 field savestate.
Usage: python trace_field_background.py R235.gba settled.state HEADLESS_TOOLKIT_DIR
"""
import ctypes,json,os,sys
from pathlib import Path
rom,state,toolkit=map(Path,sys.argv[1:4])
sys.path.insert(0,str(toolkit/'qa/probe'))
os.environ['CP08_PROBE_LIB']=str(toolkit/'qa/probe/probe.so')
from mgba2 import GBA,L
L.mg_video_count.restype=ctypes.c_uint64
L.mg_video_rows.restype=ctypes.c_uint32
L.mg_video_row.restype=ctypes.c_uint32
g=GBA(rom);g.load(state);g.run(10)
def snap():
    return dict(map_id=g.rd32(0x02024068)&255,
                player_pixels=[g.rd32(0x020240e4),g.rd32(0x020240e8)],
                dispcnt=hex(g.rd16(0x04000000)),
                layers=[dict(bgcnt=hex(g.rd16(0x04000008+2*i)),
                             hofs=hex(g.rd16(0x04000010+4*i)),
                             vofs=hex(g.rd16(0x04000012+4*i)))
                        for i in range(4)])
before=snap()
L.mg_video_watch(1)
g.run(30,'RIGHT')
after=snap()
counts=[int(L.mg_video_count(i)) for i in range(10)]
L.mg_video_watch(0)
print(json.dumps(dict(before=before,after=after,video_write_counts=counts,
    interpretation='Counts index 6 means CPU stores observed during active DMA, not a list of unique BG pixel changes; this is a read-only probe'),indent=2))
