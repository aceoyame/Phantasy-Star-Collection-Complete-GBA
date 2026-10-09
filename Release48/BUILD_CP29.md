# Build and verify CP29

Extract the complete ZIP, then run from its root:

```sh
bash tools/build_cp29.sh /absolute/path/to/Release48
```

The wrapper restores the bundled offline LLVM18 Linux x86-64 toolchain into a
fresh temporary directory when needed, links `Release48/compiler`, sets compiler
paths, and runs `Release48/source/build_cp29.py`. It verifies a 33,554,432-byte
ROM with CRC32 `46C024A5`. Python 3, tar and Linux x86-64 are required. Keep the
accepted generated AOT source; raw regeneration is not part of this rebuild.

Host checks need GCC. GBA checks use the provided unmodified mGBA 0.10.5 source,
GCC/zlib and Python Pillow. Build their runtime and bridge:

```sh
cd Release48
python3 source/tools_host/r46_mgba/build_minimal.py --out /absolute/path/mgba-runtime
python3 source/tools_host/cp08_mgba/build_probe2.py --runtime-dir /absolute/path/mgba-runtime
```

The bridge builder writes `libprobe2.so` beside its Python module. Set PATH to
`Release48/compiler/root/usr/lib/llvm-18/bin` and LD_LIBRARY_PATH to
`Release48/compiler/root/usr/lib/x86_64-linux-gnu`, as in the build wrapper.
Run the current checks from Release48:

```sh
python3 source/tools/verify_cp27_text_indexes.py
python3 source/tools_host/check_cp29_fixed_cpu.py
python3 source/tools_host/check_cp29_row_boundaries.py
python3 source/tools_host/check_cp29_shifted_boundaries.py
python3 source/tools_host/check_cp29_fusion.py
python3 source/tools_host/check_cp29_preservation.py
python3 source/tools_host/check_cp29_gba_busy.py
python3 source/tools_host/check_cp29_gba_pixels.py
python3 source/tools_host/check_cp29_gba_pcm.py
python3 source/tools_host/check_cp29_saves.py
python3 source/tools_host/check_cp29_psc8_saves.py
CP29_SPEED_GAMES=2,3,4,5,6,7,8,9 python3 source/tools_host/check_cp29_speed.py
python3 source/additional/check_cp09.py --rom binaries/RUN_THIS_R48_CP29_NATIVE_ROWS.gba --base binaries/RUN_THIS_R48_CP07_SAFE_HANDOFF.gba --report build_artifacts/CP29_BUILD_REPORT.json --out ../validation_cp29/static --static-only
```

The CP28 baseline ROM is in Release48/binaries and its ELF/source snapshot is
in cp28_base. Sealed CP21 CPU references remain in source/references/cp21_final.
Evidence lives in validation_cp29; earlier validation is retained. For timings,
reuse the measured CP28 baseline if desired:

```sh
CP29_SPEED_BASE=../validation_cp28/FIXED_GUEST_SPEED.json CP29_SPEED_GAMES=2,3,4,5,6,7,8,9 python3 source/tools_host/check_cp29_speed.py
```

From the ZIP root, independently verify a clean complete rebuild:

```sh
python3 tools/check_cp29_archive_rebuild.py /absolute/path/R48_CP29_NATIVE_TEXT_SPEED_2026-10-08.zip --out /absolute/path/rebuild-check
```

Host executables and compiler caches are not included; they are rebuilt locally.
MD hot RAM is almost full. Preserve the existing linker boundaries and native
fusion fingerprints when modifying or regenerating source.
