# Stage 6 live portrait-border replay probe — 2026-11-22 22:26 ET

The accepted R235 local cartridge was independently found at `/mnt/data/files/PSC_R235_FIXB_NEWGAME_RELEASE.gba` and verified by SHA256 `4caf23ae8b030c16a32658fdc95085330de6c6dd4a51498f0e2b9c17c7d89308`, exactly 33,554,432 bytes. It was not modified.

## Actual emulator execution (mGBA)
Loaded each of three real existing mGBA states into two different Stage6 experimental 32MiB cartridges:
- Original Genesis gray/white portrait border: SHA256 `4dade6c06e0d2cbc3d17cbbc20a5bfb475c4a71fa2132512132ad8a65dca9617`
- Optional muted silver highlight: SHA256 `e8d5418560cdd43fac46a6e5214b7adc691c17b8f4e3bc1eddb58741a77f57b0`

States: `academy_to_principal.state` frame 13142, `after_principal_basement_settled.state` frame 16445, `overnight_basement_search.state` frame 16520. Ran 56 more frames each, capturing 8 checkpoints at eight-frame intervals. All **24 framebuffer comparisons had exactly zero changed pixels**. Screenshots and machine-readable per-frame CRC32s were produced locally (`portrait_border_live_results.json`).

**Interpretation: These three state windows do not exercise the changed portrait border. This is a negative coverage result, NOT proof the border works or is invisible.** The prior field parity remains intact, but no verified dialogue portrait playback is established here.

## Blocker and next executable step
Obtain a genuine saved mGBA state with an active PSIV portrait dialogue visible, or drive a deterministic real input replay from a pre-dialogue state into that scene. Compare exact Genesis border against silver variant in that scene with framebuffer ROI and palette; verify actual ROM portrait tile records are being loaded. Then replay same scene with VBA-M/Delta-style core and test hardware. Do not promote a border variant based on static ROM pixel comparisons.

No public copyrighted assets, binaries, or save states committed. Accepted R235 and production main untouched.
