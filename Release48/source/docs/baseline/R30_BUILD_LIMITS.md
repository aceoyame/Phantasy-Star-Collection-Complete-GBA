# R30 build limits

- PSC module: 24,211,728 bytes; 954,096 bytes remain in the 24 MiB slot.
- Combined ROM: 32 MiB. The original first 8 MiB shell is retained byte for byte.
- IWRAM allocation ends at 0x03006B18; 488 bytes remain before the reserved IRQ stack.
- EWRAM BSS ends at 0x02029FFD; 3 bytes remain before the presentation arena. No ordinary EWRAM globals added.
- New state uses the existing source RAM: tile animation records E800, field cadence ECE2/ECE3/ECE5, original map update bytes and existing reload continuation.
- Accepted audio: 34 files compare directly with R29. No SHA256 calculated.
- One focused ASan/UBSan controller probe passes. Leak scanning is disabled because this host cannot access the needed process information; the probe allocates no heap memory.
- No emulator playthrough, hardware timing certification or audio retuning.
