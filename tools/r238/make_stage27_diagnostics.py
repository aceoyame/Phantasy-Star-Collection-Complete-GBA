#!/usr/bin/env python3
"""Create isolated test-only R238 probes; never distribute as playable builds."""
import argparse, hashlib, struct
from pathlib import Path
N=32*1024*1024
H237='a84e9c0501c16a2b750a85037cfa16bcd859101fb4b201ff4edaa1da6b2742de'
H238='9d9ca1b32cfdd844d648fe00caa9549bd702c193d4753d7de9dc5cadd2b365fa'
BASE=0x08000000
def sha(b):return hashlib.sha256(b).hexdigest()
def load(p,h):
 b=Path(p).read_bytes()
 if len(b)!=N or sha(b)!=h:raise ValueError('wrong ROM input: '+str(p))
 return b
def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('r237',type=Path);p.add_argument('experimental',type=Path);p.add_argument('out',type=Path)
 a=p.parse_args();base=load(a.r237,H237);candidate=load(a.experimental,H238)
 a.out.mkdir(parents=True,exist_ok=True)
 for name,sites in [('NO_FOG_TEST_ONLY',(0x08866F84,)),('NORMAL_LOADER_TEST_ONLY',(0x0882FD68,0x0882FDB8))]:
  b=bytearray(candidate)
  for address in sites:
   off=address-BASE
   if b[off:off+4]==base[off:off+4]:raise ValueError('expected changed hook')
   b[off:off+4]=base[off:off+4]
  with (a.out/(name+'.gba')).open('xb') as f:f.write(b)
  print(name,sha(b))
 # Tested 32-byte quick gate; rejected: does not restore full timing/PCM parity.
 stub=struct.pack('<8I',0xe59f0014,0xe5d01000,0xe3510000,0x05d01001,0x03510002,0x8ad85d34,0xeafffae5,0x0200ec28)
 b=bytearray(candidate);loc=0x09250200;off=loc-BASE
 if any(b[off:off+len(stub)]):raise ValueError('gate cave occupied')
 b[off:off+len(stub)]=stub;site=0x08866f84
 struct.pack_into('<I',b,site-BASE,0xeb000000|(((loc-site-8)//4)&0xffffff))
 with (a.out/'FAST_FOG_TEST_ONLY.gba').open('xb') as f:f.write(b)
 print('FAST_FOG_TEST_ONLY',sha(b))
if __name__=='__main__':main()
