# Build and verify CP30

Extract the entire ZIP, then run:

```sh
bash tools/build_cp30.sh /absolute/path/to/Release48
```

The wrapper restores bundled LLVM18 into fresh /tmp storage, sets compiler paths and runs source/build_cp30.py. Expected ROM: 33,554,432 bytes, CRC32 CF8D26AF. Python 3, tar and Linux x86-64 are required. Keep accepted AOT source for normal rebuilding.

To reproduce generated AOT code and both fusions, from Release48:

```sh
python3 source/tools/extend_cp30_aot.py
python3 source/tools/fuse_cp30_nametable.py
```

Host checks require GCC. Build mGBA and its bridge from supplied source:

```sh
python3 source/tools_host/r46_mgba/build_minimal.py --out /absolute/path/mgba-runtime
python3 source/tools_host/cp08_mgba/build_probe2.py --runtime-dir /absolute/path/mgba-runtime
```

Use the compiler environment from the build wrapper, then from Release48:

```sh
python3 source/tools_host/check_cp30_fixed_cpu.py
python3 source/tools_host/check_cp30_nametable_boundaries.py
python3 source/tools_host/check_cp30_tile_pack_boundaries.py
python3 source/tools_host/check_cp30_transcript.py
python3 source/tools_host/check_cp30_gba_busy.py
python3 source/tools_host/check_cp30_gba_pixels.py
python3 source/tools_host/check_cp30_gba_pcm.py
python3 source/tools_host/check_cp30_saves.py
python3 source/tools_host/check_cp30_psc8_saves.py
python3 source/tools_host/check_cp30_preservation.py
CP30_SPEED_BASE=../validation_cp29/FIXED_GUEST_SPEED.json CP30_SPEED_GAMES=2,3,4,5,6,7,8,9 python3 source/tools_host/check_cp30_speed.py
python3 source/additional/check_cp09.py --rom binaries/RUN_THIS_R48_CP30_NATIVE_CALLS.gba --base binaries/RUN_THIS_R48_CP07_SAFE_HANDOFF.gba --report build_artifacts/CP30_BUILD_REPORT.json --out ../validation_cp30/static --static-only
```

Clean archive verification, from ZIP root:

```sh
python3 tools/check_cp30_archive_rebuild.py /absolute/path/R48_CP30_NATIVE_ENGINE_SPEED_2026-10-08.zip --out /absolute/path/rebuild-check
```

The sealed CP29 source/ELF is cp29_base. Transcript checks report inherited reference stack-state differences explicitly, while requiring full CP29/CP30 frame memory equality and exact independent-reference hardware events. merge_cp30_calls.py and check_cp30_ram_stubs.py are experimental/rejected work; neither is part of the accepted build.
