#!/usr/bin/env python3
"""Analyze deterministic stereo PCM mismatch without copyrighted sample data.

Usage: python stage22_pcm_timing_diagnostic.py baseline.pcm candidate.pcm
The expected input is raw interleaved signed little-endian 16-bit stereo PCM.
This tool never writes sample data to the repository.
"""
import argparse, hashlib, json
from array import array
from pathlib import Path
import sys

def load(path):
    raw=Path(path).read_bytes()
    if len(raw)%4: raise ValueError("Expected stereo int16 PCM (length divisible by four)")
    a=array("h");a.frombytes(raw)
    if sys.byteorder!="little":a.byteswap()
    return raw,a

def analyze(a,b,frames):
    if len(a)!=len(b):raise ValueError("PCM lengths differ; inspect emulator capture cadence")
    mismatches=[i for i,(x,y) in enumerate(zip(a,b)) if x!=y]
    runs=[]
    for i in mismatches:
        if not runs or i>runs[-1][1]+1:runs.append([i,i])
        else:runs[-1][1]=i
    deltas=[b[i]-a[i] for i in mismatches]
    sample_frames=len(a)//2
    return {
        "samples_interleaved":len(a),"stereo_frames":sample_frames,
        "mismatched_samples":len(mismatches),
        "mismatch_percent":round(100*len(mismatches)/len(a),6) if a else 0,
        "first_mismatch_sample":mismatches[0] if mismatches else None,
        "first_mismatch_stereo_frame":mismatches[0]//2 if mismatches else None,
        "first_mismatch_emulated_video_frame_estimate":round(mismatches[0]*frames/len(a),5) if mismatches and frames else None,
        "last_mismatch_sample":mismatches[-1] if mismatches else None,
        "contiguous_mismatch_runs":len(runs),
        "max_absolute_delta":max(map(abs,deltas),default=0),
        "all_deltas_multiple_of_192":all(x%192==0 for x in deltas),
        "channel_mismatch_counts":{"left":sum(i%2==0 for i in mismatches),"right":sum(i%2==1 for i in mismatches)},
        "identical_sample_fraction_with_lag":{
            str(lag):round(sum(a[i]==b[i-lag] for i in range(max(0,lag),min(len(a),len(a)+lag)))/max(1,len(a)-abs(lag)),6)
            for lag in (-4,-2,0,2,4)
        },
        "first_12_mismatch_samples":mismatches[:12],
        "interpretation":"PCM quantization/IRQ timing hypotheses only; compare actual audio mixer/IRQ traces before assigning cause"
    }

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("baseline",type=Path);p.add_argument("candidate",type=Path)
    p.add_argument("--video-frames",type=int,default=180)
    p.add_argument("--json",type=Path)
    x=p.parse_args()
    raw_a,a=load(x.baseline);raw_b,b=load(x.candidate)
    report=analyze(a,b,x.video_frames)
    report["baseline_sha256"]=hashlib.sha256(raw_a).hexdigest()
    report["candidate_sha256"]=hashlib.sha256(raw_b).hexdigest()
    rendered=json.dumps(report,indent=2)+"\n"
    if x.json:x.json.write_text(rendered)
    print(rendered,end="")

if __name__=="__main__":main()
