# R235 native viewport experiment — test record

- Source ROM R235 (unmodified) SHA-256: `4caf23ae8b030c16a32658fdc95085330de6c6dd4a51498f0e2b9c17c7d89308`
- Host C11 camera / clipping / UI assertions: **PASS**
- Academy verified 1:1 wall geometry: **9,900 registered pixels, 11 colors, zero mapping violations**
- ROM modified for live rendering: **NO**
- Emulator tests for *this new renderer*: **NOT YET AVAILABLE**
- Original static R235 crop-preview ROM: separate prior experiment
- GBA Micro / real hardware validation: **NOT TESTED**

The accepted R235 ROM cannot safely be recompiled from the public R48 tree; the separate C++ PSIV checkout has been ruled out as a proven R235 ancestor. Authentic R235 renderer source must be staged and verified before moving these functions into active gameplay.
