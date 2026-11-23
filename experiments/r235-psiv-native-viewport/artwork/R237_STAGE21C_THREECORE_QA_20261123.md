# R237 + Stage21C: actual three-core field and save-state QA (2026-11-23)

**Status: experimental, built and executed, NOT accepted as a full-game release.** All proprietary cartridges, screenshots, SRAM/state images and toolchain binaries are private. Git stores only source scripts and this QA ledger.

## Provenance and production isolation

- Accepted R235, 32 MiB: SHA256 `4caf23ae8b030c16a32658fdc95085330de6c6dd4a51498f0e2b9c17c7d89308`.
- User-supplied R237 IPS32 applied to exact R235: 3,298 changed bytes, exact match to supplied candidate SHA256 `a84e9c0501c16a2b750a85037cfa16bcd859101fb4b201ff4edaa1da6b2742de`, CRC32 `916fd618`.
- Stage14 source-parent: SHA256 `c1ed9db09505fe2d17322efa9f7c4915e7a4cd57bdf2a3dc6f24d0f8f1d15609`.
- Stage21C native-OAM experiment: SHA256 `ff5f96aae1a536ee13994040ff98bb74a2afbfe1f1a3065df6e996aa1cab5139`.
- New **R237 + Stage21C relocated binary**, SHA256 **`cc3b2abc0f12b3fdaa4df795994d79221ee2537bc576c8cc643276ad99d6957e`**, CRC32 `cbd20103`, **33,554,432 bytes**. The private output was NOT committed to public GitHub.
- Rebase builder: [build_stage21c_r237_relocated.py](build_stage21c_r237_relocated.py); smoke/field replay harness: [verify_r237_stage21c_threecore.py](verify_r237_stage21c_threecore.py).

## Real execution with the headless PSC mGBA and two VBA-M/Delta-style cores

The signed toolkit archive was staged from the owner's Library, the actual emulator cores were loaded, and all runs used normal GBA execution. Inputs were repeated **from cold boot independently for each cartridge and core**; no CPU-memory poke or frame skipping was used.

- Opening: boot PSC → start PSIV → skip title sequence → start a new game.
- **33 natural field checkpoints** from frame **1599 through 3135**, scripted A presses at real frame intervals. Scenes include opening dialogue, Piata, party/follower fields and movement across outdoor town/bridge areas, overworld, and entry toward Motavia Academy.
- Paired reference is the owner's actual Stage21C ROM, not an imagined pixel oracle. References and candidate are compared only *within the same emulator core*.
- Each checkpoint captures real framebuffer, map bytes `0x02024068..6B`, and party `0x0200F40A..0E`.

| Core | Same RGB framebuffers | Same map bytes | Same party bytes | Frames |
|---|---:|---:|---:|---:|
| mGBA | **33/33** | 33/33 | 33/33 | 1599–3135 |
| VBA-M libretro | **27/33** | 33/33 | 33/33 | 1599–3135 |
| Delta-style VBA-M libretro | **33/33** | 33/33 | 33/33 | 1599–3135 |

**Important unresolved visual difference:** VBA-M libretro shows a *repeatable* narrow animated-water strip discrepancy at frames **2559, 2607, 2655, 2703, 2751, 2943**; at frame 2559 **1,512 RGB pixels differ**, all in x=178..209, y=0..159. The exact mismatches reproduce on independent reruns (27/33 comparison again). They do not occur in the same route on mGBA or the Delta-style VBA-M core. Map and party bytes are unchanged. A water-animation phase/timing cause is plausible but **not proved**, so do not treat as harmless without code/timer evidence.

## Save-state fidelity spot check

At actual emulation frame 3135, each combined-ROM core serialized a state, ran a further 175-frame A/dialogue input replay, restored that state, then replayed the same inputs. The **RGB framebuffer CRC and map/party bytes match exactly after replay** for all three cores:

| Core | Restored replay framebuffer CRC32 | Map bytes | Party bytes |
|---|---|---|---|
| mGBA | `d1c33da6` | `0000158c` | `0100ffffff` |
| VBA-M | `30d169e2` | `0000158c` | `0100ffffff` |
| Delta-style VBA-M | `30d169e2` | `0000158c` | `0100ffffff` |

This is **emulator-state round-trip**, not the game's SRAM save menu / persistent-save check. VBA-M wrapper's host frame counter is not serialized; comparisons use resulting pixels and game state.

Private screenshot contact sheets and reproducible JSON are retained outside public GitHub. Their SHA256 values: mGBA contact `d8220354469461173ad1156958d9663bcb988c064a8d95a1a58c48d93302526c`; VBA-M contact `d96af9c2e2b08a80dded081f537f9adbb12e49b4f9aba12fae69b54934aed6cf`; Delta contact `a26a2eb008624d4f85c0f0d3f765243c121572812cd85efc92fc08e8c66e5a88`.

## Outstanding acceptance gates

- Resolve or explain **six VBA-M water-strip mismatches** with original frame timing.
- Replay **actual R237 fixes** on this combined cartridge: Krup kindergarten Saya event, Igglanova/Zema event-battle palette, title erase-last-save, PD3 stack-safe boss battle, credits→Fin screen.
- Repeat live Academy entrance, especially frame 16571 first-reveal strip, and original Genesis comparison.
- Recheck original Stage21C audio PCM divergence, stack/cycle budget, real SRAM operations, physical GBA / Micro and full natural ending route. **None of these are claimed validated here**.
- New relocated code region was all zero in three supplied cartridges but needs full route coverage and source allocation proof; no release acceptance.

The already source-ported five R237 fixes remain separate from viewport experimentation; accepted R235, main and production R236/R237 were unchanged.
