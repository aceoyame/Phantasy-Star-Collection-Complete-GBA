# Upstream mGBA diagnostics

This test-only bridge uses upstream mGBA 0.10.5. The original release source
archive is included, with its upstream licenses inside. Source URL:
https://codeload.github.com/mgba-emu/mgba/tar.gz/refs/tags/0.10.5

Run `python3 source/tools_host/r46_mgba/build.py` from R46. Requirements:
Python 3.12+, GCC, Make, CMake, and system development libraries selected by
mGBA's CMake configuration (the tested host includes zlib and libpng). Pillow
is needed by the Python screenshot wrapper. Supply `--cmake /path/to/cmake`
if CMake is outside PATH. Build output goes into a new temporary directory;
keep it while using libprobe.so. The checkpoint excludes generated shared
libraries and object files, so rebuild on a restored checkout.

Release 05 also includes `build_minimal.py`, an offline GCC-only fallback for
hosts without CMake/libpng development packages. Run
`python3 source/tools_host/r46_mgba/build_minimal.py`. It builds the bundled
unmodified mGBA 0.10.5 GBA core with debugger, pthread and zlib support, without
frontends, GL or PNG support; the Python wrapper writes screenshots via Pillow.
Keep its printed temporary runtime directory while using the generated bridge.

The bridge boots the unmodified full PSC ROM at its native module entry,
uses software rendering and mGBA's default emulated BIOS, disables idle
optimization, and supplies explicit frontend volume. It provides hardware
breakpoints, CPU/memory access, state files, frame/cycle counters, optional
instruction cycle histograms, screenshots and native stereo S16 PCM.
CPU state, register writes and controlled function calls are test fixtures.
The native PCM callback follows SOUNDBIAS (32768 Hz in the tested ROM).

Commands from R46 after rebuilding its ROM:

```
python3 source/tools/profile_r46_mgba.py --label CP17
python3 source/tools/profile_r46_mgba_hot.py --label CP17
python3 source/tools/check_r46_mgba_rows.py --label CP17
python3 source/tools/check_r46_mgba_pd_arm.py --label CP17
python3 source/tools/check_r46_mgba_field_audio.py --label CP17
python3 source/tools/check_r46_mgba_pd_return.py --label CP17
python3 source/tools/check_r46_zio_emulator.py --core mgba --label CP17
```

Always run the profiler fresh after a ROM build: an older saved state contains
the older IWRAM program. `--reuse` is only for the exact same ROM build.
PD timing fixtures use actual initialized actor records and call the native
controllers. Field checks use controlled native map warps with default flags.
The audio fault test suppresses VCount production for eight display frames
while Timer1 consumption runs, then checks uninterrupted recovery.

These checks provide emulated timing and transport evidence. They do not
certify a normal-save playthrough, physical GBA behavior, original Genesis VDP
parity or audible quality. The pre-volume-fix CP15 PCM captures were silent
test-harness output and were removed; their CPU timing remains valid.

## Release 06 FIFO continuity and recovery

`probe.c` adds `mg_fifo_state`, a test-only view of the unmodified upstream core's queued words, partly consumed sample word and live DMA pointer. Rebuild the bridge, then run from R46:

```sh
python3 source/tools/inspect_r46_fifo_seams.py --label RELEASE06 --commits 3600 --expect-continuity
```

It compares actual queued bytes against the intended next block before and after each publication. The field recovery fixture now skips eight calls at the native producer entry while keeping the real Timer1 consumer and DMA running; VCount-disable fault injection is historical only. All current states must match the current ROM CRC. These tests do not certify physical audio output.
