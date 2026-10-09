#!/bin/sh
# Builds the GBA-side FM test (fmtest/: random register writes, fastopll_render vs n x fastopll_calc, run on the GBA so the assembly
# loops are what gets tested) into $1 (default build/fmtest.gba).  Run it with tools/run_fmtest.py.   Run from psc8/.
set -e
OUT=${1:-build/fmtest.gba}; T=${TMPDIR:-/tmp}/fmtest.$$; mkdir -p $T
CC=${PSC8_CC:-clang}; F="--target=arm-none-eabi -mcpu=arm7tdmi -marm -O2 -ffreestanding -fno-builtin -fno-unwind-tables -fno-exceptions -Iinclude -I. -Wno-missing-braces"
$CC $F -c fmtest/start.s -o $T/start.o
$CC $F -c fmtest/fmt.c -o $T/fmt.o
$CC $F -Os -c fast_opll.c -o $T/fo.o
$CC $F -O2 -c fast_opll_block.c -o $T/fob.o
$CC $F -c fm_steady.s -o $T/fs.o
${PSC8_LD:-ld.lld} -T fmtest/t.ld $T/start.o $T/fmt.o $T/fo.o $T/fob.o $T/fs.o -o $T/fmt.elf
${PSC8_OBJCOPY:-llvm-objcopy} -O binary $T/fmt.elf $T/fmt.bin
python3 -I - $T/fmt.bin $OUT <<'P'
import sys
b=open(sys.argv[1],'rb').read(); open(sys.argv[2],'wb').write(bytes(0xC0)+b+bytes(4))
P
rm -rf $T; echo "built $OUT"
