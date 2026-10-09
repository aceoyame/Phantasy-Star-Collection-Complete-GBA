# Phantasy Star Collection GBA — Release 48 / CP58

**Status:** experimental source and research archive; not a finished retail-quality release.

Source checkpoint: **R48 CP58**, 2026-10-09. Builds on CP57. Read [CP58_HANDOFF.md](CP58_HANDOFF.md) for implemented ARM/Thumb native kernels, audited changes, measurements and pending work; read [Release48/BUILD.md](Release48/BUILD.md) for the Release 48 build layout.

## Layout
- `Release48/source/` — current PSC integration, native gameplay/audio, additional-content launcher and build/test tools.
- `native58/` — CP58 native text/decoder/kernel performance research, implementations, selected source comparisons and tests.
- `cp57_base/` — previous CPU/audio/render source baseline for differential investigations.
- `Release48/` — project build documentation, performance notes, and release/issue tracking.

## Rebuilding
This is a **Git-safe, source-first import**, not the original all-inclusive checkpoint. It intentionally excludes copyrighted game ROMs, ROM-extracted assets, prebuilt game images, compiled objects, compiler packages, and test captures. Source file paths have been preserved where possible. The original CP58 all-inclusive checkpoint is needed for a byte-identical rebuild until a clean asset-import/rebuild workflow has been implemented.

Expected build entry point in the original complete checkout: `python3 Release48/source/build_cp58.py --out output.gba` with the requisite legal ROM/input assets and LLVM 18 ARM toolchain already provisioned. The default build script references a CP07 base ROM that is **not** in this repository. Do **not** expect an out-of-the-box compile from this sanitized source tree. See [ASSETS_REQUIRED.md](ASSETS_REQUIRED.md).

The documented best measured minimum is 42.5848% of full speed across 24 windows (CP58), not full speed; see handoff for context.

## Publication and intellectual property
This is a public source/research archive containing reverse-engineering and integration work related to third-party games; **public availability does not confer any third-party redistribution rights**. No license is granted for Sega/Nintendo/third-party code or assets. Original commercial game files must be supplied separately by the operator, subject to applicable rights. Do not push ROMs, extracted assets or toolchains into Git history.

See [CLEANUP_REPORT.md](CLEANUP_REPORT.md) for the exact import decisions and removed-file list.
