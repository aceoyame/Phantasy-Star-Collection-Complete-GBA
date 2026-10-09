# PSIII: PSC native voices vs. original six FM + three PSG lanes (CP08 audit)

Scope: decide, song by song, which original Genesis PSIII lanes PSC's six retained native voices already contain, so
that nothing is restored twice (the cause of the rejected CP06 echo). **No PSIII audio byte is changed in CP08.**

## Method (reproducible, no listening required)

| Side | How it was read |
| --- | --- |
| Source | Original PSIII ROM (`ps3original.bin` == `ps3built.bin`), Z80 driver executed offline (`rejected/CP06_PSIII_ECHO/ps3_vgm_extract.c`, superzazu z80) for songs 0..41, 3600 frames each, mode `full`. YM2612 key-on writes give FM note onsets (6 lanes); the per-frame SN76489 state gives PSG tone onsets (3 lanes; new onset = pitch step >= 0.6 semitone, re-audible from silence, or attenuation drop >= 4); PSG noise and YM2612 DAC writes are counted separately. |
| PSC | The native six-channel script interpreter was simulated (`psc3.py`; song table `0x269EF0`, 12-byte command table `0x26A4F0`, 60-byte wave records `0x152080`). Only opcodes 0..9 occur in the 29,535 reachable note events of all 42 songs, and no event uses the 0x80 "repeat" form, so the simulation covers every event the engine can play. Note frequency = 21024.08 * step / wave_cycle. PSC song `n+1` is source song `n`. |
| Match | +-1 frame in time; pitch within 0.6 semitone either exactly or modulo octave; for PSG lanes a lead/lag window of -6..+40 frames against each PSC voice (finds doublings and delayed echo copies). |

The simulation is validated by the FM lanes alone: for the 191 FM lanes that have notes, **every lane's onsets exist in a PSC
voice with identical timing** (171 also match pitch exactly; the other 20 are 16 FM3 lanes - special-mode operator
frequencies are not readable from the VGM - plus FM5 in source songs 10/11 and FM2 in songs 16/33, all with matching timing).
PSC voice *i* is the original FM lane *i+1*; voice 2 (the FM3 slot) is empty in songs that do not use FM3.

## Result

| Class | Lanes | Meaning |
| --- | ---: | --- |
| FM lane present in PSC | 191 / 191 | nothing FM is missing |
| PSG lane **doubles a PSC voice** | 79 / 99 | same notes, same time (within 6 frames), any octave: **already represented**; restoring it again is exactly the CP06 double representation |
| PSG lane partial / texture | 15 / 99 | harmony or arpeggio texture whose notes are not in any PSC voice; all six PSC voices are busy in these songs, so restoring them needs a seventh layer (the rejected design) |
| PSG lane provably absent | 5 / 99 | source songs 10 (PSG1) and 11, 12 (PSG1 + PSG2): a slow high arpeggio (24-frame notes, 96-frame cycle, 150 notes in the first 3600 frames; PSG2 is the same line an octave lower and 2 frames earlier) |
| PSG noise / YM2612 DAC | 0 | no audible noise-channel event and no DAC write in any of the 42 songs - nothing percussive was lost there |

Why CP06 echoed: in 79 of 99 PSG lanes the overlaid notes were already played by a PSC voice, and PSC voices 0/3/4 in
several songs reproduce the PSG-lane octave (e.g. source song 11: PSC voice 0 plays PSG3's MIDI 74/73/76, not FM1's 62/61/64),
so a PSG overlay put a second copy of the same line on top. In several songs the original PSG note starts 1-3 frames before
the FM key-on of the same note (e.g. source song 9: PSG at frame 0, FM at frame 3), so a doubled overlay is audible as a
flam/echo rather than a clean unison.

Partial/texture lanes (15; source song / lane): 1 PSG3; 3 PSG2, PSG3 (parallel-interval harmonies); 9 PSG1; 10 PSG2;
18 PSG1, PSG2 and 19 PSG2, PSG3 (rising 9-frame arpeggio); 20 PSG1, PSG3; 27 PSG3; 29 PSG3; 30 PSG2, PSG3 (an identical pair).
`ps3_audit_summary.json` lists the class of every lane of every song.

## Decision

* **Nothing is restored in CP08.** The only provably absent material (the arpeggio above) can be hosted on a free native voice
  only in source songs 11 and 12 (PSC songs 12 and 13: voices 2 and 5 / 2, 4 and 5 are empty); source song 10 (PSC 11) has all
  six voices busy.
* Why it was not shipped blind: (1) the 12-byte command table is followed immediately by the 20-byte scene-record table at
  `0x28702C`, so new commands cannot be appended in place - the 117,564-byte table has to be relocated and its five literal
  pool references (`0x829C0, 0x829F4, 0x82A48, 0x82A70, 0x82ECC`) patched; (2) every PSC wave is an FM-derived complex
  cycle, a PSG-style pulse needs a new wave record; (3) the level of a restored line can only be judged by ear, and a
  wrong guess would repeat the CP06 complaint. These are listed as the exact remaining PSIII blockers.
* If the user wants the arpeggio back, the evidence needed to do it is in `ps3_audit3.json` (lane onsets, pitches and
  timing for songs 10-12) and `psc3.py` already decodes the host scripts it must be added to.

## Files

`psc3.py` (PSC script simulator), `src3.py` (VGM -> lane events), `audit_ps3.py` (helpers), `audit3.py` (classification),
`summarize3.py`, `ps3_audit3.json`, `ps3_audit_summary.json`, `ps3_audit2.json`.
Reproduce: `vgmx ps3original.bin <song> 3600 full vgm/s<song>.vgm` for songs 0..41, then `python3 audit3.py 3600 && python3 summarize3.py`.
