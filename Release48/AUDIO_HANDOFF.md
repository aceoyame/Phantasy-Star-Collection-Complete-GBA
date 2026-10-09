# Audio handoff — Release 48

## Preserve the playable sound

The user accepted IV instrumentation and transport. All 27 protected files are unchanged in content and its three complete wave/player profiles remain available. See `ROLLBACK.md`. The playable I/II/III banks also remain unchanged; the only donor-derived production changes are startup logo removal. Current audio component work is under `source/psc8/` and is not linked into the playable ROM.

## Recovered FM/GG chat

`source/references/psc8ext_cp03/` is deprecated reference source. Its PSG ports remain tied to PSC's native statically translated PSI functions: changing cartridge pointer literals does not execute arbitrary cartridge instructions. The FM and GG diagnostic ROMs never reached their intended port code. Hook calls to `machine_is_gg` also overwrite LR before unhandled returns; do not blindly reapply those patches. Preserve the accepted game's launch/save behavior.

## New development path

`source/psc8/` executes Z80 instructions using MIT superzazu/z80 and synthesizes YM2413 with MIT digital-sound-antiques/emu2413. Pristine vendor sources/licenses are included; adapted sources are produced by `adapt_cores.py`. The component test checks 16 instrument configurations across nine voices, register/key/rhythm/custom-patch changes and 64,000 integer box-filter sample outputs against unmodified upstream chip ticks. The small precomputed TLL table preserves all values (0..238) while using bytes instead of 32-bit words.

PSG uses exact fractional NTSC clock against GBA Timer0 /924, integrated samples, Sega 16-bit noise taps and correct 512/1024/2048 clocks per fixed noise shift. Period 0/1 produces the constant output used for PCM. FM control values 0/1/2/3 route PSG-only/FM-only/silence/both. This is a component improvement, not audible hardware certification.

The updated diagnostic runtime reaches FM and GG ports, and shows the 160×144 GG red viewport at (40,8). In 900 display frames it executes 101 GG guest frames and 22 FM guest frames, with hundreds of late/silent blocks. DMA recovery prevents reads beyond the buffer but cannot fix insufficient CPU speed. Real PSI shows stripes: its legacy display mode is incomplete. Do not integrate this runtime until performance, VDP compatibility and saves are resolved.

Outstanding generic engine limitations: legacy SMS VDP modes; full SMS viewport/scaling; mode4 28-row scroll wrapping, line scrolling, lock regions, zoom, sprite overflow/collision; more precise IRQ/region/timing and noise-divider synchronization; persistent cart SRAM; accurate anti-alias/reference resampling; commercial mapper/game validation. The small test cartridges are homebrew diagnostics, not the requested bonus games.

## Native II/III AudioQ1 candidate (2026-10-04)

A conservative playable A/B candidate now exists as `binaries/RUN_THIS_RELEASE48_AUDIOQ1_PSC.gba`. It leaves every native II/III wave, table, phase/envelope kernel and DMA parameter untouched and changes only the final six-channel signed-8 quantizer in each native mixer. The original used arithmetic `ASR #8`, which floors negative values; Q1 uses symmetric round-to-nearest before the same saturation. The two 136-byte IWRAM mixer blocks remain exactly the same size and addresses. Host proof shows zero error versus the intended nearest-integer quantizer in the tested range; mGBA execution smoke confirms both native mixers are reached after the patch. Listening/device validation is still required before promotion.

This is deliberately a first quality pass. The larger fidelity gain still requires source-faithful II/III instrument reconstruction and/or a carefully validated resampling upgrade; do not claim Q1 solves the Dezo missing-instrument defect.

## Native II/III path now located

`source/tools/audit_psc_native_audio.py` inventories 512 table entries. PSII has 104 normal positive-count wave rows. PSIII has 132 positive-count rows plus seven valid `count=-8` special-wave references (four unique physical intervals); only the `-1` sentinels are excluded. The `-8` handling was confirmed directly in PSIII's native note initializer: it takes `phase_cycle >> 14` as the waveform length and initializes the loop span rather than rejecting the row. `--export-waves DIRECTORY` exports decoded native mono PCM; these are existing PSC assets, not Genesis reference captures. Rate 21,024 in the exported files is nominal; GBA Timer0 reload FCE2 gives a different exact hardware rate (~21,024.08 Hz).

