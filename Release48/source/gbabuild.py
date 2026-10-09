"""Shared ARM build helpers for the CP08+ engines (Genesis runtime, menu)."""
from pathlib import Path
import os, subprocess

R = Path(__file__).resolve().parent
A = R / 'additional'
O = A / 'build'
O.mkdir(exist_ok=True)
CC = os.environ.get('PSC8_CC', 'clang')
LD = os.environ.get('PSC8_LD', 'ld.lld')
OC = os.environ.get('PSC8_OBJCOPY', 'llvm-objcopy')
HOT_FILES = ('m68k.c', 'md.c', 'mdrender.c', 'mdsndmix.c')          # .text -> IWRAM (.hot)
MD_FILES = ('mdmain.c', 'mdrt.c', 'm68k_ref.c', 'm68k.c', 'md.c', 'mdrender.c', 'mdsnd.c', 'mdsndmix.c', 'gsnd/gsnd_bank.c')
MD_THUMB = ()                                # Z80 core + synthesiser: Thumb, run from ROM


def flags(opt='-Os', debug=False):
    return ['--target=arm-none-eabi', '-mcpu=arm7tdmi', '-marm', opt, '-ffreestanding', '-fno-builtin',
            '-fno-unwind-tables', '-I' + str(R / 'psc8/include'), '-I' + str(A)] + (['-g'] if debug else [])


