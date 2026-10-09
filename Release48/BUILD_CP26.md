# Build and verify CP26

Extract the complete ZIP, then run from its root:

```sh
bash tools/build_cp26.sh /absolute/path/to/Release48
```

The script extracts the included LLVM18 Linux x86-64 toolchain when needed,
sets compiler paths, calls `Release48/source/build_cp26.py`, and checks the
33,554,432-byte ROM and CRC32 `73A056D3`. Python 3 and standard tar are needed.
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
python3 source/tools_host/check_cp26_fixed_cpu.py
python3 source/tools_host/check_cp26_nibble_boundaries.py
python3 source/tools_host/check_cp26_render.py
python3 source/tools_host/check_cp26_gba_busy.py
python3 source/tools_host/check_cp26_gba_pcm.py
python3 source/tools_host/check_cp26_saves.py
python3 source/tools_host/check_cp26_psc8_saves.py
CP26_SPEED_GAMES=2,3,4,5,6,7,8,9 python3 source/tools_host/check_cp26_speed.py
python3 source/additional/check_cp09.py --rom binaries/RUN_THIS_R48_CP26_NIBBLE_NATIVE.gba --base binaries/RUN_THIS_R48_CP07_SAFE_HANDOFF.gba --report build_artifacts/CP26_BUILD_REPORT.json --out ../validation_cp26/collection --static-only
```

The CP25 baseline ROM is in Release48/binaries and its ELF/source snapshot
is in cp25_base. Sealed CP21 CPU references remain in source/references/cp21_final.
Evidence lives in validation_cp26; earlier validation is retained.
The supplied mGBA bridge is rebuilt locally; host binaries are not included.

For timings, the CP25 cycle baseline can be reused:
`CP26_SPEED_BASE=../validation_cp25/FIXED_GUEST_SPEED.json CP26_SPEED_GAMES=2,3,4,5,6,7,8,9 python3 source/tools_host/check_cp26_speed.py`.
All checks run from Release48 with the included compiler paths configured.
Keep the accepted generated AOT source and its regeneration guard.
