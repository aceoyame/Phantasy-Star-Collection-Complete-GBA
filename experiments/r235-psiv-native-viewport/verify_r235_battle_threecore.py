#!/usr/bin/env python3
"""R235 battle regression against a field-hook canary (no UI modification yet).
Requires operator-supplied original and candidate 32MiB ROMs plus extracted
PSC_HEADLESS_EMULATOR_TOOLKIT_2026-11-16.zip.
Usage:
 python verify_r235_battle_threecore.py --baseline accepted.gba --canary canary.gba --toolkit toolkit/ --output qa_out/
"""
import argparse, hashlib, json, os, sys, tempfile, zlib
from pathlib import Path
PARENT_SHA='4caf23ae8b030c16a32658fdc95085330de6c6dd4a51498f0e2b9c17c7d89308'
CANARY_SHA='da48301d7285822dc0306be174e87221dd744bd3b8843b2e7a3dd80f1880feea'
def run(rom, name, core_name, toolkit, output, llvm_dir):
    from contexts import build_driver
    with tempfile.TemporaryDirectory() as tmp:
        test_rom=build_driver(rom,llvm_dir,Path(tmp),6,False)
        if core_name=='mgba':
            from mgba2 import GBA
            game=GBA(test_rom,0x08800000)
        else:
            from vbam86 import VBA
            game=VBA(test_rom,0x08800000)
        try:
            fixture=(toolkit/'qa/save_cart_slot149.bin').read_bytes()
            game.put(0x0e000000,fixture[:65536 if core_name=='mgba' else 32768])
            game.run(600)
            for key in ('START','START','A','A'):
                game.press(key,3,240)
            game.run(180)
            checks=[]
            def frame(label):
                shot=game.shot()
                shot.save(output/f'{core_name}_{name}_{label}.png')
                checks.append(dict(label=label,frame=game.frame(),crc32=f'{zlib.crc32(shot.tobytes()):08x}',
                                   state=game.rd8(0x02028d04),width=240,height=160))
            game.press('START',3,0); game.run(90);frame('command')
            game.press('A',3,10);frame('commands_open')
            game.press('RIGHT',3,10);frame('selected')
            return checks
        finally:
            if hasattr(game,'close'):game.close()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('baseline','canary','toolkit','output','llvm-dir'):
        p.add_argument('--'+name,type=Path,required=name!='llvm-dir',default=Path('/usr/local/swift/usr/bin') if name=='llvm-dir' else None)
    a=p.parse_args()
    assert hashlib.sha256(a.baseline.read_bytes()).hexdigest()==PARENT_SHA
    assert hashlib.sha256(a.canary.read_bytes()).hexdigest()==CANARY_SHA
    a.output.mkdir(parents=True,exist_ok=True)
    os.environ['CP08_PROBE_LIB']=str(a.toolkit/'qa/probe/probe.so')
    sys.path[:0]=[str(a.toolkit/'qa/probe'),str(a.toolkit/'qa/tests')]
    checks={}
    for core,kind in (('mgba',None),('vbam','libretro'),('delta','delta')):
        if kind:os.environ['PSC_VBAM_CORE']=str(a.toolkit/f'qa/vbam/{kind}/vbam_libretro.so')
        original=run(a.baseline,'accepted',core,a.toolkit,a.output,a.llvm_dir)
        canary=run(a.canary,'canary',core,a.toolkit,a.output,a.llvm_dir)
        assert original==canary,(core,original,canary)
        assert all(x['state']==6 for x in original)
        checks[core]=original
    result={'status':'PASS','purpose':'No-op battle parity guard; NOT alternate live battle UI',
            'accepted_sha256':PARENT_SHA,'canary_sha256':CANARY_SHA,'cores':checks}
    (a.output/'results.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))
if __name__=='__main__':main()
