#!/usr/bin/env python3
"""Classify inherited versus new PCM differences across four identical replay routes.

Analysis only: input is raw interleaved stereo signed-16-bit little-endian PCM.
This does not run an emulator or establish audio fidelity.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path

def load(path):
    raw = path.read_bytes()
    if len(raw) % 4:
        raise ValueError(f"{path}: not whole stereo int16 frames")
    return raw, struct.unpack("<" + "h" * (len(raw) // 2), raw)

def analyze(accepted, parent, candidate, sample_rate, fps):
    if not (len(accepted) == len(parent) == len(candidate)):
        raise ValueError("PCM captures must have identical sample counts and route timing")
    inherited = {i for i, (a, p) in enumerate(zip(accepted, parent)) if a != p}
    novel = {i for i, (a, c) in enumerate(zip(accepted, candidate)) if a != c}
    parent_changed = {i for i, (p, c) in enumerate(zip(parent, candidate)) if p != c}
    def detail(indices):
        if not indices:
            return {"count": 0, "first_sample": None, "first_frame_approx": None}
        first = min(indices)
        return {"count": len(indices), "first_sample": first,
                "first_frame_approx": round(first / (2 * sample_rate) * fps, 5),
                "first_channel": "L" if first % 2 == 0 else "R"}
    return {
        "accepted_vs_parent": detail(inherited),
        "candidate_vs_accepted": detail(novel),
        "candidate_vs_parent": detail(parent_changed),
        "candidate_vs_accepted_at_parent_identical_positions": detail(novel - inherited),
        "candidate_vs_parent_at_accepted_identical_positions": detail(parent_changed - inherited),
        "inherited_mismatch_positions_still_mismatched": len(inherited & novel),
        "candidate_restored_accepted_at_inherited_positions": len(inherited - novel),
        "first_20_novel_indices": sorted(novel - inherited)[:20],
        "interpretation": "PCM differential only; inspect mixer/IRQ state at earliest novel index"
    }

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--accepted", type=Path, required=True)
    ap.add_argument("--parent", type=Path, required=True)
    ap.add_argument("--candidate", type=Path, required=True)
    ap.add_argument("--sample-rate", type=float, required=True,
                    help="Measured output PCM sample rate, not an assumed default")
    ap.add_argument("--fps", type=float, default=59.7275,
                    help="Actual emulated video rate; default nominal GBA")
    args = ap.parse_args()
    if args.sample_rate <= 0 or args.fps <= 0:
        ap.error("sample-rate and fps must be positive")
    captures = {}
    for key in ("accepted", "parent", "candidate"):
        path = getattr(args, key)
        raw, samples = load(path)
        captures[key] = samples
        captures[key + "_sha256"] = hashlib.sha256(raw).hexdigest()
    report = analyze(*(captures[k] for k in ("accepted", "parent", "candidate")),
                     args.sample_rate, args.fps)
    report["capture_sha256"] = {k: captures[k + "_sha256"] for k in ("accepted", "parent", "candidate")}
    report["sample_rate_hz"] = args.sample_rate
    report["test_class"] = "OFFLINE_PCM_ANALYSIS_NOT_EMULATOR_QA"
    print(json.dumps(report, indent=2))

if __name__ == "__main__":
    main()
