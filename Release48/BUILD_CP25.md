# Build and verify CP25

Extract the complete ZIP, then run from its root:

```sh
bash tools/build_cp25.sh /absolute/path/to/Release48
```

The script extracts the included LLVM18 Linux x86-64 toolchain when needed,
sets compiler paths, calls `Release48/source/build_cp25.py`, and checks the
33,554,432-byte ROM and CRC32 `056427B8`. Python 3 and standard tar are needed.
Keep the accepted generated AOT source. No raw AOT regeneration is needed.
The older BUILD.md documents historical builds, not the current checkpoint.

Host tests need GCC. GBA tests use the provided unmodified mGBA 0.10.5 source,
GCC/zlib and Python Pillow. To build their runtime and bridge:

```sh
cd Release48
python3 source/tools_host/r46_mgba/build_minimal.py --out /absolute/path/mgba-runtime
python3 source/tools_host/cp08_mgba/build_probe2.py --runtime-dir /absolute/path/mgba-runtime
```

Set PATH to `Release48/compiler/root/usr/lib/llvm-18/bin` and
LD_LIBRARY_PATH to `Release48/compiler/root/usr/lib/x86_64-linux-gnu`
(as in the build script). The current checks are:

```sh
python3 source/tools_host/check_cp25_fixed_cpu.py
python3 source/tools_host/check_cp25_bytes_boundaries.py
python3 source/tools_host/check_cp25_render.py
python3 source/tools_host/check_cp25_gba_busy.py
python3 source/tools_host/check_cp25_gba_pcm.py
python3 source/tools_host/check_cp25_saves.py
python3 source/tools_host/check_cp25_psc8_saves.py
CP25_SPEED_GAMES=2,3,4,5,6,7,8,9 python3 source/tools_host/check_cp25_speed.py
python3 source/additional/check_cp09.py --rom binaries/RUN_THIS_R48_CP25_ADVENTURE_INDEX.gba --base binaries/RUN_THIS_R48_CP07_SAFE_HANDOFF.gba --report build_artifacts/CP25_BUILD_REPORT.json --out ../validation_cp25/collection --static-only
```

The CP24 baseline ROM is in Release48/binaries and its ELF/source snapshot
is in cp24_base. Sealed CP21 CPU references remain in source/references/cp21_final.
Evidence lives in validation_cp25; earlier validation is retained.
The supplied mGBA bridge is rebuilt locally; host binaries are not included.

For timings, the CP24 cycle baseline can be reused:
`CP25_SPEED_BASE=../validation_cp24/FIXED_GUEST_SPEED.json CP25_SPEED_GAMES=2,3,4,5,6,7,8,9 python3 source/tools_host/check_cp25_speed.py`.
All checks run from Release48 with the included compiler paths configured.
Keep the accepted generated AOT source and its regeneration guard.
