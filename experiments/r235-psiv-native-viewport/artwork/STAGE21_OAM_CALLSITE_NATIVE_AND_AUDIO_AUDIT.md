# Stage 21 — source-level OAM room-entry correction trials (experimental only)

**Date:** 2026-11-23. This is an actual live-emulator engineering run, not a release or full viewport acceptance. All original ROMs, extracted graphics, state files, audio PCM and full screenshots remain local/private and were **not published to GitHub**.

## Live control and staged provenance

Read the installed PSC Engineering Worker skill and live Drive worker directives doc ID `1-RLEVE-uPiguM0CCwJDqlSKMC_AHGqQ9Z-nHgsyuE-I`, revision `AHj4eMSAUZAcFOaQJ1Kh5Rr2LutyJSuDgn7zoQNkS7ax3_UTNfo6nTUKtIPlWi60R6KRkL3OI-IYmLIfrVvLZoMpjlz1T9bTeh1O3SdY33I`. The directives still identify accepted R235 and keep R236 production separate. This work uses only the existing experiment branch.

Actual cartridge and source files in the container were re-SHA-verified:
- Accepted production R235: 33,554,432 bytes, SHA256 `4caf23ae8b030c16a32658fdc95085330de6c6dd4a51498f0e2b9c17c7d89308`.
- Stage14 experimental parent: 33,554,432 bytes, SHA256 `c1ed9db09505fe2d17322efa9f7c4915e7a4cd57bdf2a3dc6f24d0f8f1d15609`.
- Original US Genesis PSIV input: 3,145,728 bytes, SHA256 `511f35cc11f88316f8b8940e28ab298bd75a4da193672a80172884d6eb913b6a`; no original Genesis scene equivalence test completed this run.
- Natural Academy after-principal map18 mGBA state: 397,312 bytes, SHA256 `3a18bcce1f725baaeac2114f20edda5aca4aa4e482464aba6f15ee21b0454ae3`.
- Native owner-supplied mGBA 0.10.5 bridge, VBA-M libretro and Delta-style VBA-M cores, extracted from `PSC_HEADLESS_EMULATOR_TOOLKIT_2026-11-16.zip`. The verified R234 free-space map permits the retired sound sample42 region `0x08F97D64..0x08F980FF`.

## Proven first producer and patch placement

The original native field OAM routine receives camera values in `r0`/`r1`. Its field-specific ARM wrapper is called at **ROM `0x08C7D4F4`** (return address `0x08C7D4F8`) and dispatches into `0x08C4D13C`, then the original IWRAM OAM writer `0x0300286C`. A genuine mGBA breakpoint confirms that call in BOTH the opening field and the Academy new-room route. The displaced camera offset before sprite publication was the true source of the 681-pixel Stage14 frame16572 divergence.

**Important state-cache finding:** The Stage21A first prototype patched the ROM backing of an 8-byte IWRAM writer entry at `0x09EEE91C`; cold boot loaded the modified IWRAM bytes and executed the injected helper at `0x08F97F60`, but loading the prior Academy savestate restored the old IWRAM writer bytes and bypassed the patch. This is why QA must test actual runtime memory/code origin, not merely cartridge file hashes. It is a concrete limitation of testing patched IWRAM overlays from inherited snapshots.

An explicitly labeled **RAM-only diagnostic** restored just those eight IWRAM instructions after loading the old state; this reduced Stage14 frame16572 mismatch from 681 to 0, and the first reveal from 3135 to 3114. That test was **NOT playable-source validation**.

## Stage21B / Stage21C: actual ROM call-site fixes, no memory poke

Moving the patch from the cached IWRAM region to the ROM callsite avoids the old-savestate problem. Both builds restore accepted R235's original actor-camera hook and instead add +16/+8 **exactly once** before the native field OAM wrapper. They preserve GBA header, full 32 MiB size, 1:1 pixels and all Stage14 enemy art.

- Stage21B: ROM stub with balanced call/return; SHA256 `830bb2b8f05d828dc793dc5a5d1a0e49d237ce6168cc5e6a66bb7b3de060f2cd`; **34 changed parent bytes**.
- Stage21C: shorter ARM7TDMI tail-call, user-rebuildable with [build_stage21c_oam_tailcall.py](build_stage21c_oam_tailcall.py), SHA256 `ff5f96aae1a536ee13994040ff98bb74a2afbfe1f1a3065df6e996aa1cab5139`, CRC32 `56666aa6`; **22 changed parent bytes**.
- Stage21C exact changed ROM regions: `0x08C7D4F4` (4 bytes, native OAM BL redirected), `0x08CD3854` (4 bytes, original actor hook restored), `0x08F97F90` (16 bytes, ARM7TDMI native tail-call in retired sample 42).
- Stage21C ARM tail-call source: `add r0,r0,#16; add r1,r1,#8; ldr pc,[pc,#-4]; .word 0x08C4D13C`. It preserves the caller's LR; no global rescale, no fast EWRAM workaround.