| Property | PSII | PSIII |
| --- | --- | --- |
| Native table file offset | `0x481510` | `0x152080` |
| Records / stride | 256 / 8 bytes | 256 / 60 bytes |
| Record fields | sample length, ROM pointer | repeat count, ROM pointer, phase-cycle, envelope records |
| Native ARM mixer address | `0x08395530` | `0x08083140` |
| Native ARM phase kernel | `0x083955B8` | `0x080831C8` |
| Log lookup file offset | `0x6A62B0` | `0x269AF0` |
| Audited logical wave bytes | 2,246,040 | 1,060,203 (includes valid `-8` special-wave references) |

Both mixers combine six channels through a signed exponential lookup, saturating to signed-8 output. Wave bytes are logarithmic, not ordinary signed-linear PCM. Attenuation reduces signed magnitude before lookup; phase is Q14 and sampling uses the integer position. II's voice record begins pointer/length/step/phase/attenuation/ramp/loop; III adds envelope state/counters and derives its length from `(phase_cycle >> 14) * repeat_count`. Do not treat their distinct tables as interchangeable. The PSG/DAC special rows still need source-specific auditing.



## Native II/III AudioW12R — preferred listening candidate (2026-10-04)

`binaries/RUN_THIS_RELEASE48_PS23_AUDIO_W12R.gba` (CRC32 `f310f1f6`) supersedes W12 as the preferred **listening** candidate, while leaving accepted Release 48 untouched. It uses the same zero-runtime 12.5% linear-amplitude forward preblend but applies bounded per-wave RMS retention (maximum 1.08x) before nearest signed-log re-encoding. The mixer still uses signed round-to-nearest; tables, pointers, lengths, Q14 phase/pitch, envelopes, loops, phase kernels, DMA/sample timing, ROM size and the appended PSIV region remain unchanged.

In the same 600-frame mGBA windows, II preserves level essentially exactly (`1489.86 -> 1489.87` RMS) while delta RMS falls `145.98 -> 140.51`. III retains much more level than W12 (`1160.91 -> 1144.14` RMS) while delta RMS falls `656.07 -> 614.98`. Peaks remain far below the probe's near-clipping threshold. Structural scope is recorded in `build_artifacts/RELEASE48_PS23_AUDIO_W12R_SCOPE.json`, and runtime metrics in `RELEASE48_PS23_AUDIO_W12R_MGBA.json`. Device/listening judgment is still required before promotion.

`build_artifacts/RELEASE48_NATIVE_AUDIO_CHANNEL_AUDIT.json` also confirms from the retail PSC code that both retained native players are hard-limited to six concurrent scheduled/mixed slots: the mixer sum loops count 5..0, and the scheduler/initializer loops run indices 0..5. This does not identify the exact original source part that is absent, but it moves the next fidelity task away from stronger filtering and toward source sequence/channel mapping.

## Native II/III AudioW12 — conservative fallback listening candidate (2026-10-04)

`binaries/RUN_THIS_RELEASE48_PS23_AUDIO_W12.gba` (CRC32 `77a66742`) is now the preferred **listening** candidate for the current II/III quality pass. It does **not** install the per-sample interpolation helper used by the earlier PS23 AudioQ1/Q2/Q3 experiments. Instead, it preconditions each audited native waveform offline, including PSIII's valid `count=-8` special waveform records: every sample except the table-defined last byte is moved 12.5% toward the next sample in **linear amplitude**, then re-encoded to PSC's nearest signed-log code. The tiny final mixer change uses signed round-to-nearest. Wave pointers, lengths, Q14 pitch/phase, envelopes, loop metadata, phase kernels, DMA timing, ROM size and the entire appended PSIV region remain unchanged. This adds no runtime audio work and therefore avoids competing with the games for ARM7 time.

