# R38 progress — 2026-10-01

Checkpoint 01: original RajaSick ($12) and MeetingKyra ($13) traced; first native handlers, AF map continuations and append-only source actor art implemented. These changes are not yet verified or built. Preserve R37 as the tested baseline.

Source: ps4.asm $7734C and $77788. Raja: Tree18 entries59/62/63, true clinic134/133 reload, carrier/macro removal, collapsed374 actor, complete64-byte Esper copy, source waits/facing/camera and RedAlert. Kyra: Tree37 entries27/28, panel140, F7 seam, slot5/macro/flagA0 and real Dezolis reload to exact IceDigger placement.

R31 actor bank has419 poses/160480bytes, with all156128 R37 bytes retained. Type8194 Esper variants use original19E726 art; Saya remains unchanged.

Next: source probes, exact Esper step state, native compiler and preservation checks; save checkpoint02 after probes, final04 after full build. No emulator/hardware playthrough.

Checkpoint 02: scene probes pass under ASan/UBSan. The independent48-frame Esper reference passes, all Raja follower positions and literal64-byte copy are tested, Tree18/37 pause seams and exact Kyra vehicle coordinates are checked. R31–R37 regression probes pass. First ARM build passed; rebuilding after a focused clinic NPC/source-record synchronization fix before final memory/preservation checks. Ordinary NPC wandering, map load, UI/panel drawing and hardware are stubbed in the scene probe.

Checkpoint03: final ARM build, accepted audio/shell/prefix comparison, unchanged EWRAM objects and RAM/ROM bounds pass. Original panel140 compositor matches all38400 independent reference pixels under ASan/UBSan. Native sprite and panel previews inspected. Remaining: final ZIP/restore equality and durable final delivery. Read R38_NOTES.md.

Final04: cumulative source handoff and evidence. All build/source/panel/preservation checks pass; final recovery is verified against the resulting archive before delivery.
