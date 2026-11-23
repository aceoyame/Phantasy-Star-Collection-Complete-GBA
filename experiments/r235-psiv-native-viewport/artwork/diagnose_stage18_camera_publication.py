#!/usr/bin/env python3
"""Stage18: read-only Academy breakpoint chronology + optional RAM-only OAM oracle.

Private inputs required: exact accepted R235, exact Stage14, Academy state,
headless mGBA probe. NEVER commits a game ROM/asset/savestate to GitHub.

This diagnostic's --probe-ram-oam option is a HOST-ONLY temporary emulator
memory edit, not a cartridge patch. It is deliberately restricted to one
SHA-pinned Academy scene, exact frame and three OAM records. No production
source/ROM bytes are changed. Results cannot constitute release acceptance.
"""
from __future__ import annotations
import argparse, ctypes, hashlib, json, os, sys, zlib
from pathlib import Path

ROM_SHA = {
    "accepted": "4caf23ae8b030c16a32658fdc95085330de6c6dd4a51498f0e2b9c17c7d89308",
    "stage14": "c1ed9db09505fe2d17322efa9f7c4915e7a4cd57bdf2a3dc6f24d0f8f1d15609",
}
STATE_SHA = "3a18bcce1f725baaeac2114f20edda5aca4aa4e482464aba6f15ee21b0454ae3"
ROUTE = (("idle",25),("right",20),("idle",16),("left",18),
         ("up",12),("down",12),("idle",55))
FIRST_FRAME = 16446
BG_DMA_SETUP = 0x08C7D17C
ACTOR_HOOK = 0x08CD3854

def verify(path: str, expect: str, size: int | None = None) -> Path:
    p = Path(path)
    if not p.is_file():
        raise FileNotFoundError(p)
    if size is not None and p.stat().st_size != size:
        raise ValueError("Unexpected input size " + str(p))
    actual = hashlib.sha256(p.read_bytes()).hexdigest()
    if actual != expect:
        raise ValueError("SHA mismatch "+str(p)+" expected "+expect+" got "+actual)
    return p

def startup(rom: Path, state: Path, GBA):
    g = GBA(rom)
    g.load(state)
    if (g.rd32(0x02024068) & 255) != 18 or g.frame() != FIRST_FRAME-1:
        raise ValueError("Expected the original Academy map18 settled state")
    return g

def replay_to(g, frame: int):
    for key, n in ROUTE:
        for _ in range(n):
            if g.frame() >= frame:
                return
            g.run(1, "" if key == "idle" else key.upper())
    if g.frame() != frame:
        raise AssertionError("Target frame not reached")

def breakpoint_first(rom, state, GBA, L, point: int):
    g = startup(rom, state, GBA)
    replay_to(g, 16569)
    L.mg_break(point)
    try:
        g.run(7)
        if not L.mg_hit():
            return {"breakpoint":hex(point),"hit":False,"last_frame":g.frame()}
        return {"breakpoint":hex(point), "hit":True, "frame":g.frame(),
                "map_id":g.rd32(0x02024068)&255,
                "r0_r1_r2_r3":[g.reg(i) for i in range(4)],
                "pc":g.reg(15)}
    finally:
        L.mg_break(0xffffffff)

