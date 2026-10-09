#!/bin/sh
# mdhost_aot: host harness with statically recompiled code.  usage: build_aot.sh GENDIR
set -e
cd "$(dirname "$0")"
A=../../additional
CF="-O1 -g -DMD_HOST -DAOT_ON -I$A -I$A/aot -w"
gcc $CF mdhost.c $A/md.c $A/mdrender.c $A/m68k.c $A/m68k_ref.c $A/aot/aotdisp.c $1/aot_code.c $1/aot_tabs.c -o mdhost_aot
echo built mdhost_aot
