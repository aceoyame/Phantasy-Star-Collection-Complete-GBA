# Build and verify CP24

Extract the complete ZIP, then run from its root:

```sh
bash tools/build_cp24.sh /absolute/path/to/Release48
```

The script extracts the included LLVM18 Linux x86-64 toolchain when needed,
sets compiler paths, calls `Release48/source/build_cp24.py`, and checks the
33,554,432-byte ROM and CRC32 `F35DA10F`. Python 3 and standard tar are needed.
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
python3 source/tools_host/check_cp24_fixed_cpu.py
python3 source/tools_host/check_cp24_bytes_boundaries.py
python3 source/tools_host/check_cp24_render.py
python3 source/tools_host/check_cp24_gba_busy.py
python3 source/tools_host/check_cp24_gba_pcm.py
python3 source/tools_host/check_cp24_saves.py
python3 source/tools_host/check_cp24_psc8_saves.py
CP24_SPEED_GAMES=2,3,4,5,6,7,8,9 python3 source/tools_host/check_cp24_speed.py
python3 source/additional/check_cp09.py --rom binaries/RUN_THIS_R48_CP24_ADVENTURE_SPEED.gba --base binaries/RUN_THIS_R48_CP07_SAFE_HANDOFF.gba --report build_artifacts/CP24_BUILD_REPORT.json --out ../validation_cp24/collection --static-only
```

The merged baseline ROM and ELF/source snapshot are included for speed
reproduction. Sealed CP21 CPU references and our CP22 ARM/Thumb reference ELF
and ROM are included for differential checks. Evidence lives beside Release48
in validation_cp24. Historical CP22 evidence is retained in validation_cp22.
The supplied bridge is rebuilt locally; native host binaries are not included.

`additional/aot/native_spans.py` is imported by the generator and preserves
the new byte-loop fusion when generating into a fresh directory. The existing
generator guard still protects earlier hand-fused VRAM code. Do not replace
the accepted generated file with a raw coverage regeneration without merging
and testing those earlier fusions and duplicate-function sharing.
