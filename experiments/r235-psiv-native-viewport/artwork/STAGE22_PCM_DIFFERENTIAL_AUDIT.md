# Stage 22 — deterministic PCM differential analysis (no ROM promotion)

**Date:** 2026-11-23. Read current Drive PSC_WORKER_DIRECTIVES revision `AHj4eMSAUZAcFOaQJ1Kh5Rr2LutyJSuDgn7zoQNkS7ax3_UTNfo6nTUKtIPlWi60R6KRkL3OI-IYmLIfrVvLZoMpjlz1T9bTeh1O3SdY33I`; accepted R235 remains immutable, R236 production separate. Re-verified real accepted R235 in local container: SHA256 `4caf23ae8b030c16a32658fdc95085330de6c6dd4a51498f0e2b9c17c7d89308`. R234 source evidence ZIP is present; full R86/R203/R209 source archives were not verified here. Desktop Commander Ryzen currently offline.

## New executed data analysis

Analyzed the four **actual existing mGBA stereo signed-int16 PCM files** captured on the same 180-frame cold-start field route, each 197,508 samples, 395,016 bytes. This run analyzed recorded bytes; **it did not run a new emulator replay**. No copyrighted PCM is committed.

| ROM | PCM SHA256 | Differing int16 samples vs Stage14 | First differing sample | Max absolute difference |
|---|---|---:|---:|---:|
| accepted R235 | `99a84e62926075df4b9f400c35788606005b4c6326097147fa6a9919c1b7a905` | 398 | 528 | 4800 |
| Stage14 | `1358da4fddabe7cb733cdd15d35dbe1bbb5fc0b9255b6aad4c5af41ca47e0d71` | 0 | none | 0 |
| Stage21B | `734c97b3a2501a9808b6e356593da31cb6f65bcafb5ee36e562b09e660520527` | 1626 | 102 | 4224 |
| Stage21C | `e3353d630482c23d81fae51cbb7656e90eebd6bc642ca75725cda5d7e4a40955` | 1910 | 528 | 4800 |

**New discriminating evidence:** Stage21C's first divergence from Stage14 is sample 528, identical to the first divergence of accepted R235, **but Stage21C vs accepted first diverges only at sample 1006**. Thus the first changed Stage21C audio sample is not necessarily introduced by the OAM tail-call; it can inherit a preexisting accepted-vs-Stage14 discrepancy. Stage21B vs Stage14 diverges much earlier at sample 102. Stage21C vs accepted differs in **1908** samples; Stage21B vs accepted differs in **1612** samples. Stage21B and C differ in **1820** samples from each other. Stage21B and C mismatch indices overlap the accepted-vs-Stage14 set in 206 and 200 samples respectively.

All four comparisons against Stage14 have differences exactly divisible by **192** PCM amplitude units, in matched stereo channel pairs. Stage21C has **955 left-channel and 955 right-channel** differing int16 samples and **641 separate contiguous mismatch runs**; the first differs at interleaved sample 528, approximately **0.48 emulated video frames** into capture. ±2 and ±4 sample alignment tests do not improve equality over zero lag (Stage21C matches Stage14 at **99.03295%** of original sample positions), arguing against a constant PCM offset. These are signatures consistent with mixer scheduling/quantized output differences, but are **not yet proof of IRQ timing or source cause**.

The fresh local evidence JSON SHA256 is `f9b5535f7bbd3cc82ffaf557854685014a7efd527a4796c6be1b060aa427e68d`. New public, asset-free [stage22_pcm_timing_diagnostic.py](stage22_pcm_timing_diagnostic.py) performs per-channel, first-divergence, run, amplitude and sample-lag analysis for subsequent captures.

## Engineering consequence

**Do not treat the shared first mismatch at sample 528 as evidence the new OAM hook caused that specific sample.** Compare the first *novel* Stage21C-vs-accepted divergence at sample **1006**, and Stage21B-vs-accepted at sample **102**, against ARM CPU/IRQ/audio-mixer execution state; obtain original PCM source callstacks or frame/cycle positions. Before a patch is acceptable, rerun mGBA and VBA-M Academy route from true cold-boot or validated compatible state, plus dialogue, menus, battles and save/load. Academy frame16571 still has 1319 hybrid-screen residual mismatches. Existing Stage21C cartridge eliminated frame16572 681-pixel sprite lag in prior genuine mGBA replay, but **no new cartridge was built, no new gameplay route was executed, and no QA acceptance occurred in Stage22**.

Preserved accepted R235, production R236, rollback, GitHub main, and copyrighted assets unchanged.