def build(name, files, addr, entry, hot_limit, opt='-Os', debug=False, extra_objs=(), rom2=None, defines=(), thumb_files=(), cold=(), sndidx=None, zaot=None, aotextra=None):
    F = flags(opt, debug) + ['-D' + d for d in defines]
    start = (R / 'psc8/start.s').read_text().replace('psc8_start', name + '_start').replace('psc8_main', entry)
    if name == 'md':                                   # user stack top 0x03007F00 (the BIOS default): the IRQ stack (0x03007FA0) has 160 B below it
        assert '0x03007e00' in start
        start = start.replace('0x03007e00', '0x03007f00')
    sp = O / (name + '_start.s')
    sp.write_text(start)
    objs = []
    for file in [sp, *files]:
        obj = O / (name + '_' + file.name + '.o')
        ff = F + (['-O2'] if file.name == 'mdsnd.c' else []) + (['-mthumb', '-O2', '-I' + str(A / 'aot')] if file in thumb_files else ['-I' + str(A / 'aot')])
        subprocess.run([CC, *ff, '-c', str(file), '-o', str(obj)], check=True)
        if zaot and file.name == 'mdsnd.c':           # the whole sound engine (Thumb, runs from ROM) goes to the .zaot region
            subprocess.run([OC, '--rename-section', '.text=.zaot,alloc,load,readonly,code,contents',
                            '--rename-section', '.rodata=.zaot.ro,alloc,load,readonly,data,contents', str(obj)], check=True)
        if file.name in HOT_FILES and file.name not in cold:
            subprocess.run([OC, '--rename-section', '.text=.hot,alloc,load,readonly,code,contents', str(obj)], check=True)
        objs.append(str(obj))
    objs += [str(x) for x in extra_objs]
    rom2s = f' .rom2 {hex(rom2)} : {{ *(.rom2*) }}' if rom2 else ''
    sndidxs = f' .sndidx {hex(sndidx)} : {{ *(.sndidx*) }}' if sndidx else ''     # first: output sections must be in address order
    if zaot:                                          # translated Z80 sound driver: its own ROM region (after the sound bank)
        sndidxs += f' .zaot {hex(zaot)} : {{ *(.zaot*) }}'
    if name=='md' and aotextra:
        sndidxs += ' .aotmore 0x08079000 : { *(.aotmore*) }'
    if aotextra:
        sndidxs += f' .aotextra {hex(aotextra)} : {{ *(.aotextra*) }}'
    native_bounds = 'ASSERT(SIZEOF(.aotmore)<=0x3000,"native extension overflow") ASSERT(ADDR(.zaot)+SIZEOF(.zaot)<=0x08078000,"sound code crosses PSIII panel")' if name=='md' and aotextra else ''
    script = f'''ENTRY({name}_start)
SECTIONS {{{sndidxs} . = {hex(addr)}; .rom : {{ *(.start) *(.text*) *(.rodata*) }} . = ALIGN(4); __hot_load = .;
 .hot 0x03000000 : AT(__hot_load) {{ __hot_start = .; *(.hot*) *(.hotdata*) __hot_end = ALIGN(4); }}
 __data_load = __hot_load + SIZEOF(.hot);
 .data 0x02000000 : AT(__data_load) {{ __data_start = .; *(.data*) __data_end = ALIGN(4); }}
 __rom_end = __data_load + SIZEOF(.data);
 .bss (NOLOAD) : {{ __bss_start = .; *(.bss*) *(COMMON) __bss_end = ALIGN(4); }}
{rom2s}
 /DISCARD/ : {{ *(.ARM.exidx*) *(.ARM.extab*) *(.comment*) *(.eh_frame*) }}
{native_bounds}
 ASSERT(__bss_end < 0x0203e000, "EWRAM overflow")
 ASSERT(__hot_end < {hot_limit}, "IWRAM overflow") }}'''
    ld = O / (name + '.ld')
    ld.write_text(script)
    elf = O / (name + '.elf')
    subprocess.run([LD, '-T', str(ld), '-Map=' + str(O / (name + '.map')), *objs, '-o', str(elf)], check=True)
    out = O / (name + '.bin')
    subprocess.run([OC, '-O', 'binary', '--remove-section=.rom2', '--remove-section=.sndidx', '--remove-section=.zaot', '--remove-section=.aotextra', '--remove-section=.aotmore', str(elf), str(out)], check=True)
    if name=='md' and aotextra:
        subprocess.run([OC, '-O', 'binary', '--only-section=.aotmore', str(elf), str(O / (name + '_aotmore.bin'))], check=True)
    if aotextra:
        subprocess.run([OC, '-O', 'binary', '--only-section=.aotextra', str(elf), str(O / (name + '_aotextra.bin'))], check=True)
    if zaot:
        subprocess.run([OC, '-O', 'binary', '--only-section=.zaot', str(elf), str(O / (name + '_zaot.bin'))], check=True)
    if rom2:
        subprocess.run([OC, '-O', 'binary', '--only-section=.rom2', str(elf), str(O / (name + '_rom2.bin'))], check=True)
    if sndidx:
        subprocess.run([OC, '-O', 'binary', '--only-section=.sndidx', str(elf), str(O / (name + '_sndidx.bin'))], check=True)
    return out.read_bytes(), elf


def sym(elf, name):
    t = subprocess.run(['llvm-nm', str(elf)], capture_output=True, text=True, check=True).stdout
    for line in t.splitlines():
        p = line.split()
        if len(p) == 3 and p[2] == name:
            return int(p[0], 16)
    raise KeyError(name)


HOT_FILES = HOT_FILES + ('aotdisp.c',)


def build_md(hot_limit='0x03007800', opt='-Os', debug=False, aot=(), rom2=None, sndidx=None, zaot=None, aotextra=None):
    files = [A / f for f in MD_FILES] + [A / 'divmod.s']
    if aot:
        files += [A / 'aot/aotdisp.c'] + list(aot)
    return build('md', files, 0x086e2000, 'md_main', hot_limit, opt, debug, defines=(('AOT_ON',) + (('AOT_ROM2',) if rom2 else ())) if aot else (), thumb_files=tuple(aot) + tuple(A / f for f in MD_THUMB), cold=(), rom2=rom2, sndidx=sndidx, zaot=zaot, aotextra=aotextra)


def build_menu(opt='-Os'):
    return build('menu', [A / 'menu.c'], 0x08005000, 'menu_main', '0x03007000', opt)
