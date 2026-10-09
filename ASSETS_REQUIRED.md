# Missing inputs for local restoration and builds

The source-only archive does not contain all files needed to reproduce CP58 byte-for-byte. This is intentional for Git publication. In particular:

- `Release48/binaries/RUN_THIS_R48_CP07_SAFE_HANDOFF.gba` — base donor ROM referenced as the default by `Release48/source/build_cp58.py`.
- `Release48/source/gba/*.bin` — generated/derived assets needed by native build assembly `.incbin` directives (e.g. `ps4_reversed.bin`, `precomp_assets.bin`, field/audio banks).
- `Release48/additional_roms/` — SMS/GG and adventure content inputs.
- `Release48/source/additional/build/` and `native58/armcheck/` — generated link objects / test data; need regeneration or retrieval from original checkpoint.
- `SUPPORT/R48_LLVM18_1_3_MINIMAL_REBUILD_TOOLCHAIN.tar.gz` — bundled compiler, deliberately not versioned; obtain an ARM-capable LLVM 18 toolchain independently.
- `Release48/source/tools_host/r46_mgba/mgba-0.10.5.tar.gz` — external emulator archive; obtain upstream directly.

Use the **unchanged original checkpoint kept outside this repository** for local comparison or input restoration, but review rights before redistributing derived content. Use `python3 repo_tools/restore_from_local_checkpoint.py /path/to/R48_CP58_NATIVE_COMPLETE_CHECKPOINT_2026-10-09.zip` to copy excluded assets from your own preserved checkpoint into this working directory. Optional `--include-built` also restores generated objects, and `--include-toolchain` copies the compiler archive. This helper does not fetch content or perform a build. The restored assets are ignored by `.gitignore` and must never be force-added to Git. A clean rebuild remains unverified for this sanitized source tree.
