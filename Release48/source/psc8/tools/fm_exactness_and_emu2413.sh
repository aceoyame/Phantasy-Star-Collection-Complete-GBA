#!/bin/sh
set -e
cd $(dirname $0)/../../psc8/tests
gcc -O2 -DFM_STEADY_C -I.. -o fm_block_test fm_block_test.c ../fast_opll.c ../fast_opll_block.c
./fm_block_test
cd ../../fmcmp
gcc -O2 -w -DFM_STEADY_C -I../psc8 -I../psc8/vendor/emu2413 -o render render.c ../psc8/fast_opll.c ../psc8/fast_opll_block.c ../psc8/vendor/emu2413/emu2413.c -lm
./render fmlog.txt
python3 metric.py fast.raw
