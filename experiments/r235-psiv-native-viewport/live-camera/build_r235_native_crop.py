#!/usr/bin/env python3
"""Build a separate, non-production R235 native-pixel field-camera test ROM.
Operator supplies a legally obtained accepted R235 ROM. No ROMs are in Git.
Requires clang, ld.lld, llvm-objcopy supporting arm-none-eabi.
"""
import argparse,hashlib,json,struct,subprocess,tempfile,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parent
BASE=0x08000000
SOURCE_SHA='4caf23ae8b030c16a32658fdc95085330de6c6dd4a51498f0e2b9c17c7d89308'
EXPECTED_CANDIDATE='f3a80bad91b6d3f1809bf1e09ea09f887e204428aeb55247bf67426caf03f89d'
BG_HOOK=0x08C4D198
BG_CODE=0x08F97F00
ACTOR_HOOK=0x08CD3854
ACTOR_CODE=0x08F97D64

def assemble(source,entry,origin):
    with tempfile.TemporaryDirectory() as folder:
        root=Path(folder)
        (root/'layout.ld').write_text(f'SECTIONS {{ .probe {origin:#x} : {{ *(.probe) }} }}\n')
        commands=[
            ['clang','--target=arm-none-eabi','-mcpu=arm7tdmi','-marm','-c',str(ROOT/source),'-o',str(root/'p.o')],
            ['ld.lld','-T',str(root/'layout.ld'),'-e',entry,str(root/'p.o'),'-o',str(root/'p.elf')],
            ['llvm-objcopy','-O','binary','--only-section=.probe',str(root/'p.elf'),str(root/'p.bin')]
        ]
        for command in commands:
            subprocess.run(command,check=True)
        return (root/'p.bin').read_bytes()

def branch(source,destination):
    delta=destination-source-8
    assert delta%4==0 and -(1<<25)<=delta<(1<<25)
    return struct.pack('<I',0xEA000000|((delta>>2)&0xFFFFFF))

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('parent',type=Path)
    p.add_argument('output',type=Path)
    args=p.parse_args()
    original=args.parent.read_bytes()
    assert len(original)==33554432
    assert hashlib.sha256(original).hexdigest()==SOURCE_SHA,'R235 parent SHA mismatch'
    assert original[ACTOR_HOOK-BASE:ACTOR_HOOK-BASE+4]==bytes.fromhex('a999ffea')
    assert original[BG_HOOK-BASE:BG_HOOK-BASE+4]==bytes.fromhex('30002de9')
    actor=assemble('actor_pan_16x8.S','actor_shift_probe',ACTOR_CODE)
    bg=assemble('bg_pan_16x8.S','bg_scroll_shift_probe',BG_CODE)
    assert len(actor)==132 and len(bg)==60,(len(actor),len(bg))
    assert ACTOR_CODE+len(actor)<BG_CODE and BG_CODE+len(bg)<0x08F98100
    patched=bytearray(original)
    patches=[
        (ACTOR_CODE,actor),(BG_CODE,bg),
        (ACTOR_HOOK,branch(ACTOR_HOOK,ACTOR_CODE)),
        (BG_HOOK,branch(BG_HOOK,BG_CODE))
    ]
    for address,data in patches:
        patched[address-BASE:address-BASE+len(data)]=data
    assert patched[:0xC0]==original[:0xC0],'GBA header modified'
    changed=sum(a!=b for a,b in zip(original,patched))
    assert changed==193,changed
    result=dict(
        parent_sha256=SOURCE_SHA,
        candidate_sha256=hashlib.sha256(patched).hexdigest(),
        expected_candidate_sha256=EXPECTED_CANDIDATE,
        crc32=f'{zlib.crc32(patched):08x}',
        changed_bytes=changed,rom_bytes=len(patched),
        implementation='fixed 16px X + 8px Y crop, no scaling, field renderer only',
        patches=[dict(address=hex(address),bytes=len(data)) for address,data in patches],
    )
    assert result['candidate_sha256']==EXPECTED_CANDIDATE,'binary does not match tested candidate'
    args.output.write_bytes(patched)
    print(json.dumps(result,indent=2))
if __name__=='__main__':
    main()
