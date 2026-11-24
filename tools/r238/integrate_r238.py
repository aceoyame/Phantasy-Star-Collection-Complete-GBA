#!/usr/bin/env python3
"""Exact R237->R238 and fail-closed Stage26 experimental integration.
ROM-free repository: privately supplied checkpoint ZIP contains patch and art.
No input is overwritten; no altered overlapping Stage26 byte is silently replaced.
"""
import argparse, hashlib, json, zipfile
from pathlib import Path

N=32*1024*1024
BASE=0x08000000
R237="a84e9c0501c16a2b750a85037cfa16bcd859101fb4b201ff4edaa1da6b2742de"
R238="20f4fa76fb591b453a9c28b2b650a4dc1259904b083cbe0cc3129fb638a2c558"
STAGE26="3154fb729fdb016574d63dff8dea84e01f1b77e2ac346390530d4f90d932a1a0"

def sha(data):
    return hashlib.sha256(data).hexdigest()

def load(path, expected):
    data=Path(path).read_bytes()
    if len(data)!=N or sha(data)!=expected:
        raise ValueError(f"{path}: expected 32MiB SHA256 {expected}, got {len(data)} bytes {sha(data)}")
    return data

def ips32(source, patch):
    if patch[:5]!=b"IPS32": raise ValueError("wrong IPS32 header")
    at,out,nrec=5,bytearray(source),0
    while True:
        if at+4>len(patch): raise ValueError("truncated patch")
        if patch[at:at+4]==b"EEOF":
            if at+4!=len(patch): raise ValueError("trailing patch data")
            return bytes(out),nrec
        if at+6>len(patch): raise ValueError("short record")
        off=int.from_bytes(patch[at:at+4],"big")
        n=int.from_bytes(patch[at+4:at+6],"big");at+=6
        if n:
            data=patch[at:at+n];at+=n
        else:
            if at+3>len(patch): raise ValueError("short RLE record")
            n=int.from_bytes(patch[at:at+2],"big")
            data=patch[at+2:at+3]*n;at+=3
        if not n or off+n>N or len(data)!=n: raise ValueError(f"invalid IPS32 offset {off:x}")
        out[off:off+n]=data;nrec+=1

def overlay_stage26(r237,r238,stage26):
    if len(r237)!=N or len(r238)!=N or len(stage26)!=N:
        raise ValueError("wrong ROM size")
    out=bytearray(stage26);changes=0;conflicts=[]
    for i,(old,new) in enumerate(zip(r237,r238)):
        if old==new: continue
        if stage26[i]!=old:
            if len(conflicts)<12: conflicts.append(hex(BASE+i))
            continue
        out[i]=new;changes+=1
    if conflicts:
        raise ValueError("R238/Stage26 conflict at "+", ".join(conflicts)+"; refusing overwrite")
    # Stage26's relocated renderer helpers must be unchanged by R238.
    for start,stop in ((0x0924CE40,0x0924E000),(0x08F97F00,0x08F97FD0)):
        lo,hi=start-BASE,stop-BASE
        if out[lo:hi]!=stage26[lo:hi]:
            raise ValueError(f"experimental helper overwritten: {start:08x}")
    return bytes(out),changes

def build(r237_path, checkpoint, out_path, stage26_path=None, report_path=None):
    paths=[Path(r237_path),Path(checkpoint)]
    if stage26_path: paths.append(Path(stage26_path))
    output=Path(out_path)
    if output.exists() or any(output.resolve()==p.resolve() for p in paths):
        raise ValueError("output already exists or aliases an input")
    if report_path and Path(report_path).exists(): raise ValueError("report exists")
    base=load(r237_path,R237)
    with zipfile.ZipFile(checkpoint) as z:
        candidate=z.read("r238/PSC_R238_CANDIDATE.gba")
        patch=z.read("r238/R237_to_R238.ips32")
    if len(candidate)!=N or sha(candidate)!=R238:
        raise ValueError("R238 candidate in checkpoint is not accepted expected hash")
    rebuilt,nrec=ips32(base,patch)
    if rebuilt!=candidate: raise ValueError("IPS32 fails byte-for-byte roundtrip")
    report={"mode":"R238_MAINLINE_EXACT", "r237_sha256":R237,
            "r238_sha256":R238,"patch_records":nrec,"size_bytes":N}
    data=candidate
    if stage26_path:
        data,changes=overlay_stage26(base,candidate,load(stage26_path,STAGE26))
        report.update(mode="R238_STAGE26_EXPERIMENTAL_UNTESTED",
                      stage26_sha256=STAGE26, changed_from_stage26=changes)
    report["output_sha256"]=sha(data)
    report["classification"]=("STATIC_MERGE_ONLY_NEEDS_EMULATOR_QA" if stage26_path
                               else "BIT_IDENTICAL_TO_SUPPLIED_R238")
    output.parent.mkdir(parents=True,exist_ok=True)
    with output.open("xb") as f: f.write(data)
    if report_path:
        p=Path(report_path);p.parent.mkdir(parents=True,exist_ok=True)
        with p.open("x",encoding="utf8") as f: json.dump(report,f,indent=2);f.write("\n")
    return report

if __name__=="__main__":
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--r237",type=Path,required=True)
    p.add_argument("--checkpoint",type=Path,required=True,help="PSC_R238_checkpoint3.zip")
    p.add_argument("--stage26",type=Path,help="sha3154... experimental ROM")
    p.add_argument("--out",type=Path,required=True)
    p.add_argument("--report",type=Path)
    a=p.parse_args()
    print(json.dumps(build(a.r237,a.checkpoint,a.out,a.stage26,a.report),indent=2))
