# Mission Route Failure Corpus Audit Round 2

## Objective

Reclassify the 164 routes that remain non-passing after the first corpus audit, measure the exact physical-shot rejection reasons, and move legitimate cases to passing only through generally valid game-physics or planner-search corrections.

## Guardrails

- Use the accepted 1,110 `ok`, 86 `partial`, and 78 `failed` corpus as the fixed starting point.
- Do not add mission, level, segment, wall, coordinate, status, or corpus-specific behavior.
- Do not keep an old route merely because a new route has a worse final status.
- Require a uniform physical proof for every accepted shot and retain navigator-occupiable firing poses.
- Separate shot-validation defects from objective-order and state-space search defects.
- Review every regression, not only aggregate status counts.

## Phase 1: authoritative round-two inventory

- [x] Pair every raw analyzer result to its archive and descriptor without relying on non-unique mission display names.
- [x] Extract all 164 non-passing levels with game, archive, mission, level, status, frontier, and exact problem.
- [x] Confirm the inventory reproduces the accepted corpus counts.

## Phase 2: measured failure buckets

- [x] Add opt-in aggregate diagnostics for firing-pose and projectile-validation rejection reasons.
- [x] Run representative levels from each broad failure family and record the dominant rejection reason.
- [x] Split failures into shot false negatives, absent optimistic topology, dependency cycles, global search/order limitations, and malformed or missing data.
- [x] Rank candidate fixes by physical correctness, affected breadth, and regression risk.

## Phase 3: general corrections

- [x] Inspect projectile collision radius, FVI behavior, sampling coverage, and transparent/no-hit traversal against actual gameplay semantics.
- [x] Implement only corrections supported by a general physical or search invariant.
- [x] Run focused tests and a full no-copy corpus regeneration after each retained correction.
- [x] Maintain a complete gained, lost, status-changed, and objective-sequence ledger.

## Phase 4: validation and report

- [x] Add or extend high-level regression coverage for every retained correction.
- [x] Run scoped code quality, native tests, Windows builds, corpus baseline test, and Android validation when runtime code changes.
- [x] Update checked-in metadata and the maintained baseline only after full-corpus review.
- [x] Report the measured buckets, fixes, remaining limitations, and validation evidence.

## Findings

The accepted round-two starting point was 1,110 `ok`, 86 `partial`, and 78 `failed`. Archive and mission indices, rather than display names, produced an exact 164-level inventory:

- 75 key unreachable
- 55 route target unreachable
- 17 trigger dependency loops
- 8 missing keys
- 5 exit unreachable
- 3 missing exits
- 1 missing level file

Opt-in wall-shot diagnostics measured the physical validator separately from final planner status. In the mapped non-passing set, approximately 2.96 million uncached candidate poses were not occupiable by the player, 1.94 million projectile rays hit another wall, 8,212 transparent/no-hit rays failed connected traversal confirmation, and only nine transparent rays passed it. More than 80 non-passing levels made no wall-shot request at all. Therefore the remaining corpus is not primarily an FVI segment-list failure; most cases are topology, data, dependency, or search failures, with a smaller physically blocked-shot population.

The retained defect was the projectile radius. The validator hard-coded `F1_0`, one game unit. Both engines create the base laser as a polymodel and calculate its collision radius as model radius divided by the weapon length-to-width ratio. With the loaded D1 and D2 data this is 11,883 fixed-point units, approximately 0.181 game units. The adapter now uses the same render-type-dependent calculation as gameplay.

A second candidate sampled nine interior points on each switch face because gameplay accepts a hit anywhere on that wall. It was rejected after measurement: among the first 594 corpus levels it changed only two routes, moved only one failure to partial, and roughly doubled analysis time. The production implementation remains center-targeted until a more efficient geometric visibility method is justified.

## Before and after

The retained radius correction produces 1,114 `ok`, 83 `partial`, and 77 `failed`, reducing non-passing routes from 164 to 160 with no status regression.

Routes moved to `ok`:

- Descent 2: Counterstrike! level 17
- Project KCX-F2 (2026) levels 1 and 6
- Orion Nebula Project D2 level 1

Two already-passing routes changed without losing status:

- levigen level 6 gained the switches and fly-through trigger required by the newly proven route.
- Omicron Project 1.4c level 2 gained a blastable-wall objective before the red key.

## Remaining buckets

The final 160 non-passing routes contain 74 key-unreachable cases, 53 route-target-unreachable cases, 16 explicit trigger dependency loops, 8 missing keys, 5 unreachable exits, 3 missing exits, and 1 missing level file.

- Missing objects, exits, and level files are data defects unless mission-specific alternate completion mechanics can be demonstrated.
- Explicit trigger loops require state-space search or backtracking over trigger order; relaxing shot proof cannot solve them legitimately.
- Start-only key and route-target failures with no wall-shot requests have no optimistic planner connection and need topology or unsupported-mechanic inspection.
- Partial key and route-target failures that do invoke wall shots split mainly between player-unoccupiable poses and rays physically blocked by other walls. The advisory FVI segment-chain rejection is now a minor bucket.
- PhenoHunt level 1 remains the known global-order example: a valid remote shot bypasses a useful fly-through side effect. Correct recovery requires global planning, not a corpus-status fallback.

## Validation evidence

- Full retained-fix host regeneration: 109 analyzable archives passed, one descriptor-less archive skipped, and zero generator failures.
- Full corpus review found four status gains, zero status losses, and two reviewed same-status sequence changes.
- D1 and D2 Windows builds passed.
- D1 and D2 level-metadata, route-snapshot, and route-analysis-cache native tests passed.
- The maintained 1,274-level route corpus baseline was updated and passed exactly.
- Scoped code quality passed.
- Android debug build passed for arm64-v8a, armeabi-v7a, and x86_64 with JDK 21.
