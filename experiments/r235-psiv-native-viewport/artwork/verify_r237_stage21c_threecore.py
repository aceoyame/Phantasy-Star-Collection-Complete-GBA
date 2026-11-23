#!/usr/bin/env python3
"""Compare Stage21C and R237-rebased experimental PSIV in real GBA cores.

Requires the owner's private GBA cartridges and the PSC headless emulator
toolkit, installed *outside* Git. Never embeds, changes or uploads ROMs.
Opening + early field is a targeted regression, NOT full-game QA.
"""
import argparse
import hashlib
import json
import os
import sys
import zlib
from pathlib import Path

STAGE21C = "ff5f96aae1a536ee13994040ff98bb74a2afbfe1f1a3065df6e996aa1cab5139"
REBASED = "cc3b2abc0f12b3fdaa4df795994d79221ee2537bc576c8cc643276ad99d6957e"
ROM_BYTES = 33554432


def hashfile(path):
    data = path.read_bytes()
    if len(data) != ROM_BYTES:
        raise ValueError(f"Not a 32MiB GBA cartridge: {path}")
    return hashlib.sha256(data).hexdigest()


def run_path(rom, core, toolkit, output, label):
    from ps4_intro_route import start_ps4

    if core == "mgba":
        from mgba2 import GBA
        emulator = GBA(rom)
    else:
        from vbam86 import VBA
        suffix = "delta" if core == "delta" else "libretro"
        os.environ["PSC_VBAM_CORE"] = str(toolkit / "qa/vbam" / suffix / "vbam_libretro.so")
        emulator = VBA(rom)
    start_ps4(emulator)
    emulator.run(220)
    emulator.press("START", 5, 90)
    emulator.press("START", 5, 90)
    emulator.press("A", 5, 150)
    emulator.press("A", 5, 220)
    emulator.run(300)
    observations = []
    output.mkdir(parents=True, exist_ok=True)
    # Authored input replay, without memory patches or skipped game frames.
    for index in range(33):
        if index:
            emulator.press("A", 3, 45)
        image = emulator.shot()
        if index in (0, 10, 20, 24, 28, 30, 32):
            image.save(output / f"{label}_{core}_{index:02d}.png")
        observations.append({
            "checkpoint": index,
            "host_frame": emulator.frame(),
            "framebuffer_crc32": f"{zlib.crc32(image.tobytes()):08x}",
            "map_bytes": emulator.mem(0x02024068, 4).hex(),
            "party_bytes": emulator.mem(0x0200F40A, 5).hex(),
        })
    if hasattr(emulator, "close"):
        emulator.close()
    return observations


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--toolkit", type=Path, required=True)
    parser.add_argument("--stage21c", type=Path, required=True)
    parser.add_argument("--rebased", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    assert hashfile(args.stage21c) == STAGE21C, "Stage21C source input SHA mismatch"
    assert hashfile(args.rebased) == REBASED, "R237-rebased experimental SHA mismatch"
    toolkit = args.toolkit.resolve()
    if not (toolkit / "qa/probe/probe.so").exists():
        raise FileNotFoundError("Missing actual emulator toolkit probe.so")
    os.environ["CP08_PROBE_LIB"] = str(toolkit / "qa/probe/probe.so")
    sys.path[:0] = [str(toolkit / "qa/probe"), str(toolkit / "qa/tests")]
    args.output.mkdir(parents=True, exist_ok=True)
    result = {
        "classification": "REAL_EMULATOR_EARLY_FIELD_TEST_NOT_FULL_PLAYTHROUGH",
        "stage21c_sha256": STAGE21C, "rebased_sha256": REBASED,
        "checkpoint_count": 33, "cores": {}
    }
    for core in ("mgba", "vbam", "delta"):
        a = run_path(args.stage21c, core, toolkit, args.output, "stage21c")
        b = run_path(args.rebased, core, toolkit, args.output, "r237_rebased")
        differences = [
            {"frame": x["host_frame"], "checkpoint": x["checkpoint"],
             "baseline_crc32": x["framebuffer_crc32"],
             "rebased_crc32": y["framebuffer_crc32"]}
            for x, y in zip(a, b) if x["framebuffer_crc32"] != y["framebuffer_crc32"]
        ]
        result["cores"][core] = {
            "image_match": len(a) - len(differences),
            "total": len(a),
            "map_match": sum(x["map_bytes"] == y["map_bytes"] for x, y in zip(a, b)),
            "party_match": sum(x["party_bytes"] == y["party_bytes"] for x, y in zip(a, b)),
            "frame_count_match": sum(x["host_frame"] == y["host_frame"] for x, y in zip(a, b)),
            "image_differences": differences,
        }
    (args.output / "threecore_comparison.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
