# Build Release 48

Run commands from this `Release48/` directory. Python 3, Clang/LLD 18.1.3 and llvm-objcopy with ARM7TDMI support are the tested toolchain. devkitARM is an alternate path in the builder but was not tested in this release. System compiler packages are not bundled.

```sh
python3 source/build_release48.py
```

This compiles the active native module from supplied sources/assets, applies the guarded PSC startup cleanup to the pristine donor, and writes `binaries/RUN_THIS_RELEASE48_PSC.gba`. The older `source/build_r46.py` name is a compatibility entry. The default build uses accepted assets and skips regeneration.

For the current **PSII/PSIII AudioQ1** listening candidate, keep the default release untouched and build to a separate ROM:

```sh
python3 source/build_release48.py --native-audio-q1 --out binaries/RUN_THIS_RELEASE48_AUDIOQ1_PSC.gba
python3 source/tools/check_release48_native_audio_quality.py
```

AudioQ1 changes only the two retail native six-channel mixer loops: final signed-8 conversion uses signed round-to-nearest instead of arithmetic truncation. Wave banks, sequence tables, phase/envelope kernels, DMA timing and PSIV audio are byte-preserved. It is a low-risk fidelity/quantization pass, **not** the finished II/III instrument replacement.

The current build's module map/stack usage is included in `source/build-psc-module/`; object files, ELF and duplicate raw module are generated when building. Build tested with Clang/LLD 18.1.3; restored package rebuild must produce byte-identical `721e446d` ROM.

## Current checks

Host probes require GCC; emulator probes require Pillow plus zlib and the upstream mGBA bridge. The unmodified mGBA source archive is supplied for offline rebuilding; its own licenses are inside. To build without CMake:

```sh
python3 source/tools_host/r46_mgba/build_minimal.py
python3 source/tools/check_r48_departure_mgba.py
python3 source/tools/check_r48_launcher_mgba.py
python3 source/tools/check_r46_level99_mgba.py
python3 source/tools/inspect_r46_fifo_seams.py --label RELEASE48 --commits 180 --expect-continuity
python3 source/tools/check_r46_sfx_ownership_mgba.py --label RELEASE48
python3 source/tools/check_r46_mgba_field_audio.py --label RELEASE48
python3 source/tools/check_r31_story.py
python3 source/tools/check_r34_scenes.py
```

Run SFX ownership first to create the fresh debug state used by the field probe. Host scene checks include rendering/orchestration stubs; linked calls use controlled records. Historical tests requiring old checkpoints/captures are optional and may need omitted inputs. Do not use older map/state data for this ROM.

## PSII/PSIII audio Q1 candidate

The accepted Release 48 ROM remains the default. To build the current II/III resampling candidate from it:

```sh
python3 source/tools/build_psc_native_audio_interp.py
python3 source/build_release48_ps23_audio_q1.py
python3 source/tools/check_psc_native_audio_quality.py
```

This writes `binaries/RUN_THIS_RELEASE48_PS23_AUDIO_Q1.gba` (expected CRC32 `8897ec05`). To rebuild the accepted base first, add `--rebuild-base` to the candidate builder. After building the mGBA bridge, the scoped runtime/audio smoke test is:

```sh
python3 source/tools/check_r48_ps23_audio_mgba.py
```

Q1 is not yet the production default because its smoothing also reduces RMS level; it needs a user listening pass. The Dezo/Dezoris missing-instrument issue is separate and remains open.

## Experimental FM/Game Gear build

```sh
python3 source/psc8/adapt_cores.py
python3 source/psc8/tests/check_audio.py
python3 source/psc8/build.py
python3 source/psc8/tests/check_runtime.py
```

This generates a separate development module/test ROM in `source/psc8/build/`, not a replacement playable release. Read its README first. Full runtime diagnostics are expected to report slow guest execution and audio starvation. They must not be advertised as completed support.

To keep the archive small, inactive full-song PCM masters and packed hybrid song tracks, historical recording capture inputs, old disassembly listing/output ROMs and bundled compiler packages were omitted. They are not used by the default sequenced build. The `PS4_HYBRID_PCM_MUSIC=1` path and historical capture regeneration require those additional inputs. Active binary assets, original PSIV source ROM, source disassembly, prerecorded production clip banks and every requested rollback wave remain available.

### PSII/PSIII AudioW12 listening build

The accepted Release 48 output remains the default. To reproduce the current low-runtime-risk II/III audio listening candidate from the same preserved donor/module, use:

```bash
python3 source/build_release48.py --pack-only --native-audio-wave12 --out binaries/RUN_THIS_RELEASE48_PS23_AUDIO_W12.gba
python3 source/tools/check_release48_native_wave_smooth.py binaries/RUN_THIS_RELEASE48_PSC.gba binaries/RUN_THIS_RELEASE48_PS23_AUDIO_W12.gba --out build_artifacts/RELEASE48_PS23_AUDIO_W12_SCOPE.json
```

`--native-audio-wave12` is mutually exclusive with the older quantizer-only `--native-audio-q1`. It changes only audited PSII/PSIII wave bytes and the two native mixer quantizers; PSIV remains byte-identical.

### Preferred PSII/PSIII AudioW12R listening build

```bash
python3 source/build_release48.py --pack-only --native-audio-wave12r --out binaries/RUN_THIS_RELEASE48_PS23_AUDIO_W12R.gba
```

This is shell-only and reuses the accepted PSIV tail byte-for-byte. W12R adds no runtime DSP work; it preconditions only audited native PSII/PSIII wave bytes, preserves their level with bounded per-wave RMS retention, and applies the existing signed round-to-nearest mixer quantizer. Accepted Release 48 is not replaced unless this candidate is explicitly promoted after listening.

Static six-slot audit:

```bash
python3 source/tools/audit_release48_native_audio_channels.py binaries/RUN_THIS_RELEASE48_PSC.gba -o build_artifacts/RELEASE48_NATIVE_AUDIO_CHANNEL_AUDIT.json
```
