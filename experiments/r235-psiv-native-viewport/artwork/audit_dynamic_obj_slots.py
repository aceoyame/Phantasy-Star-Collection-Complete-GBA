#!/usr/bin/env python3
"""Analyze real emulator OAM/OBJ traces; distinguish VRAM addresses from pose IDs.

Input: stage14_overworld_snapshots.json from the private Stage14 QA kit.
This source contains no game ROM/assets. Do not mistake equal OBJ tile indices
for equal animation frames; VRAM is overwritten during runtime.
"""
import argparse, collections, hashlib, json, pathlib, sys

def audit(data):
    records = data.get("data", [])
    if len(records) < 2:
        raise ValueError("Need parent and candidate traces")
    by_rom = {}
    for run in records:
        steps = run["steps"]
        slot_hashes = collections.defaultdict(set)
        evidence = collections.defaultdict(list)
        signatures = []
        for step in steps:
            sprites = step.get("sprites", [])
            signature = (step["step"], step["frame"], step["map_id"],
                         tuple(step["player_xy"]), step["screenshot_crc32"],
                         tuple(sorted((s["slot"], tuple(s["xy"]), tuple(s["dims"]),
                            s["tile_index"], s["palette_bank"], s["raw_sha256"])
                            for s in sprites)))
            signatures.append(signature)
            for sprite in sprites:
                key = (sprite["tile_index"], sprite["palette_bank"])
                slot_hashes[key].add(sprite["raw_sha256"])
                evidence[key].append({"step":step["step"], "frame":step["frame"],
                                      "oam_slot":sprite["slot"],
                                      "sha256":sprite["raw_sha256"]})
        by_rom[run["rom"]] = {"signatures":signatures,
                             "slot_hashes":slot_hashes, "evidence":evidence,
                             "steps":len(steps)}
    keys=list(by_rom)
    parent,candidate=by_rom[keys[0]],by_rom[keys[1]]
    if parent["signatures"] != candidate["signatures"]:
        raise AssertionError("Parent/candidate live field trace diverges")
    mutable=[]
    for key,hashes in sorted(candidate["slot_hashes"].items()):
        if len(hashes)>1:
            mutable.append({"tile_index":key[0], "palette_bank":key[1],
                            "distinct_sprite_hashes":len(hashes),
                            "observations":candidate["evidence"][key]})
    return {"parent":keys[0],"candidate":keys[1],
            "paired_checkpoints":parent["steps"],
            "paired_field_trace_equal":True,
            "mutable_vram_slots":mutable,
            "conclusion":"OBJ tile_index is a VRAM allocation/address, NOT a stable Genesis pose ID.",
            "required_next_capture":"Capture actual OBJ VRAM writes (destination, source ROM address, byte count, frame, actor/pose ID) during movement; link to Genesis source pose only after tracing source."}

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("trace",type=pathlib.Path)
    ap.add_argument("--output",type=pathlib.Path)
    args=ap.parse_args()
    result=audit(json.loads(args.trace.read_text()))
    output=json.dumps(result,indent=2)+"\n"
    if args.output: args.output.write_text(output)
    else: print(output)
if __name__=="__main__":
    main()
