# R30 current coverage and remaining completion blockers

R30 is a built checkpoint, not a finishable-game certification. R28 audio and R29 presentation are retained. Several prior R29 missing-event entries were false positives caused by the coverage parser omitting range dispatches and the guardian helper.

Static source dispatch/data audit plus focused status/tile-animation probes; no full playthrough. A handled ID is a code path, not behavior certification. No SHA256 calculated.

| Dispatch | Covered | Total |
|---|---:|---:|
| Ordinary events | 153 | 161 |
| Real cutscenes | 8 | 33 |
| Map-used updates (including source no-op) | 48 | 54 |
| Map-used conditions | 98 | 98 |
| Map-used managers | 127 | 127 |

A dispatch count indicates a native path, not certification of all choreography or controller behavior.

## Implemented in R30

- Fourteen source palette controllers
- Vah Fort/Weapon Plant conveyor animation and dirty-chunk presentation
- Strength/Courage ring chest completion and Canceller event gates
- Six source tile animation controllers; 19 records in nine maps
- Field poison/death/paralysis/android regeneration cadence
- Original vehicle field graphics and palettes; source vehicle encounter rate
- AlysFound shared executor and macro insertion
- GettingLandRover source party/camera/conveyor sequence, map continuation and ControlKey replacement

## Remaining ordinary event executors

| ID | Source routine |
|---|---|
| $27 | Event_RuneFlaeli |
| $2C | Event_MonsenEarthquake |
| $2F | Event_PsycoWandChest |
| $32 | Event_MeetingDorin |
| $46 | Event_EclipseTorchStolen |
| $47 | Event_EclipseTorchUsed |
| $48 | Event_MeetingLeRoof |
| $86 | Event_DyingBoy |

## Remaining cutscenes

| ID | Source routine |
|---|---|
| $09 | Cutscene_AlysWounded |
| $0A | Cutscene_PsycoWand |
| $0B | Cutscene_ZioDefeated |
| $0C | Cutscene_MeetingWren |
| $0D | Cutscene_InsideSpaceship |
| $0E | Cutscene_SpaceshipSabotage |
| $0F | Cutscene_CrashLanding |
| $10 | Cutscene_Landale |
| $11 | Cutscene_DarkForce1Defeated |
| $12 | Cutscene_RajaSick |
| $13 | Cutscene_MeetingKyra |
| $14 | Cutscene_LutzRevelation |
| $15 | Cutscene_FindingAirCastle |
| $16 | Cutscene_LashiecDefeated |
| $17 | Cutscene_DarkForce2Defeated |
| $18 | Cutscene_GumbiousBishop |
| $19 | Cutscene_MeetingSeth |
| $1A | Cutscene_AeroPrism |
| $1B | Cutscene_Rykros |
| $1C | Cutscene_LeRoofAgain |
| $1D | Cutscene_BeforeElsydeonCave |
| $1E | Cutscene_Elsydeon |
| $1F | Cutscene_Reunion |
| $20 | Cutscene_ProfoundDarkness |
| $21 | Cutscene_Ending |

## Remaining map-used updates

| ID | Source routine |
|---|---|
| $05 | MapUpdate_ZioFortMoveUpdateRate |
| $07 | MapUpdate_ZioFortMoveUpdateRate2 |
| $0D | MapUpdate_BioPlantMoveUpdateRate |
| $0F | MapUpdate_MachCntrMoveUpdateRate |
| $10 | MapUpdate_PlateSysMoveUpdateRate |
| $22 | MapUpdate_GaruberkTowerPart4 |

## Remaining controller and verification work

- Field teleport: Hinas/Ryuka/Escapipe/Telepipe selectors and controllers
- Vehicle-specific traversal, collision and weapons
- Composite/mechanical field object parity outside the implemented handlers
- Remaining physical weapon and movement-heavy TECH/SKILL controllers
- Profound Darkness phases and the normal ending/credits path
- Normal-save full playthrough and hardware timing remain unverified

## Immediate next work

Translate RuneFlaeli and MeetingDorin with their complete source actors/animations, then the missing AlysWounded/PsycoWand/ZioDefeated cutscenes and map continuations. GettingLandRover no longer ends at a missing executor. Do not bypass story flags to claim completion.
