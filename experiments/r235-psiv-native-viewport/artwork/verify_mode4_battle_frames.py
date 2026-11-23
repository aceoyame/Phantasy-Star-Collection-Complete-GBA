#!/usr/bin/env python3
"""Reconstruct R235's 240x160 battle screen directly from live Mode4 VRAM.

Supply: --rom SHA-verified experimental GBA, --toolkit headless toolkit root,
--save-cart test cart save, --llvm-dir clang/ld.lld/llvm-objcopy bin directory.
This test inserts a nonshipping debug battle driver in an ephemeral ROM only.
No copyrighted ROM, palette, game assets or screenshots are embedded.
"""
import argparse,hashlib,json,os,sys,tempfile,zlib
from pathlib import Path
import numpy as np

EXPECTED='5ff6d48606576e19ee46d14d7d8892b9835d4161afebde74f781caf76d38ff02'
STEPS=[('',0),('A',10),('B',10),('A',10),('A',20),('A',20),
       ('A',20),('A',20),('A',50),('A',50),('',90)]
def run(rom, toolkit, cart, llvm, output=None):
    root=toolkit/'qa'
    sys.path[:0]=[str(root/'probe'),str(root/'tests')]
    os.environ['CP08_PROBE_LIB']=str(root/'probe/probe.so')
    from mgba2 import GBA
    from contexts import build_driver
    assert hashlib.sha256(rom.read_bytes()).hexdigest()==EXPECTED,'Stage12 hash mismatch'
    with tempfile.TemporaryDirectory() as t:
        driver=build_driver(rom,llvm,Path(t),0x109,False)
        g=GBA(driver,0x08800000)
        g.put(0x0e000000,cart.read_bytes()[:65536])
        g.run(600)
        for k in ('START','START','A','A'):g.press(k,3,240)
        g.run(180);g.press('START',3,0);g.run(100)
        rows=[]
        for key,n in STEPS:
            if key:g.press(key,3,n)
            elif n:g.run(n)
            display=g.rd16(0x04000000)
            assert display&7==4 and display&0x400,'battle not using Mode4 BG2'
            page=0x0600A000 if display&0x10 else 0x06000000
            indices=np.frombuffer(g.mem(page,38400),dtype=np.uint8).reshape(160,240)
            palette=np.frombuffer(g.mem(0x05000000,512),dtype='<u2')
            channels=[(palette>>shift)&31 for shift in (0,5,10)]
            rgb=np.stack([(c<<3)|(c>>2) for c in channels],axis=-1).astype(np.uint8)[indices]
            shot=np.asarray(g.shot())
            incorrect=np.count_nonzero(np.any(rgb!=shot,axis=2))
            assert incorrect==0,f'R235 frame {g.frame()}: {incorrect} pixels differ'
            rows.append(dict(frame=g.frame(),dispcnt=hex(display),pixel_mismatches=incorrect,
                             framebuffer_crc32=f'{zlib.crc32(indices.tobytes()):08x}'))
    result=dict(status='PASS',frames=len(rows),source_sha256=EXPECTED,
      scene='native single-enemy battle #09',max_pixel_mismatches=0,checks=rows,
      caveat='Exact GBA framebuffer reconstruction is not Genesis-versus-GBA artistic fidelity validation')
    if output:output.write_text(json.dumps(result,indent=2)+'\n')
    return result
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('rom','toolkit','save_cart','llvm_dir'):
        p.add_argument('--'+name.replace('_','-'),type=Path,required=True)
    p.add_argument('--output',type=Path)
    a=p.parse_args()
    print(json.dumps(run(a.rom,a.toolkit,a.save_cart,a.llvm_dir,a.output),indent=2))
