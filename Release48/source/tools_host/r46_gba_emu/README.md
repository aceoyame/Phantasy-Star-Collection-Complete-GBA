# R46 headless GBA test emulator

`gba.c` is a small ARM7TDMI + PPU (modes 0-5, OBJ incl. affine, windows, blending) + DMA/timer/IRQ
emulator with an HLE BIOS (`bios.s`). Build with `build.sh` (needs clang for the BIOS, gcc for the
library). It boots the full 32 MiB PSC ROM (launcher included) or the PSIV module directly
(`GBA(rom, entry=0x08800000)`). Not cycle-accurate; used for screenshots and scripted regression of
battles, fonts and effects. `gtest.py` forces an enemy skill by breaking on
`battle_enemy_pick_skill`'s return. `boot_debug.py` drives a new game to the debug BOSS BATTLES menu.