def capture(rom, state, GBA, L, poke: bool):
    import numpy as np
    g = startup(rom, state, GBA)
    images = {}
    events = []
    for key, n in ROUTE:
        for _ in range(n):
            g.run(1, "" if key == "idle" else key.upper())
            f = g.frame()
            if f == 16571 and poke:
                if (g.rd32(0x02024068)&255) != 17:
                    raise AssertionError("Unexpected map; refusing OAM oracle")
                if (g.rd32(0x020240e4),g.rd32(0x020240e8)) != (328,24):
                    raise AssertionError("Unexpected world position; refusing OAM oracle")
                for slot, tile in ((0,496),(2,480),(40,448)):
                    addr = 0x07000000+8*slot
                    a0, a1, a2 = [g.rd16(addr+i) for i in (0,2,4)]
                    if (a0>>14, a1>>14, a2&1023) != (2,2,tile):
                        raise AssertionError("Unexpected OAM shape/index; refusing oracle")
                    new_a0 = (a0&0xff00) | ((a0-8)&255)
                    new_a1 = (a1&0xfe00) | ((a1-16)&511)
                    g.wr16(addr,new_a0)
                    g.wr16(addr+2,new_a1)
                    if g.rd16(addr)!=new_a0 or g.rd16(addr+2)!=new_a1:
                        raise AssertionError("OAM oracle write not observable")
                    events.append({"frame":f,"slot":slot,"tile":tile,
                                   "before":[a0,a1],"after":[new_a0,new_a1]})
            if 16570 <= f <= 16578:
                data = ctypes.string_at(L.mg_pixels(),240*160*4)
                images[f] = {
                    "rgba":np.frombuffer(data,dtype=np.uint8).reshape(160,240,4).copy(),
                    "frame_crc32":f"{zlib.crc32(data):08x}",
                    "map_id":g.rd32(0x02024068)&255,
                    "xy":[g.rd32(0x020240e4),g.rd32(0x020240e8)]}
            if f >= 16578:
                return images, events
    raise AssertionError("Not enough frames")

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--accepted",required=True)
    ap.add_argument("--stage14",required=True)
    ap.add_argument("--state",required=True)
    ap.add_argument("--toolkit",required=True)
    ap.add_argument("--output",required=True)
    ap.add_argument("--probe-ram-oam",action="store_true")
    args=ap.parse_args()
    accepted=verify(args.accepted,ROM_SHA["accepted"],33554432)
    stage14=verify(args.stage14,ROM_SHA["stage14"],33554432)
    state=verify(args.state,STATE_SHA,397312)
    probe=Path(args.toolkit)/"qa"/"probe"
    if not (probe/"mgba2.py").is_file() or not (probe/"probe.so").is_file():
        raise FileNotFoundError("Authentic headless mGBA probe missing")
    os.environ["CP08_PROBE_LIB"]=str((probe/"probe.so").resolve())
    sys.path.insert(0,str(probe.resolve()))
    from mgba2 import GBA,L
    L.mg_hit.restype=ctypes.c_int
    L.mg_break.argtypes=[ctypes.c_uint32]
    events={"accepted":{}, "stage14":{}}
    for label,rom in (("accepted",accepted),("stage14",stage14)):
        for address,tag in ((BG_DMA_SETUP,"bg_dma_setup"),(ACTOR_HOOK,"actor_hook")):
            events[label][tag]=breakpoint_first(rom,state,GBA,L,address)
    a,_=capture(accepted,state,GBA,L,False)
    b,_=capture(stage14,state,GBA,L,False)
    if args.probe_ram_oam:
        c,pokes=capture(stage14,state,GBA,L,True)
    else:
        c,pokes=None,[]
    comparison=[]
    import numpy as np
    for f in sorted(a):
        xa,xb=a[f],b[f]
        if xa["xy"]!=xb["xy"] or xa["map_id"]!=xb["map_id"]:
            raise AssertionError("Cartridge progression divergence")
        def score(variant):
            # Compare RGB only: hardware frame buffer alpha bits are not artwork.
            diff=np.any(xa["rgba"][8:,16:,:3]!=variant["rgba"][:152,:224,:3],axis=2)
            return int(np.count_nonzero(diff))
        row={"frame":f,"map_id":xa["map_id"],"world_xy":xa["xy"],
             "stage14_shifted_crop_rgb_mismatch_pixels":score(xb)}
        if c is not None:
            row["ram_only_oam_shift_rgb_mismatch_pixels"]=score(c[f])
        comparison.append(row)
    result={"status":"LIVE_MGBA_DIAGNOSTIC_NOT_CARTRIDGE_PATCH",
            "rom_sha256":ROM_SHA,"state_sha256":STATE_SHA,
            "breakpoint_first_hits":events,"route":ROUTE,
            "ram_only_oracle_enabled":args.probe_ram_oam,
            "oam_ram_pokes":pokes,"frame_comparison":comparison,
            "limitations":["This traces first observed breakpoints, not an instruction-complete call tree.",
                           "OAM memory edit is temporary host-only diagnostic, NOT a candidate ROM fix.",
                           "No VBA-M/Delta, dialogue, Genesis pose source or real-hardware acceptance."]}
    Path(args.output).write_text(json.dumps(result,indent=2)+"\n")
    print(json.dumps({"status":result["status"],"breakpoints":events,
        "frame_comparison":comparison},indent=2))

if __name__=="__main__":
    main()
