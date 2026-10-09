# Additional Content checkpoint update

The requested label is **Additional Content**. Its fifth launcher card and ten-game submenu are implemented in the preview ROM. All ROM inputs, including the newly supplied small-font Gaiden, are included and byte-checked. Original I/II/III and active PSIV payloads are preserved. FM/GG remain slow; Genesis adventures have a real CPU/VDP launch path but are slow and still lack audio and persistent saves. None has full playthrough/hardware certification. See `ADDITIONAL_CONTENT_START_HERE.md`.

---

The baseline issue list below is retained as history; requests for ROM inputs and a missing submenu are now resolved. Its runtime/performance/save and PSIV issues remain open.

# Remaining work — Release 48

## User priority

1. Finish **PSI FM**, **Game Gear support**, then **PSII/III sound improvements**. Preserve accepted IV sound and rollback waves.
2. Put extra games behind one **Bonus Content** entry. Text adventures are tertiary; user estimates roughly 1.7 MB for the set. Obtain exact ROM sizes before reserving space.
3. Continue other gameplay/progression/interface/save issues. Further PD action optimization comes last; the existing menu workaround stays.

## Expansion work — four open categories

| Category | Current result | Next work |
| --- | --- | --- |
| PSI FM | Real YM2413 component and generic CPU path; CP03 injection rejected by runtime tests | Supply FM-capable PSI ROM; fix generic SMS legacy display, region/mapper/timing; improve speed and uninterrupted sound. Consider a fast licensed ARM CPU or PSI-specific native FM integration. Do not ship 1.5-fps diagnostic output. |
| Game Gear + Bonus Content | Generic CPU, GG viewport/colors/Start/stereo diagnostics work | Obtain Gaiden/Adventure ROMs; finish VDP scroll/collision/overflow/zoom, interrupts, region, persistent save, cartridge tests and speed. Add submenu only when games launch correctly. |
| Native PSII/III sound | Tables/encoding/mixers located. **AudioW12R is now the preferred low-risk listening candidate**: a 12.5% offline linear-amplitude wave preblend, bounded per-wave RMS retention, plus signed round-to-nearest, with **zero added runtime CPU cost**. Both titles advance 600 mGBA frames with nonzero PCM; PSIV is byte-identical. A new static audit also proves the retained PSII and PSIII players each schedule/mix only six concurrent slots. | User-listen AudioW12R before promotion; keep AudioW12 as the more conservative no-renormalization fallback and the heavier runtime-interpolation builds experimental. Then match original Genesis instruments/envelopes/loops/channels when authoritative source assets are available. Separately restore PSII Dezo/Dezoris map missing instrument and audit SFX/DAC/PSG/base-game audio defects. |
| Genesis text adventures | Requested tertiary content; no ROMs supplied/merged | Obtain exact set and sizes; allocate explicit ROM regions and implement real execution/display/saves. Do not label placeholder launchers as support. |

## Newly logged user-reported fixes (2026-10-04)

These are now explicit release blockers/follow-ups. The already-resolved **level-99 stat corruption** is intentionally not reopened.

| Game | Issue | Required result |
| --- | --- | --- |
| PSI | Retail PSC save bug | Reproduce the reported save defect, fix the verified failure mode, and retain existing save compatibility. |
| PSI | Retail PSC **invincibility** bug | Reproduce and fix the known false-invincibility behavior without changing intended defense/damage rules. |
| PSII | Dezo/Dezoris world-map music is missing an instrument | Restore the missing part and verify the map theme across entry/exit/reload. |
| PSII | Remaining base/original-game bugs requested for cleanup | Audit known original PSII defects and fix only verified bugs without changing intended mechanics. |
| PSIII | Remaining base/original-game bugs requested for cleanup | Audit and repair verified original PSIII defects while preserving intended generation/ending behavior. |
| PSIV | Nazan/Megid overwrite bug | Verify the NTSC-U-derived source path does not allow learning Megid to overwrite Nazan; fix if the current port reproduces it. |
| PSIV | Horizontal distortion/seam line while walking after tile-space reduction work | Remove the visible walking-time distortion line while keeping the reduced-space tile representation. |
| PSIV | Responsive battle-menu workaround suppresses Profound Darkness animation | Keep the responsive command menu **and** restore PD's normal animation/update cadence. Do not revert to the old sluggish menu. |
| PSIV | Battle attack animations are substantially too slow | Restore source-like attack/effect timing without reintroducing audio starvation or menu latency. |
| PSIV | Corruption visible along the bottom of the battle screen | Eliminate the lower-screen corruption shown in the 2026-10-04 PD playtest; check VRAM/row bounds and battle-layer ownership. |
| PSIV | Profound Darkness battle names expose form numbers | Battle UI must display **Profound Darkness** for all forms. The debug battle selector should continue to distinguish **Profound Darkness 1/2/3**. |

## Core PSIV work — eight continuing categories

| Category | Retained fixes / remaining verification |
| --- | --- |
| Combat | Illusion blink/clones/motion, Telele/Shadow retained. Other ability/item/enemy/weapon/combo/vehicle controllers, clocks, poses, windows, cues, overlap, equipment/target/escape/rewards still need source and ordinary-battle review. |
| DF2 black rendering (**not PD2**) | Stable body palette fixed. Original opening/fade, attack/death and ordinary story encounter remain unverified. |
| Blank enemy names, including PD | Seven placeholders made readable; historical 153 native captions pass. Check actual boss targeting/banner contexts and record enemy ID/action if a blank remains. |
| DEBUG story locations | Historical 150/160 destinations warp before dispatch. Full prerequisites/scenes/flags/party/inventory and multi-map continuations still need testing. Dynamic events `09,0A,0B,10,21,23` remain unresolved; `00,01,02,05` have no authored event. |
| Quests/progression | Eight Guild routes through inventory/money/flags/battles/save; Anger Tower/Megid; Silver Tusk full-inventory retry; King Rappy zero-Meseta and once-only behavior. Apply Release48 departure guard in a normal-save Zio scene and review all story replacements. |
| Field/interface/loading | Piata 24-frame improvement retained, five frames above historical Release07. Travel/camera/encounters/events/seams/vehicles, text/HUD and first-open/ally menus need wider coverage. |
| Ending/saves | Normal-save final victory, ending/credits, Fin input, clear writes, SRAM slots, CONTINUE and power-cycle persistence. |
| PD action speed/fidelity | Command menus poll every display frame while costly idle rendering pauses. Actions/transforms remain slow; original planes/palettes/controllers, PLC consumers, attacks/death and final integration remain open. Device-test the workaround; optimize last. |

**The open categories and newly logged defects above are broad workstreams, not isolated one-line fixes.** The level-99 corruption and scripted-last-survivor departure have passing scoped regressions; ordinary-save validation belongs to progression/ending coverage. Startup logo cleanup is complete. No further healing, wave or encounter behavior should be changed solely to make a diagnostic pass.