Structural guard `source/tools/check_release48_native_wave_smooth.py` reports zero changes outside the audited PSII/PSIII wave intervals and two 136-byte mixer blocks; PSIV is byte-identical. The 600-frame mGBA capture also advances both games normally with nonzero PCM. Relative to accepted Release 48, II keeps essentially the same level (`1489.86 -> 1486.59` RMS) while sample-to-sample delta RMS falls `145.98 -> 137.98`. III is more affected (`1160.91 -> 1096.56` RMS, delta RMS `656.07 -> 571.99`), so device/listening judgment still matters; the point of the 12.5% blend is to avoid the much stronger level/timbre loss seen in the 25%/50% and full runtime-interpolation tests. No near-clipping samples appeared in the captured windows. See `build_artifacts/RELEASE48_PS23_AUDIO_W12_MGBA.json` and `RELEASE48_PS23_AUDIO_W12_SCOPE.json`.

This is an anti-edge/quantization cleanup, **not** a reconstruction of missing Genesis channels or instruments. Public reports of the retail collection specifically describe PSII/III as mono/muffled and PSIII tracks as missing a channel, so the next fidelity step must audit the source track/channel conversion rather than blindly filtering harder. The PSII Dezo/Dezoris missing-instrument defect remains separately open.

The stronger zero-runtime `AudioW25`/`AudioW50` comparisons and the runtime interpolating Q1/Q2/Q3 builds are retained only as engineering references. W25/W50 smooth more strongly but attenuate III more; Q1/Q2/Q3 add per-sample helper work. Do not promote those over W12 without a listening reason.

## PSII/PSIII audio-quality Q1 candidate (2026-10-04)

A first non-destructive quality pass is now implemented as `binaries/RUN_THIS_RELEASE48_PS23_AUDIO_Q1.gba` (CRC32 `8897ec05`). The accepted Release 48 ROM remains unchanged at CRC32 `721e446d`; Q1 is deliberately a listening candidate, not a silent promotion of the production ROM.

Q1 replaces only the four nearest-neighbor **sample fetch/attenuation** spans in the native II/III phase kernels. PSC copies those ARM kernels to IWRAM at runtime, so ordinary ROM-relative `BL` hooks are invalid. The installed 24-byte stubs preserve the original leaf-function LR, derive the relocated local return address from PC, jump through an absolute ROM helper literal, restore LR, then branch locally to the original `STRB`. The helper itself is 240 bytes and shares a 32,769-byte inverse magnitude table. Total reserved PSIII-tail interval is 33,153 bytes at `0x2D3B80..0x2DBD01`.

The helper preserves each engine's stock Q14 phase, attenuation ramp, loop/end policy, six-voice scratch buffers, final signed-exponential mixer, DMA and sample rate. It attenuates both adjacent logarithmic samples exactly as stock, decodes them through the live PSC LUT at `0x03002000`, interpolates **linear amplitude**, then re-encodes to the nearest normal log magnitude. It does not transplant PSIV waves or invent new instruments. Sources: `source/gba/psc_native_audio_interp.s`, `source/tools/psc_native_audio_quality.py`, and `source/build_release48_ps23_audio_q1.py`.

The scoped mGBA probe launches both native games and advances 600 display frames while capturing nonzero PCM. Against the same title-screen windows, sample-to-sample delta RMS fell from about `145.98` to `101.29` in II and `656.07` to `502.46` in III, while RMS level also fell (`1489.86→1098.29` and `1160.91→916.90`). That is consistent with less nearest-neighbor edge energy, but it also means Q1 is quieter; **listening on emulator/device is required before making it the default**. `build_artifacts/RELEASE48_PS23_AUDIO_Q1_MGBA.json` records the run.

The PSII Dezo/Dezoris missing-instrument report is a separate sequence/instrument-content defect and is not claimed fixed by Q1. Original Genesis II/III ROMs or other user-supplied authoritative source assets are still needed for exact instrument/envelope/loop reconstruction.

Next: obtain original Genesis II/III ROMs, map instrument/envelope/loop/sequence correspondence, render with a faithful YM2612 source core, transcode and compare level/loop/phase behavior. A resampling improvement must interpolate **linear amplitude**, not raw logarithmic bytes, and must preserve buffer layout, envelope transitions, loop/end boundaries, budget and DMA continuity. A blanket IV-wave transplant is invalid. The two native mixers are independent of IV's P134R code.

No FM-capable PSI, commercial GG or Genesis II/III/text-adventure source ROM was found in the recovered project. Ask the user for those inputs when the implementation is ready for them. Do not download replacements from ROM sites or call empty menu entries “support.”
