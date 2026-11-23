#!/usr/bin/env python3
"""Pixel-exact, core-aware frame comparison for PSIV native-viewport QA.

Inputs are independently captured, unscaled 240x160 RGB/RGBA PNGs. This is
offline analysis only, never proof of emulator execution or Genesis parity.
Never compare CRCs between different emulator cores as an acceptance gate.
"""
import argparse
import hashlib
import json
from pathlib import Path
from PIL import Image

W, H = 240, 160

def read_frame(path):
    raw = path.read_bytes()
    with Image.open(path) as im:
        if im.size != (W, H):
            raise ValueError(f"{path}: expected {W}x{H}, got {im.size}")
        if im.mode not in ("RGB", "RGBA"):
            raise ValueError(f"{path}: indexed/grayscale frame needs explicit RGB export")
        return list(im.convert("RGB").getdata()), hashlib.sha256(raw).hexdigest()

def compare(a, b, roi):
    x0, y0, x1, y1 = roi
    changed = []
    for y in range(y0, y1):
        for x in range(x0, x1):
            p = y * W + x
            if a[p] != b[p]:
                changed.append((x, y, max(abs(a[p][i] - b[p][i]) for i in range(3))))
    if not changed:
        return {"different_pixels": 0, "bbox": None, "max_channel_delta": 0,
                "different_rows": [], "different_columns": []}
    xs = [p[0] for p in changed]
    ys = [p[1] for p in changed]
    return {"different_pixels": len(changed),
            "bbox": [min(xs), min(ys), max(xs)+1, max(ys)+1],
            "max_channel_delta": max(p[2] for p in changed),
            "different_rows": sorted(set(ys)),
            "different_columns": sorted(set(xs))}

def main():
    p = argparse.ArgumentParser()
    p.add_argument("--manifest", type=Path, required=True,
                   help="JSON with captures: [{frame, core, path, rom_sha256, input_sha256}]")
    p.add_argument("--reference-core", required=True)
    p.add_argument("--roi", nargs=4, type=int, default=[0, 0, W, H],
                   metavar=("X0", "Y0", "X1", "Y1"))
    p.add_argument("--output", type=Path, required=True)
    a = p.parse_args()
    x0, y0, x1, y1 = a.roi
    if not (0 <= x0 < x1 <= W and 0 <= y0 < y1 <= H):
        p.error("ROI must be a nonempty half-open rectangle within 240x160")
    manifest = json.loads(a.manifest.read_text())
    rows = manifest["captures"]
    grouped = {}
    for row in rows:
        for key in ("frame", "core", "path", "rom_sha256", "input_sha256"):
            if key not in row:
                raise ValueError(f"missing {key} in capture entry")
        key = (row["frame"], row["core"])
        if key in grouped:
            raise ValueError(f"duplicate frame/core: {key}")
        path = (a.manifest.parent / row["path"]).resolve()
        pixels, sha = read_frame(path)
        grouped[key] = (row, pixels, sha)
    output = []
    for (frame, core), (row, pixels, sha) in sorted(grouped.items()):
        if core == a.reference_core:
            continue
        ref = grouped.get((frame, a.reference_core))
        if ref is None:
            output.append({"frame": frame, "core": core, "status": "NO_MATCHED_REFERENCE"})
            continue
        rrow, rpixels, rsha = ref
        if row["rom_sha256"] != rrow["rom_sha256"] or row["input_sha256"] != rrow["input_sha256"]:
            output.append({"frame": frame, "core": core, "status": "PROVENANCE_MISMATCH",
                           "capture_sha256": sha, "reference_capture_sha256": rsha})
            continue
        output.append({"frame": frame, "core": core, "status": "OFFLINE_PIXEL_COMPARISON",
                       "capture_sha256": sha, "reference_capture_sha256": rsha,
                       "comparison": compare(rpixels, pixels, a.roi)})
    result = {"test_class": "OFFLINE_PNG_ANALYSIS_NOT_EMULATOR_QA",
              "manifest_sha256": hashlib.sha256(a.manifest.read_bytes()).hexdigest(),
              "reference_core": a.reference_core, "roi": a.roi, "results": output}
    a.output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({"output": str(a.output), "comparisons": len(output)}))

if __name__ == "__main__":
    main()
