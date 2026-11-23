# R237 source integration (2026-11-23)

**Accepted binary:** R237 supplied by Claude, 32 MiB, SHA-256 `a84e9c0501c16a2b750a85037cfa16bcd859101fb4b201ff4edaa1da6b2742de`, CRC32 `916fd618`.

**Lineage warning:** both repository branches share the older Release48/CP58 game source. These commits **port the five R236/R237 fixes into that source**, but do **not** establish a byte-identical source rebuild of accepted R237 nor silently replace the R235 binary. The experimental renderer is unchanged.

## Ported source changes
1. **Profound Darkness / IWRAM stack collision:** R236 System/IRQ stack tops; System-only audio mixer alternate-stack shim. This source port additionally masks/restores IME around the alternate-stack call because the older CP58 polling producer is not inherently inside a critical section. IRQ-mode production remains on the IRQ stack.
2. **Krup kindergarten / Saya:** move the actual leader, temporarily withhold Hahn from the follower list, then snap his object mirror and native follower to the 16px return grid behind the preceding party member; preserve follower ty.
3. **Event-battle map palettes:** after event battle and Garuberk same-map refresh, rebuild authored palette then replay only palette-writing manager effects; preserve all other callers (including text action 6) and avoid repeating managers' story or layout side effects.
4. **Fin:** compose both MD plane mirrors with camera-relative 512x256 layout wrapping and synchronize plane B scroll to plane A.
5. **Erase last save:** reset BG palette for START-only title menu from original title ROM colors every time that state is entered, without overwriting the party CRAM.

## Supplied R237 validation evidence (not re-run by these source commits)
- mGBA with a save: Continue -> The Edge -> PD forms 1/2/3 -> ending -> credits -> Fin: PASS.
- Krup Saya event, Zema event-battle palette, save flow and erasing final slot: PASS.
- R237 idle-loop headroom identical to R236; ~99.98% of R235 per supplied notes.
- Not tested: VBA-M, Delta/VBA-M core, GBA Micro or physical hardware.
- The supplied evidence is an ending-route replay and targeted checks, **not** a verified uninterrupted start-to-Fin playthrough.
- These CP58 source changes are unbuilt until the necessary original ROM/assets and exact modern R235 source lineage are staged and the build is rerun.

Do not merge native-viewport experimental code into the traditional renderer as part of R237 acceptance.