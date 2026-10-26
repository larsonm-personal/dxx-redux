# Singleplayer gameplay support roadmap

Continue the original 10-20 comparable chunks estimate from
[the teleport plan](xl_wall_teleports_20261025.md). The estimate describes broadly
useful gameplay fidelity with a longer tail of unusual missions; it is not a
fixed feature count, percentage, or promise of universal mission support

## Completion requirements

Keep the existing shared stock/XL binary, capability-based detection and useful
rejections. Every admitted feature needs reference semantics, activation through
the production runtime, persistence/lifecycle checks and stock isolation.
Loading, gameplay qualification and presentation remain separate claims

Defer multiplayer and cosmetic rendering work. Retain gameplay-affecting
visibility, transparency, collision, audio cues and authored resource semantics.
Use controlled poses, physical crossings, authored actions and combat checks;
do not attempt autonomous 3D navigation

The original overlapping work areas remain open until their acceptance evidence
is complete. In particular, completing a parser or a derived fixture does not
finish original-mission qualification

| Work area | Current evidence and remaining acceptance |
| --- | --- |
| Teleports, speed boosts, spawn destinations | Immediate wall teleports verified. Boost direction/constraints/release/orientation, boost-area collision, spawn state and interactions remain open |
| Trigger chains, timing/flags, object events | Immediate wall masters and enable/disable are the active chunk. Delayed/permanent/alternating/autoplay behavior, cancellation, scheduling state, object activation and saved chains remain open |
| Segment properties, walls and generators | Modern robot-free water and no-damage gates verified within documented limits. Repair/team/ownership/lava/blocked/speed-area behavior, gameplay wall semantics, equipment generators and XL robot-generation differences remain open |
| Layouts, capacities, custom gameplay data and transitions | Audited bounded 15/33, 17/36 and 23/40 readers exist. Other observed layouts/payloads, wider tables and dynamic spawn headroom, expanded HAM/HXM robot/weapon data and authored transitions remain open |
| Cross-feature regressions and original missions | Existing Ahayweh Gate qualification and documented Neptune/Observatory evidence remain scoped. Combined movement/events/generators/combat/persistence controls and original missions exercising newly supported features remain open |

## Chunk ledger

1. Immediate wall teleports: complete within the evidence scope recorded in
   [wall-teleports.md](../../XL_compat/evidence/wall-teleports.md)
2. Immediate wall trigger chains and enable/disable counters: native and Android
   checks complete within the [recorded scope](../../XL_compat/evidence/trigger-chains.md)
   and [chunk plan](xl_trigger_chains_20261026.md)

Next take trigger scheduling and stateful flags as a coherent chunk: positive
and randomized delays, permanent/repeat behavior, cancellation through disabling,
and source save/restore. Inspect the reference before choosing the precise
implementation boundary; scheduling is a known uncertainty in the estimate

After that, continue the unfinished areas above one chunk at a time. Do not mark
the overall goal complete from a count of completed chunks or green fixture
tests. Audit each area against live behavior and original mission requirements
