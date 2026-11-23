#!/usr/bin/env python3
"""Offline temporal-phase triage of R237 Stage21C water-strip differences.

Never treats a shifted match as a valid gameplay match: this is a diagnostic
only. Captures must come from actual emulator replay, with unchanged ROM,
input trace and core identity for each candidate time series.
"""
import argparse
import hashlib
import json
from pathlib import Path
from PIL import Image

W, H = 240, 160

def pixels(path):
    with Image.open(path) as im:
        if im.size != (W, H) or im.mode not in ("RGB", "RGBA"):
            raise ValueError(f"unscaled RGB 240x160 PNG required: {path}")
        return im.convert("RGB").tobytes()

def distance(a, b, roi):
    x0, y0, x1, y1 = roi
    n = 0
    for y in range(y0, y1):
        for x in range(x0, x1):
            k = (y * W + x) * 3
            n += a[k:k+3] != b[k:k+3]
    return n

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--manifest", type=Path, required=True)
    p.add_argument("--reference", required=True, help="capture series identifier")
    p.add_argument("--candidate", required=True, help="capture series identifier")
    p.add_argument("--roi", nargs=4, type=int, default=[178, 0, 210, 160])
    p.add_argument("--max-offset", type=int, default=8)
    p.add_argument("--output", type=Path, required=True)
    a = p.parse_args()
    x0,y0,x1,y1 = a.roi
    if not (0 <= x0 < x1 <= W and 0 <= y0 < y1 <= H):
        p.error("invalid half-open ROI")
    if a.max_offset < 0 or a.max_offset > 240:
        p.error("max-offset must be 0..240")
    raw = a.manifest.read_bytes()
    manifest = json.loads(raw)
    # Required: series, frame, core, rom_sha256, input_sha256, path.
    # input_sha256 must identify the *whole* replay script, not per-frame inputs.
    entries = {}
    for row in manifest["captures"]:
        for key in ("series","frame","core","rom_sha256","input_sha256","path"):
            if key not in row:
                raise ValueError(f"missing {key}")
        k = (row["series"], int(row["frame"]))
        if k in entries:
            raise ValueError(f"duplicate {k}")
        if not isinstance(row["frame"], int) or row["frame"] < 0:
            raise ValueError("frame must be nonnegative integer")
        path = (a.manifest.parent / row["path"]).resolve()
        entries[k] = (row, pixels(path), hashlib.sha256(path.read_bytes()).hexdigest())
    ref = {f:v for (s,f),v in entries.items() if s == a.reference}
    cand = {f:v for (s,f),v in entries.items() if s == a.candidate}
    if not ref or not cand:
        raise ValueError("both capture series required")
    results = []
    for frame in sorted(ref.keys() & cand.keys()):
        rr,rpx,rhash = ref[frame]
        cr,cpx,chash = cand[frame]
        if (rr["core"],rr["rom_sha256"],rr["input_sha256"]) != (cr["core"],cr["rom_sha256"],cr["input_sha256"]):
            results.append({"frame":frame,"status":"PROVENANCE_MISMATCH"})
            continue
        exact = distance(rpx,cpx,a.roi)
        alternatives = []
        for offset in range(-a.max_offset,a.max_offset+1):
            if offset == 0 or frame+offset not in cand:
                continue
            ar,apx,ahash = cand[frame+offset]
            if (ar["core"],ar["rom_sha256"],ar["input_sha256"]) != (rr["core"],rr["rom_sha256"],rr["input_sha256"]):
                continue
            alternatives.append({"offset":offset,"different_pixels":distance(rpx,apx,a.roi),
                                 "capture_sha256":ahash})
        alternatives.sort(key=lambda x:(x["different_pixels"],abs(x["offset"]),x["offset"]))
        results.append({"frame":frame,"status":"OFFLINE_TEMPORAL_DIAGNOSTIC",
                        "exact_frame_different_pixels":exact,"reference_png_sha256":rhash,
                        "candidate_png_sha256":chash,
                        "best_shifted_matches":alternatives[:5],
                        "shift_improves":bool(alternatives and alternatives[0]["different_pixels"] < exact)})
    report = {"test_class":"OFFLINE_DIAGNOSTIC_NOT_EMULATOR_OR_FIDELITY_ACCEPTANCE",
              "manifest_sha256":hashlib.sha256(raw).hexdigest(),
              "reference":a.reference,"candidate":a.candidate,
              "roi":a.roi,"max_offset":a.max_offset,"results":results}
    a.output.write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8")
    print(json.dumps({"matched_frames":len(results),"output":str(a.output)}))

if __name__ == "__main__":
    main()
