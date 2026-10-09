#!/usr/bin/env python3
"""Build CP13 from the supplied safe CP07 donor and the complete CP10 tree."""
from pathlib import Path
import argparse,json,re,struct,subprocess,sys,zlib
R=Path(__file__).resolve().parent;ROOT=R.parent
ap=argparse.ArgumentParser()
ap.add_argument('--base',type=Path,default=ROOT/'binaries/RUN_THIS_R48_CP07_SAFE_HANDOFF.gba')
ap.add_argument('--nei',type=Path,default=ROOT/'additional_roms/Nei_Adventure_-_English.bin')
ap.add_argument('--out',type=Path,default=ROOT/'binaries/RUN_THIS_R48_CP13_TEST.gba')
ap.add_argument('--debug-info',action='store_true')
ap.add_argument('--pack-only',action='store_true',help='reuse the compiled core for artwork/panel-only edits')
a=ap.parse_args()
report=ROOT/'build_artifacts/CP13_BUILD_REPORT.json'
core=R/'additional/build/cp13_core.gba';core_report=ROOT/'build_artifacts/CP13_CORE_BUILD.json'
if not a.pack_only:
    subprocess.run([sys.executable,str(R/'build_cp09.py'),'--base',str(a.base),'--nei',str(a.nei),'--out',str(core),'--report',str(core_report)]+(['--debug-info'] if a.debug_info else []),check=True,stdout=subprocess.DEVNULL)
rep=json.loads(core_report.read_text());rom=bytearray(core.read_bytes())
frozen=(ROOT/'build_artifacts/PSC8_CP10_FROZEN.bin').read_bytes()
assert rom[0x6ca500:0x6ca500+len(frozen)]==frozen,'PSC8 must remain exactly CP10'
import gbabuild
from additional.cp11_art import make_card,make_background,encode_card
art=ROOT/'build_artifacts/cp11_art';art.mkdir(exist_ok=True)
card=make_card();bg=make_background();gfx,mapping,n=encode_card(card)
rom[0x8000:0x9400]=bytes(0x1400);rom[0x8000:0x8000+len(gfx)]=gfx
rom[0x9400:0x98b0]=mapping
catalog=(R/'additional/catalog.h').read_text()
bgaddr=int(re.search(r'menu_background \(\(const unsigned char\*\)(\d+)u\)',catalog)[1])-0x8000000
assert bgaddr+38400<=0x1fd5650
rom[bgaddr:bgaddr+38400]=bg.tobytes()
card.save(art/'ADDITIONAL_CARD_INDEXED.png');bg.save(art/'SUBMENU_INDEXED.png')

# This area was verified unused by CP09's ROM-read trace and has been left
# zero in the CP07 donor. Keep it separate from the translated sound driver.
LO,HI=0x078000,0x07c000
assert int(rep['z80_driver_code']['end'],16)<=0x08000000+LO
assert not any(rom[LO:HI]),'PSIII debug region occupied'
p=R/'ps3dbg';o=R/'additional/build';objs=[]
for name in ['thunk.s','ps3dbg.c']:
    obj=o/('ps3dbg_'+name+'.o')
    subprocess.run([gbabuild.CC,'--target=arm-none-eabi','-mcpu=arm7tdmi','-mthumb','-Os','-ffreestanding','-fno-builtin','-fno-unwind-tables','-c',str(p/name),'-o',str(obj)],check=True)
    objs.append(str(obj))
ld=o/'ps3dbg.ld';elf=o/'ps3dbg.elf';blob=o/'ps3dbg.bin'
ld.write_text(f'''ENTRY(ps3dbg_thunk)
SECTIONS {{ . = {hex(0x08000000+LO)}; .rom : {{ *(.start) *(.text*) *(.rodata*) }}
 /DISCARD/ : {{ *(.ARM.exidx*) *(.ARM.extab*) *(.comment*) *(.eh_frame*) }}
 ASSERT(SIZEOF(.rom) < {HI-LO}, "PSIII panel ROM overflow") }}''')
subprocess.run([gbabuild.LD,'-T',str(ld),'-Map='+str(o/'ps3dbg.map'),*objs,'-o',str(elf)],check=True)
subprocess.run([gbabuild.OC,'-O','binary',str(elf),str(blob)],check=True)
data=blob.read_bytes();rom[LO:LO+len(data)]=data
def patch_bl(site,target,expected):
    assert rom[site:site+4]==bytes.fromhex(expected),(hex(site),'unexpected hook bytes')
    delta=(target&~1)-(0x08000000+site+4)
    assert -(1<<22)<=delta<(1<<22) and not delta&1
    rom[site:site+4]=struct.pack('<HH',0xf000|((delta>>12)&2047),0xf800|((delta>>1)&2047))
patch_bl(0x802a2,gbabuild.sym(elf,'ps3dbg_thunk'),'01f043ff')
patch_bl(0x8310e,gbabuild.sym(elf,'ps3mix_thunk'),'fff7e7fd')
a.out.parent.mkdir(parents=True,exist_ok=True)
partial=a.out.with_suffix('.partial');partial.write_bytes(rom);partial.replace(a.out)
rep.update(output=a.out.name,CRC32=f'{zlib.crc32(rom):08x}',checkpoint='CP13')
rep['ps3_debug']={'base':hex(0x08000000+LO),'bytes':len(data),'end':hex(0x08000000+LO+len(data)),
 'boot_hook':'0x080802a2','mix_hook':'0x0808310e','entry':hex(gbabuild.sym(elf,'ps3dbg_thunk')),
 'mix_entry':hex(gbabuild.sym(elf,'ps3mix_thunk'))}
rep['menu_art']={'tiles':n,'card_gfx_bytes':len(gfx),'background_offset':hex(bgaddr),
 'palette_exact':True,'old_blue_strip_removed':True}
report.write_text(json.dumps(rep,indent=2)+'\n')
print(json.dumps({'output':str(a.out),'CRC32':rep['CRC32'],'ps3_panel_bytes':len(data),'hot_end':rep['engines']['md']['hot_end'] if 'md' in rep['engines'] else rep['engines']},indent=2))
