"""Relocate R237 Stage21C experimental helpers without embedding game data.
Requires the user's exact accepted R237 and experimental Stage21C ROMs.
"""
import argparse, hashlib, struct
from pathlib import Path
SIZE=32*1024*1024
BASE=0x08000000
ACCEPTED='a84e9c0501c16a2b750a85037cfa16bcd859101fb4b201ff4edaa1da6b2742de'
PARENT='cc3b2abc0f12b3fdaa4df795994d79221ee2537bc576c8cc643276ad99d6957e'
OUTPUT='3154fb729fdb016574d63dff8dea84e01f1b77e2ac346390530d4f90d932a1a0'
# ARM callsite, previous helper, new zero-filled destination, helper bytes, branch link
RELOCATIONS=((0x08c7d4f4,0x08f97f90,0x0924d8c0,16,True),
             (0x08c4d198,0x08f97f00,0x0924d8d0,60,False))
def sha(x): return hashlib.sha256(x).hexdigest()
def arm_branch(address,target,link):
    offset=target-address-8
    if offset%4 or not -(1<<25)<=offset<(1<<25): raise ValueError('invalid ARM branch')
    return struct.pack('<I',(0xeb000000 if link else 0xea000000)|((offset//4)&0xffffff))
def build(accepted,parent):
    if len(accepted)!=SIZE or sha(accepted)!=ACCEPTED: raise ValueError('wrong accepted R237')
    if len(parent)!=SIZE or sha(parent)!=PARENT: raise ValueError('wrong experimental parent')
    out=bytearray(parent)
    for call,old,new,n,link in RELOCATIONS:
        c,o,d=(x-BASE for x in (call,old,new))
        if parent[c:c+4]!=arm_branch(call,old,link): raise ValueError('wrong branch')
        if parent[d:d+n]!=bytes(n) or accepted[d:d+n]!=bytes(n): raise ValueError('destination occupied')
        if parent[o:o+n]==accepted[o:o+n]: raise ValueError('expected overwritten source bytes')
        out[d:d+n]=parent[o:o+n]
        out[o:o+n]=accepted[o:o+n]
        out[c:c+4]=arm_branch(call,new,link)
    if sha(out)!=OUTPUT: raise ValueError('unexpected output hash')
    return out
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('accepted',type=Path);p.add_argument('parent',type=Path);p.add_argument('output',type=Path)
    a=p.parse_args()
    # Refuse all existing files, including accepted ROM and prior candidate.
    data=build(a.accepted.read_bytes(),a.parent.read_bytes())
    with a.output.open('xb') as f:f.write(data)
    print(sha(data),len(data))
