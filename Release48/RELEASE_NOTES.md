# CP10 (2026-10-07)\nAdventure saves fixed (SRAM 0x7000+slot*128, magic MDEE at 0x7F00); docs for speed and PS2/PS3 audio. ROM CRC32 e31bccaa. See CP10_* docs.\n\n---\n\n# CP09 — October 7, 2026 (test build, not a release)

- Genesis adventures: music and effects (own Z80 driver, translated; wavetable YM2612/PSG model), EEPROM saves, debug UI removed.
- Phantasy Star Gaiden: persistent cartridge SRAM.
- PSC8 / MD runtime speed work (see CP09_VALIDATION_REPORT.md). ROM CRC32 `34f14438`.

# Release 48 — October 4, 2026

The user requested this release name directly. It resumes the accepted R46 Release10 code line; no Release47 artifact is implied.

## Playable release changes

- Skip the four publisher/developer startup logos and start at the PSC title. Preserve normal initialization, title, music, four cards and save patches. Reclaim their 23,356-byte payload range; the pristine donor remains available for rollback.
- Fix a living party member's scripted departure leaving nobody alive. In the Demi-after-Zio case, Chaz now receives full maximum HP and cleared status; other members and TP remain unchanged. The common removal path covers Gryz, Demi, Raja, Kyra and Seth, with guards also on Rune/Gryz and selectable fifth-member replacements. A healthy survivor is never healed and an already-defeated party is never rescued by removing a dead member. No new persistent RAM is allocated.
- Verify the native level-99 routine already avoids the original shared-table-pointer corruption: 11 complete capped records remain unchanged and 100 other-character 98→99 transitions pass. This closes the reported corruption for this native routine; normal-save late-level progression still needs playtesting.
- Preserve all 27 protected PSIV audio source/assets, all three wave/player profiles and previous audio transport rollback. Existing combat, DF2 palette, names, story debug warps, lossless loading and PD menu changes remain in place.

## Audio development delivered

The recovered CP03 port hooks fail cartridge runtime testing. They cannot make PSC's statically translated PSI engine execute another ROM. Their old “FM/GG support” claim is superseded by these runtime findings.

`source/psc8/` now contains an independent Z80 cartridge engine and upstream YM2413 core, plus Game Gear RGB444 colors, centered 160×144 viewport, Start and stereo ports. The audio component corrects PSG pitch timing, the noise divider, latch/data writes, constant-output PCM tone periods and the four FM/PSG mute states; sample integration reduces point-sampling aliasing. Compact immutable OPLL tables save 98,304 ROM bytes and retain bit-exact chip/box-filter results for 64,000 test samples.

This engine is **not production-ready**: approximately 6.7 GG guest frames/sec and 1.5 FM frames/sec in the supplied diagnostics, with extensive audio starvation. The 90,060-byte module is not allocated in the playable ROM. Complete FM PSI and commercial GG validation are pending. It uses an integer box filter rather than upstream sinc conversion; bit-exact chip tests do not certify analog output or complete emulator accuracy.

PSII/III's existing audio has been mapped for the next implementation: six voices, logarithmic samples, Q14 nearest-sample playback, distinct 8-byte/60-byte tables, mixer/kernel locations and 512 native table entries. The inventory decodes 236 positive-count records and labels unused/special encoded rows separately. **Their playable wave banks have not been replaced.** Accurate Genesis-derived replacements require the corresponding source ROMs; copying IV's PCM bytes into these logarithmic banks would corrupt the instruments.

## Verification

- Four real PSC card selections and native game handoffs; PSC title visible at display frame 60.
- Ten linked production departure fixtures plus the real Zio continuation in an ASan/UBSan host scene test (display/orchestration stubbed).
- Eleven capped level-99 records and 100 isolated 98→99 cross-character cases.
- 180 real FIFO publications, zero queue mismatches; 402 linked SFX ownership cases and 624,096 exact PCM samples.
- 34 stationary/movement cases across 17 native fields, zero natural audio underruns. Eight intentionally skipped producer calls cause starvation, then audio recovers for 300 frames without new underruns.
- Main native games/save regions byte-identical to Release10; PSC differences confined to the startup NOP and reclaimed logo assets. Current ROM CRC32 `721e446d`.

These are scoped emulator/component checks. A full ordinary-save finish, listening review and physical hardware playthrough remain open. See `REMAINING_ISSUES.md`; this release does not claim that all issues or requested bonus features are finished.

## Package cleanup

One playable ROM, one pristine donor, active build source/assets, compressed rollback waves, current evidence and concise handoff. Earlier playable ROMs/checkpoint trees, bulk PCM/video captures, generated build objects, compiler installers and inactive full-song PCM masters were removed from the release archive. Some dormant historical generators require omitted inputs; the normal build and current checks are self-contained. `BUILD.md` describes the supported path.
