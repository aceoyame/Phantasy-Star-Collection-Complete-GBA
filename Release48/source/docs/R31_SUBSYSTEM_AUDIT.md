# R31 current coverage and remaining completion blockers

R31 continues the saved R30 checkpoint. The accepted audio and presentation are retained. Two early ordinary event blockers now have native source-derived executors. The game is not yet certified finishable.

Static source dispatch/data audit plus focused source-clock/event-state/OBJ probes; no full playthrough. A handled ID is a code path, not behavior certification. No SHA256 calculated.

| Dispatch | Covered | Total |
|---|---:|---:|
| Ordinary events | 155 | 161 |
| Real cutscenes | 8 | 33 |
| Map-used updates (including source no-op) | 48 | 54 |
| Map-used conditions | 98 | 98 |
| Map-used managers | 127 | 127 |

A dispatch count indicates a native path, not certification of all choreography or controller behavior.

## Implemented in R31

- RuneFlaeli full field sequence: party movement, casting actor, flame, 14 dust and 14 ballistic rock spawns, original four BG chunk edits and TonoePathOpen gate
- MeetingDorin field sequence: source dialogue branch, Alys wind-up/punch, anger lines, Dorin impact/star reactions, normal actor hiding/restoration and Dorin flag
- 54 offline native OBJ poses including multi-piece tall flames and shared dust/rock frames
- Original 16.16 debris gravity, source animation lists, actor counters and source RAM state
- VRAM cache invalidation on pose reallocation; no new EWRAM globals
- R30 field controllers, accepted audio, presentation, save layout and first 8 MiB PSC shell preserved

## Remaining ordinary event executors

| ID | Source routine |
|---|---|
| $2C | Event_MonsenEarthquake |
| $2F | Event_PsycoWandChest |
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

Verify the normal-save Rune/Dorin sequence in emulator/hardware. Next implement AlysWounded/PsycoWand/ZioDefeated cutscenes with complete choreography and map continuations, along with PsycoWandChest and its barrier/guardian actors. Battle controllers and later ending paths remain unfinished. Do not bypass story flags to claim completion.