An earlier **incorrect literal BL** in a discarded Stage21B scratch binary jumped to the wrong PC and failed execution. That binary was REJECTED before test results were recorded. The subsequent verified Stage21B and C builds use correctly resolved ARM control flow and must not be confused with the discarded scratch output.

## Actual executed live-game regression and visual result

Stage21B and Stage21C each independently completed the original Academy map18→17 controller replay in mGBA from the authentic settled state, **without ROM change, CPU-memory poke, emulator-speed change or skipped input frames after state load**. Per-frame map IDs, player positions and whole OBJ graphics hashes matched accepted R235 and Stage14.

Native RGB 224×152 exact shifted-overlap comparison:

| Frame | Stage14 mismatched pixels | Stage21B | Stage21C |
|---|---:|---:|---:|
| 16571, first room reveal | 3135 | 3114 | **3114** |
| 16572, next published room frame | 681 | **0** | **0** |
| 16573–16603, settled | 0 | **0 each** | **0 each** |

Stage21B and Stage21C also each passed **45/45 genuine early-game startup/map94/field/menu checkpoint comparisons** against Stage14: 15/15 on mGBA, 15/15 on native VBA-M libretro, 15/15 on Delta-style VBA-M libretro. All compared framebuffer checksums, map IDs, player positions and frame counts agree **within the same emulator core**, not across dissimilar cores.

Private receipts: Stage21C full three-core opening report SHA256 `c98d294825b1633e9810e7f4c0fbbd645b5f0857f7a910efdb049bcf67607d90`; Stage21C Academy result SHA256 `bbf1d99c1d649bab8474c2e912c043b8324664d8576b4242533026ddf4ceee23`. These are source backed by actual emulator execution; neither is a full-game route or physical GBA Micro proof.

## NEW isolated 14-scanline first-reveal discrepancy

At frame16571, after correcting OAM in the diagnostic, **all 3114 residual changed RGB pixels are restricted to native overlap rows y=68..81**, a 14-scanline strip spanning x=0..223. Outside this strip the field crop is pixel exact. Original and experiment show a white/black transition wipe at the same absolute GBA display scanlines, so naive globally shifted whole-screen comparison is not valid for this screen-space effect. However a hybrid absolute-screen/shifted-field comparison still leaves **1319 changed pixels**: do NOT dismiss this whole strip as an expected wipe without source timing proof. The native BG raster/transition-scissor first producer remains a separate open issue.

## NEW audio/performance acceptance blocker

An authentic 180-frame mGBA field replay (RIGHT90, LEFT90 from the same cold-start map94 path) captured **395,016 bytes = 197,508 signed 16-bit samples** per cartridge. Same-ROM Stage14 re-execution was audio byte-identical, so the differences are deterministic:

| Against Stage14 PCM | Differing 16-bit samples | Percent |
|---|---:|---:|
| Stage21A cached-IWRAM ROM trampoline | 1978 | 1.0015% |
| Stage21B ROM wrapper | 1626 | 0.8233% |
| **Stage21C short tail-call** | **1910** | **0.9670%** |

The maximum absolute signed-sample difference for Stage21C is **4800**. Accepted R235 vs Stage14 already differs in 398 samples on this route, so the new trial has worsened parity relative to the current experimental parent. Timing/interrupt influence is suspected but **not yet source-proven**. Normal frame cadence and displayed positions still match. **Audio parity blocks promotion.** No assertion of original Genesis music fidelity, full save/load, all dialogue/menus/battles, or Micro clock budget is made.

## Next exact engineering gate

Trace the native OAM callsite's instruction-cycle/IRQ interaction with the audio mix under Stage14 vs Stage21C at first differing PCM sample, and separately trace the frame16571 BG scissor/wipe producer to classify those 1319 genuinely unmatched pixels. Seek a scene-independent low-cycle renderer change and test source-backed fixes on real mGBA **and** VBA-M Academy map transitions, other entrances, active portrait dialogues, battle sprites/HUD, save behavior, GBA Micro timing, and eventually a full original-path FIN playthrough. Do not bypass frames, scale the scene globally, crop Rykros auroras or overwrite original graphics.

**No promotion.** These are isolated experimental candidates with known audio discrepancies. Accepted R235, Stage14, production R236, rollback and GitHub main were not modified.