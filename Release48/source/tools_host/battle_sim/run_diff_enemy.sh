#!/bin/sh
# R15 host differential test: battle_draw_enemy span kernel vs R14 per-pixel loop.
# Builds the REAL title_runtime_v06.c for x86-64 with GBA RAM/IO/VRAM mmapped at their
# real addresses (binary relinked at 0x40000000), DMA helpers replaced by CPU copies.
# Usage: tools_host/battle_sim/run_diff_enemy.sh   (from source/; needs gcc/g++/as)
set -e
SRC=$(cd "$(dirname "$0")/../.." && pwd); W=${TMPDIR:-/tmp}/ps4_bsim; mkdir -p "$W"; cd "$W"
D="-DMD2GBA_PS4_MAXMOD_MODE_A=1 -DMD2GBA_PS4_MAXMOD_CHANNEL_MAJOR=1 -DMD2GBA_PS4_V19_FULL_MIX=1 -DMD2GBA_PS4_M4A_NATIVE_ONLY=1"
python3 - "$SRC" <<'PY'
import sys
s=open(sys.argv[1]+'/gba/title_runtime_v06.c').read()
def body(s,sig,nb):
    i=s.index(sig); j=s.index('\n}\n',i)+3; return s[:i]+sig+nb+'\n}\n'+s[j:]
s=body(s,'static void dma_copy32(const void* src,void* dst,unsigned bytes){','\n    if(!bytes) return; { const uint32_t* a=(const uint32_t*)src; uint32_t* b=(uint32_t*)dst; for(unsigned i=0;i<bytes/4u;i++) b[i]=a[i]; }')
s=body(s,'static void dma_fill32(void* dst,unsigned bytes,uint32_t value){','\n    { uint32_t* b=(uint32_t*)dst; for(unsigned i=0;i<bytes/4u;i++) b[i]=value; }')
open('title_host.c','w').write(s)
s=open(sys.argv[1]+'/gba/ps4_audio_stream.c').read()
s=s.replace('#define REG16(a) (*(volatile uint16_t*)(a))','extern volatile uint16_t hw_reg16[0x200];\n#define REG16(a) (hw_reg16[((a)-0x04000000u)>>1])')
s=s.replace('#define REG32(a) (*(volatile uint32_t*)(a))','extern volatile uint32_t hw_reg32[0x100];\n#define REG32(a) (hw_reg32[((a)-0x04000000u)>>2])')
s=s.replace('*(void (* volatile*)(void))0x03007FFCu=ps4_audio_irq_entry;','(void)ps4_audio_irq_entry;')
open('audio_host.c','w').write(s)
PY
printf '#include <stdint.h>\nvolatile uint16_t hw_reg16[0x200]; volatile uint32_t hw_reg32[0x100];\nvoid ps4_audio_irq_entry(void){} void ps4_irq_cpu_enable(void){}\n' > stubs.c
O=""
for f in "$SRC"/gba/*.s; do b=$(basename "$f" .s); case $b in startup*|ps4_audio_irq|pcm_tracks) continue;; esac
  grep -v '^\s*\.syntax\|^\s*\.cpu\|^\s*\.arm\b\|^\s*\.thumb' "$f" | sed 's/%progbits/@progbits/' > $b.hs; (cd "$SRC" && as -I gba "$W/$b.hs" -o "$W/$b.o"); O="$O $b.o"; done
for c in md_bus_runtime md_io_runtime psc_field_predecode psc_map_desc psc_runtime_plan psc_panel_bank psc_field_fastassets source_ability_effects source_enemy_effects; do gcc -O1 -w -fno-pie -no-pie $D -I"$SRC/gba" -c "$SRC/gba/$c.c" -o $c.o; O="$O $c.o"; done
gcc -O1 -w -fno-pie -no-pie $D -I"$SRC/gba" -c audio_host.c -o audio_host.o
for c in ps4_audio_fallback audio_p134r/src/chip_audio audio_p134r/src/ps4_chip_audio_data audio_p134r/src/ps4_fm_multisample_data; do g++ -O1 -w -fno-pie -no-pie -std=c++17 $D -I"$SRC/gba" -I"$SRC/gba/audio_p134r/include" -I"$SRC/gba/audio_p134r/src" -c "$SRC/gba/$c.cpp" -o $(basename $c).o; O="$O $(basename $c).o"; done
gcc -O1 -w -c stubs.c -o stubs.o
gcc -O1 -w -fno-pie -no-pie $D -I"$W" -I"$SRC/gba" -I"$SRC/gba/audio_p134r/include" -c "$SRC/tools_host/battle_sim/diff_enemy.c" -o diff_enemy.o
g++ -no-pie -fno-pie -Wl,-Ttext-segment=0x40000000 -o diff_enemy diff_enemy.o audio_host.o stubs.o $O
./diff_enemy
