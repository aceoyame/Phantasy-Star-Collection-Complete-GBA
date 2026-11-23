#!/usr/bin/env python3
"""Build the isolated Stage21C experimental OAM-camera tail-call cartridge.

Source-only, no copyrighted ROM or graphics embedded. Inputs MUST be the
owner's exact SHA-pinned accepted R235 and Stage14 ROM. Never publish the output
on public GitHub or promote it without independent emulator/audio QA.

Original ARM7TDMI tail-call at 0x08F97F90, assembled by LLVM:
    add r0,r0,#16
    add r1,r1,#8
    ldr pc,[pc,#-4]
    .word 0x08C4D13C  ; original native OAM wrapper
The existing caller's BL already sets LR; the tail-call preserves it.
"""
import argparse, hashlib, json, pathlib, zlib

BASE = 0x08000000
ROM_BYTES = 33554432
ACCEPTED_SHA = "4caf23ae8b030c16a32658fdc95085330de6c6dd4a51498f0e2b9c17c7d89308"
STAGE14_SHA = "c1ed9db09505fe2d17322efa9f7c4915e7a4cd57bdf2a3dc6f24d0f8f1d15609"
EXPERIMENT_SHA = "ff5f96aae1a536ee13994040ff98bb74a2afbfe1f1a3065df6e996aa1cab5139"
PATCHES = (
    (0x08C7D4F4, "103fffeb", "a56a0ceb"),  # native field OAM BL => tail-call
    (0x08CD3854, "42110bea", "a999ffea"),  # restore original actor hook
    (0x08F97F90, "e4e1dedad7d3d0d0ccccc9c6c6c2c2c2",
     "100080e2081081e204f01fe53cd1c408"), # retired sound sample42 Tier B
)

def digest(data):
    return hashlib.sha256(data).hexdigest()

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("accepted", type=pathlib.Path)
    ap.add_argument("stage14", type=pathlib.Path)
    ap.add_argument("output", type=pathlib.Path)
    ap.add_argument("--ledger", type=pathlib.Path, required=True)
    args=ap.parse_args()
    accepted=args.accepted.read_bytes()
    parent=args.stage14.read_bytes()
    if len(accepted)!=ROM_BYTES or digest(accepted)!=ACCEPTED_SHA:
        raise ValueError("Refusing incorrect accepted R235 bytes")
    if len(parent)!=ROM_BYTES or digest(parent)!=STAGE14_SHA:
        raise ValueError("Refusing incorrect Stage14 parent bytes")
    out=bytearray(parent)
    ledger=[]
    for bus,oldhex,newhex in PATCHES:
        old,new=bytes.fromhex(oldhex),bytes.fromhex(newhex)
        pos=bus-BASE
        if pos<0 or len(old)!=len(new) or parent[pos:pos+len(old)]!=old:
            raise ValueError("Refusing unexpected parent code at "+hex(bus))
        if bus==0x08F97F90 and accepted[pos:pos+len(old)]!=old:
            raise ValueError("Tier B space is not the original retired sample")
        out[pos:pos+len(new)]=new
        ledger.append({"bus_address":hex(bus),"bytes":len(new),
                       "before_sha256":digest(old),"after_sha256":digest(new)})
    result=bytes(out)
    changed=sum(x!=y for x,y in zip(parent,result))
    if len(result)!=ROM_BYTES or result[:0xC0]!=accepted[:0xC0]:
        raise ValueError("GBA size/header changed")
    if changed!=22 or digest(result)!=EXPERIMENT_SHA:
        raise ValueError("Built different ROM; refusing release")
    details={"status":"EXPERIMENTAL_NOT_ACCEPTED",
             "accepted_sha256":ACCEPTED_SHA,"parent_sha256":STAGE14_SHA,
             "candidate_sha256":digest(result),
             "size_bytes":len(result),"crc32":f"{zlib.crc32(result):08x}",
             "changed_bytes":changed,"regions":ledger,
             "known_blockers":["Academy first-reveal 14-scanline discrepancy",
                               "Audio PCM mismatch on 180-frame field replay",
                               "No two-core Academy route, full FIN or GBA Micro QA"]}
    args.output.write_bytes(result)
    args.ledger.write_text(json.dumps(details,indent=2)+"\n")
    print(json.dumps(details,indent=2))

if __name__=="__main__":
    main()
