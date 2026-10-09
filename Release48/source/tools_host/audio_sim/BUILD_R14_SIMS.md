# R14 host audio simulators (x86 gcc/g++, no ARM toolchain needed)

Both link the **real** `gba/ps4_audio_stream.c`, `gba/ps4_audio_fallback.cpp`, P134R `chip_audio.cpp`
+ generated data and the SFX bank. Only REG16/REG32 are redirected to arrays, IWRAM section attributes
are dropped, and the 0x03007FFC vector write is removed (host copy only).

```
S=<tree>/source/gba; D="-DMD2GBA_PS4_MAXMOD_MODE_A=1 -DMD2GBA_PS4_MAXMOD_CHANNEL_MAJOR=1 -DMD2GBA_PS4_V19_FULL_MIX=1 -DMD2GBA_PS4_M4A_NATIVE_ONLY=1"
# host copy of the stream (python one-liner used in R14):
#   REG16(a) -> hw_reg16[((a)-0x04000000u)>>1], REG32(a) -> hw_reg32[...>>2],
#   '__attribute__((section(".iwram.text"),noinline))' -> '__attribute__((noinline))',
#   '*(void (* volatile*)(void))0x03007FFCu=ps4_audio_irq_entry;' -> '(void)ps4_audio_irq_entry;'
#   append: host_mix_buffer(), host_mix_buffer_size(), host_silence()
# SFX bank: sfx_host.s = gba/sfx_bank.s with '@progbits' and an absolute .incbin path
gcc -O1 -w -no-pie -fno-pie $D -I$S -c audio_host14.c
for f in $S/ps4_audio_fallback.cpp $S/audio_p134r/src/chip_audio.cpp $S/audio_p134r/src/ps4_chip_audio_data.cpp $S/audio_p134r/src/ps4_fm_multisample_data.cpp; do
  g++ -O1 -w -no-pie -fno-pie -std=c++17 $D -I$S -I$S/audio_p134r/include -I$S/audio_p134r/src -c $f; done
as sfx_host.s -o sfx_host.o
# chip_audio_prof.cpp = chip_audio.cpp + per-frame counters prof_fm/prof_psg/prof_noise/prof_dac (lane profile)
gcc -O1 -w -no-pie -fno-pie -I$S -c sim_r14.c; gcc -O1 -w -no-pie -fno-pie -I$S -c sim_irq.c
g++ -no-pie -fno-pie -o sim_r14 sim_r14.o audio_host.o ps4_audio_fallback.o chip_audio.o ps4_chip_audio_data.o ps4_fm_multisample_data.o sfx_host.o -Wl,--wrap=ps4_audio_fallback_render_224
g++ -no-pie -fno-pie -o sim_irq sim_irq.o audio_host14.o ps4_audio_fallback.o chip_audio.o ps4_chip_audio_data.o ps4_fm_multisample_data.o sfx_host.o -Wl,--wrap=ps4_audio_fallback_render_224
./sim_r14 95 <scenario 0..4> 1200 [p]   # polling transport (R13 or R14 built with -DPS4_AUDIO_IRQ_TRANSPORT=0)
./sim_irq 95 <max IRQ delay in frames, negative = constant> <sfx storm 0/1> [frames]
```
Scenario key (sim_r14): 0 on time, 1 every iteration 1.15 frames, 2 alternating 0.8/1.6, 3 one 30-frame
stall, 4 random 0.7–2.2 frames. `garbage_bytes` counts FIFO reads outside `mix_buffer`.
