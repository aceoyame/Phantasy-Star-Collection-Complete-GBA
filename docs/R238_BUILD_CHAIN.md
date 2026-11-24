# R238 integration from accepted R237 (2026-11-23)

This repository is intentionally ROM-free. The user-supplied `PSC_R238_checkpoint3.zip` contains the complete *new* R238 source files, `R237_to_R238.ips32`, three pre-generated world maps, MAP/teleport tables, and the exact R238 32-MiB candidate. Do not upload those original graphics or complete commercial ROMs to GitHub.

## Trust / lineage

- Accepted R237: SHA-256 `a84e9c0501c16a2b750a85037cfa16bcd859101fb4b201ff4edaa1da6b2742de`.
- Supplied R238: SHA-256 `20f4fa76fb591b453a9c28b2b650a4dc1259904b083cbe0cc3129fb638a2c558`.
- Stage26 experimental *R237* parent: SHA-256 `3154fb729fdb016574d63dff8dea84e01f1b77e2ac346390530d4f90d932a1a0`.
- `main` retains older CP58 native source that does NOT byte-rebuild R237. The new deterministic **mainline binary chain** starts from the accepted R237 ROM; it does not claim to be a C-source rebuild of R237.

## Mainline exact builder

```bash
python3 tools/r238/integrate_r238.py \
 --r237 /private/PSC_R237_CANDIDATE.gba \
 --checkpoint /private/PSC_R238_checkpoint3.zip \
 --out /private/PSC_R238_MAINLINE.gba \
 --report /private/R238_MAINLINE.json
```

Hash-pinned IPS32 and package ROM are cross-checked byte-for-byte. No compiler/version drift. The supplied original `r238/build/build_r238.py` and `r238_options.c` can separately be used for evolving source, but changing LLVM versions can change linked bytes; the exact supplied patch is the accepted checkpoint oracle.

## R238 additions (user-provided)

R235-style SYS > OPTIONS: BUG FIXES (default OFF), EXP x1..x4 (default x1), MESETA x1..x4 (default x1), ENEMIES Easy/Normal/Hard/Expert (default Normal), CUT CONTENT (default OFF: MAP/BLD-AXE/SPIRAL), and MAP WARP (default OFF). SRAM options record starts at `0x0E003F80`. Adds generated Motavia, Dezolis, Rykros maps, map fog saved in unused per-save RAM, and map-based destination selection for visited towns. `r238_options.c` and the generator scripts remain in the user ZIP for source evolution. Existing R237 stack/Saya/palette/Fin/title fixes are inherited.

## Experimental Stage26 rebase

```bash
python3 tools/r238/integrate_r238.py \
 --r237 /private/PSC_R237_CANDIDATE.gba \
 --checkpoint /private/PSC_R238_checkpoint3.zip \
 --stage26 /private/R237_Stage26.gba \
 --out /private/PSC_R238_Stage26_EXPERIMENTAL.gba \
 --report /private/R238_Stage26.json
```

The optional stage input is SHA-pinned and MUST be the exact Stage26 output of `artwork/stage26_restore_overwritten_data.py`. The merge applies each actual R238-vs-R237 changed byte only if Stage26 still holds the original R237 byte; shared edits abort loudly. Stage26's relocated helpers at `0x0924CE40+` and original graphics helper locations remain intact. R238 options code begins at `0x0922BA00` immediately after Stage21C's art allocation ending `0x0922BA00`; maps live at `0x09238000+`, below Stage26's relocated helpers. These are **static layout checks**, not proof of runtime compatibility.

To rebuild Stage26 from private R235, Stage14, Stage21C and R237 ZIP inputs, run the experimental branch's `artwork/build_stage26_r238.py` one-step wrapper (which invokes the existing audited Stage21C and Stage26 builders before this merger).

## Verification / release gate

- The supplied R238 mGBA/headless tests cover option windows, EXP/Meseta changes, enemy stat tiers, and battle-hook correction. They do **not** prove a new full start-to-Fin playthrough.
- Mainline deterministic patch round-tripped with 471 IPS32 records; exact SHA-256 matched the supplied candidate.
- Stage26 real input ROM is not publicly checked in and was not available for a live combined build in this session. Do not label the experimental combined ROM accepted or playable until the exact Stage26 ROM is staged, merged without conflicts and replayed in mGBA and VBA-M/Delta, including field/map/SRAM save behavior, Saya/Zema, the ending and Fin.
- Preserve original un-cropped R237 scenic battle backdrops, experimental native art, and no GBA Micro-unsafe EWRAM hack.
