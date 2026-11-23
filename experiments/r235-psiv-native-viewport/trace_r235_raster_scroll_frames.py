#!/usr/bin/env python3
"""Capture frame-exact BG0-3 raster-scroll buffers during genuine R235 Academy movement.

Read-only: no ROM/RAM edits, no debugger breakpoint, no game asset publication.
Requires the original private mGBA probe toolkit and a real Academy map-18 state.
The JSON trace can establish when buffer rows change, but cannot identify the
instructions that produced them; follow up with a memory-write breakpoint.
"""
import argparse
import ctypes
import hashlib
import json
import os
import struct
import sys
from pathlib import Path

EXPECTED_SHA = "4caf23ae8b030c16a32658fdc95085330de6c6dd4a51498f0e2b9c17c7d89308"
BASES = (0x02034000, 0x02034A10)
BYTES_PER_BUFFER = 0xA10
ROWS = BYTES_PER_BUFFER // 16  # 161 scanline-sized rows, 8 halfwords each

def digest(path):
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()

def capture(g):
    buffers = []
    for base in BASES:
        raw = g.mem(base, BYTES_PER_BUFFER)
        if len(raw) != BYTES_PER_BUFFER:
            raise RuntimeError("short scroll-buffer read")
        halfwords = struct.unpack("<" + "H" * (BYTES_PER_BUFFER // 2), raw)
        buffers.append({
            "base": hex(base),
            "sha256": hashlib.sha256(raw).hexdigest(),
            "rows": [list(halfwords[i:i+8]) for i in range(0, len(halfwords), 8)]
        })
    return {
        "frame": g.frame(),
        "map_id": g.rd32(0x02024068) & 255,
        "player_xy": [g.rd32(0x020240E4), g.rd32(0x020240E8)],
        "dispcnt": hex(g.rd16(0x04000000)),
        "dma0_source": hex(g.rd32(0x040000B0)),
        "dma0_destination": hex(g.rd32(0x040000B4)),
        "dma0_control": hex(g.rd32(0x040000B8)),
        "buffers": buffers,
    }

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("rom", type=Path)
    parser.add_argument("state", type=Path)
    parser.add_argument("toolkit", type=Path)
    parser.add_argument("--frames", type=int, default=90)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    if args.frames < 1 or args.frames > 600:
        parser.error("frames must be 1..600")
    if args.rom.stat().st_size != 33554432 or digest(args.rom) != EXPECTED_SHA:
        parser.error("ROM size/SHA256 mismatch; refusing non-accepted parent")
    probe = args.toolkit / "qa" / "probe"
    os.environ["CP08_PROBE_LIB"] = str(probe / "probe.so")
    sys.path.insert(0, str(probe))
    from mgba2 import GBA
    g = GBA(args.rom)
    g.load(args.state)
    if g.rd32(0x02024068) & 255 != 18:
        parser.error("state is not Academy map 18")
    frames = [capture(g)]
    for i in range(args.frames):
        g.run(1, "RIGHT" if i < args.frames // 2 else "LEFT")
        frames.append(capture(g))
    transitions = []
    for before, after in zip(frames, frames[1:]):
        changes = []
        for b0, b1 in zip(before["buffers"], after["buffers"]):
            changed = [j for j, (a, b) in enumerate(zip(b0["rows"], b1["rows"])) if a != b]
            changes.append({"base": b0["base"], "changed_rows": changed,
                            "changed_count": len(changed)})
        transitions.append({"frame": after["frame"], "buffer_changes": changes,
                            "player_xy": after["player_xy"],
                            "dma0_source": after["dma0_source"]})
    output = {"status": "READ_ONLY_RASTER_CAPTURE", "rom_sha256": EXPECTED_SHA,
              "frames": frames, "transitions": transitions,
              "caution": "Frame-boundary values are not proof of scanline-time buffer ownership. No alternate viewport implemented."}
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(output, indent=2), encoding="utf-8")
    print(json.dumps({"output": str(args.out), "captured_frames": len(frames),
                      "changed_transitions": sum(any(x["changed_count"] for x in t["buffer_changes"]) for t in transitions)}))

if __name__ == "__main__":
    main()
