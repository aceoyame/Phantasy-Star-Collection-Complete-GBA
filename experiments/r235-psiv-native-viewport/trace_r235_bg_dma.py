#!/usr/bin/env python3
"""Read-only R235 BG HBlank DMA ownership check in live Academy gameplay.

Inputs are deliberately supplied by the operator: no commercial assets are
stored in the test branch.
Usage:
  python trace_r235_bg_dma.py accepted_R235.gba academy.state extracted_toolkit/
"""
import ctypes, hashlib, json, os, struct, sys
from pathlib import Path

ROM_SHA = "4caf23ae8b030c16a32658fdc95085330de6c6dd4a51498f0e2b9c17c7d89308"
# Executed instructions, identified by live ARM breakpoint and register capture.
DMA0_SETUP = (0x08C7D17C, 0x08C7D180, 0x08C7D184)
BG_START = 0x04000010
BG0HOFS = 0x04000010
DMA0SAD = 0x040000B0
DMA0DAD = 0x040000B4
DMA0CNT = 0x040000B8

def sample(rom, state, toolkit):
    if hashlib.sha256(rom.read_bytes()).hexdigest() != ROM_SHA:
        raise SystemExit("Abort: ROM does not match the accepted R235 SHA256")
    probe = toolkit / "qa" / "probe"
    os.environ["CP08_PROBE_LIB"] = str(probe / "probe.so")
    sys.path.insert(0, str(probe))
    from mgba2 import GBA, L

    L.mg_hit.restype = ctypes.c_int
    G = GBA(rom)
    G.load(state)
    start_map = G.rd32(0x02024068) & 0xFF
    if start_map != 18:
        raise SystemExit("Abort: expected Academy map 18, got map %d" % start_map)
    G.run(10)
    checkpoints = []
    for name, frames in (("idle", 1), ("right", 30), ("settled", 10), ("right2", 25)):
        # No memory writes to the game; source and destination are read-only.
        G.run(frames, "RIGHT" if name.startswith("right") else "")
        arrays = {}
        for addr in (0x02034000, 0x02034A10):
            words = struct.unpack("<16H", G.mem(addr, 32))
            arrays[hex(addr)] = {
                "first_pairs": [[words[i], words[i+1]] for i in range(0, 16, 2)],
                "first_and_last_halfwords_at_offsets": {
                    hex(off): list(struct.unpack("<4H", G.mem(addr + off, 8)))
                    for off in (0x10, 0x100, 0x900)
                },
            }
        checkpoints.append({
            "label":name, "frame":G.frame(),
            "player_world_pixels":[G.rd32(0x020240E4), G.rd32(0x020240E8)],
            "bg0hofs":hex(G.rd16(BG0HOFS)),
            "buffers":arrays,
        })
    G.load(state)
    L.mg_break(DMA0_SETUP[0])
    G.run(1, "RIGHT")
    if not L.mg_hit():
        raise SystemExit("DMA0 setup breakpoint not executed")
    if G.reg(0) != BG_START:
        raise SystemExit("unexpected base register")
    chosen = G.reg(2)
    if chosen not in (0x02034010, 0x02034A20):
        raise SystemExit("unexpected DMA source "+hex(chosen))
    observed = {
        "breakpoint":hex(DMA0_SETUP[0]),
        "next_instruction":hex(G.reg(15)),
        "r0_bg_register_base":hex(G.reg(0)),
        "r2_dma0_source":hex(chosen),
        "source_first_halfwords":list(struct.unpack("<8H",G.mem(chosen,16))),
        "dma0_register_targets":[hex(DMA0SAD),hex(DMA0DAD),hex(DMA0CNT)],
        "dma0_expected_destination":hex(BG_START)
    }
    print(json.dumps({
        "status":"READ_ONLY_TRACE_COMPLETE",
        "rom_sha256":ROM_SHA,
        "observations":checkpoints,
        "dma0":observed,
        "limitation":"Output identifies scroll DMA setup and buffers, not the producer code that fills the buffers or a proven safe visual patch."
    },indent=2))
if __name__=="__main__":
    if len(sys.argv)!=4:
        raise SystemExit(__doc__)
    sample(*(Path(x) for x in sys.argv[1:4]))
