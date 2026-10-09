#!/bin/sh
# bld.sh TAG [cflags]: host sound test with the production bank
S=/home/claude/work/handoff/CLAUDE_R48_CP07_HANDOFF/Release48/source; A=$S/additional; G=$A/gsnd
TAG=$1; shift
cd /home/claude/work/cp10/snd
[ -f bank.o ] || { gcc -O2 -w -Dsnd_data0=snd_data0_unused -I$A -I$G -c $G/gsnd_bank.c -o bank0.o; gcc -O2 -w -c /home/claude/work/cp10/snd/hostbank.c -o hb.o; ld -r bank0.o hb.o -o bank.o; }
gcc -O2 -DMD_HOST -DSND_DEBUG "$@" -I$A -I$G -w $G/test/sndtest.c $A/md.c $A/mdrender.c $A/m68k.c $A/m68k_ref.c $A/mdsnd.c $A/mdsndmix.c bank.o -o sndtest_$TAG 2>&1 | head -20
[ -x sndtest_$TAG ] && echo built sndtest_$TAG
