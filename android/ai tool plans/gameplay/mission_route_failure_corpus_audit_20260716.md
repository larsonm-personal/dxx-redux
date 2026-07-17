# Mission Route Failure Corpus Audit

## Objective

Survey every checked-in mission level whose canonical route is not `ok`, group failures by planner cause, and move legitimate cases to passing through general route-model, topology, or search fixes. Do not add mission names, level numbers, coordinate thresholds, or other corpus-specific pathing exceptions.

## Guardrails

- Treat checked-in mission metadata and a fresh host regeneration as corpus evidence, not as an oracle that every level must pass.
- Distinguish genuinely unsolvable or malformed levels from planner limitations.
- Require every proposed fix to describe a general game semantic that applies independently of the corpus.
- Reject fixes that merely relax validation, ignore blockers, force a preferred objective order, or special-case a level pattern without a physical/gameplay justification.
- Compare the full corpus before and after every planner change. A net pass-count increase is insufficient if existing passing routes regress or objective sequences become physically invalid.

## Phase 1: authoritative inventory

- [x] Locate the current mission-route baseline schema and generation pipeline.
- [x] Parse every level into a normalized table containing game, mission, level, status, problem, partial frontier, and final route step.
- [x] Regenerate the corpus with the current host analyzers so stale checked-in metadata is not mistaken for current behavior.
- [x] Record total `ok`, `partial`, and `failed` counts and identify changes from the checked-in baseline.

## Phase 2: failure buckets

- [x] Bucket exact problems first, then consolidate only when code inspection shows the same underlying planner branch.
- [x] For each bucket, sample multiple missions and inspect route snapshots, partial steps, blocker details, and relevant level metadata.
- [x] Classify each bucket as likely planner defect, likely level/data defect, intentionally unsupported game mechanic, or insufficient evidence.
- [x] Rank general fixes by expected corpus coverage, correctness confidence, and regression risk.

## Phase 3: general fixes

- [x] Use the existing callback-level firing-path unit coverage and the live negative control for the adapter-specific FVI semantic. A synthetic route fixture cannot reproduce FVI's malformed advisory segment list.
- [x] Implement the smallest general planner or snapshot correction.
- [x] Run route unit tests and regenerate the full corpus after each correction.
- [x] Keep a before/after ledger of levels gained, levels lost, status changes, and objective-sequence changes.

## Phase 4: validation and report

- [x] Manually or through automation validate representative changed routes in the game when metadata alone cannot prove physical correctness.
- [x] Run scoped code quality, D1/D2 Windows builds, relevant tests, and Android build if runtime code changes.
- [x] Update the maintained route baseline only after the new full-corpus result is accepted.
- [x] Produce a concise bucket report, list every level moved to passing, document unresolved legitimate failures, and mark this plan with final evidence.

## Findings

The reviewed strict-projectile corpus began at 1,085 `ok`, 100 `partial`, and 89 `failed` routes across 1,274 levels. The dominant non-passing families were route targets with no optimistic connection, keys whose dependency path could not be resolved, trigger dependency loops, missing mission objects or exits, and malformed/missing level data.

The highest-confidence broad defect was below the planner. The engine's FVI implementation explicitly documents that its returned segment list is occasionally incorrect. The projectile validator nevertheless treated that advisory list as stronger evidence than FVI reporting that the first collision was the intended switch wall. This rejected physically proven shots and cascaded into key and endpoint failures.

The implemented rule is:

- A navigator-occupiable firing pose is still required.
- If projectile-radius FVI's first collision is the intended trigger wall, that collision is authoritative physical proof.
- A transparent/no-hit trace still requires a credible connected segment traversal.
- The removed `no locked door` fallback remains removed.
- There are no mission, level, trigger, segment, coordinate, or corpus-status exceptions.

## Before and after

The final corpus is 1,110 `ok`, 86 `partial`, and 78 `failed`. This is a net reduction from 189 to 164 non-passing routes.

- 16 `partial` routes became `ok`.
- 10 `failed` routes became `ok`.
- Konflict at Karon level 1 moved from `failed` to `partial`.
- PhenoHunt level 1 moved from `ok` to `partial` and remains deliberately visible as a global-planning defect. A valid shorter shot bypasses a fly-through trigger that is independently needed later; solving that correctly requires state-space planning or backtracking, not a status fallback.

Routes moved to `ok`:

- - MOON 04 - level 1
- Ascent levels 4, 6, 11, 16, and 18
- Bahagad Outbreak level 5
- ptmc: castaway secret level -1
- Descent Maximum (fixed) level 23
- DESCENT: DIE HARD! levels 6 and 11
- Disintegration levels 2 and 12
- Revenge o' Drillers level 3
- Entropy Experiment secret level -1
- Kryllidian Krusade level 2
- Descent 1: The Lost Levels level 20
- Plutonian Shores levels 1, 3, and 13
- Descent: The Enemy Within levels 11, 17, and secret level -2
- Trinity United level 44
- Target: Uranus (1999) level 1
- ulterior ultima thule level 17

## Remaining buckets

- `route target unreachable`: the largest endpoint bucket. Twenty-two cases fail at Start; the remainder stop after a key or trigger. This combines malformed/disconnected topology with the planner's lack of global recovery actions.
- `blue/red/gold key unreachable`: the largest dependency bucket. These need per-level blocker evidence before relaxing any rule; many are likely unresolved switch chains, while some are intentionally malformed puzzle layouts.
- `trigger route dependency loop`: a distinct search/state-space limitation. Repeatedly selecting a trigger whose source depends on its own target is correctly rejected, but alternate trigger ordering is not globally searched.
- Missing keys, exits, reactors, or level files: generally data/mission defects unless a concrete carrier or alternate completion mechanic is present.
- Exit unreachable: retained as a physical topology failure, separate from a missing exit.

## Validation evidence

- Full host regeneration: 109 analyzable archives passed, one descriptor-less archive skipped, zero generator failures.
- Native D1 and D2 host test suites passed.
- Windows D1 and D2 builds passed.
- Android debug build and install passed for arm64-v8a, armeabi-v7a, and x86_64 using JDK 21.
- Fresh-cache Obsidian level 2 automation passed. It still starts with the blue key, trigger 4 is actionable only after the key, and the route is complete. Its new seven-step route does not restore the rejected spawn-side shot.
- KCX-F2 live positive-control setup was blocked by missing base `descent2.hog` after clearing app data; the same mission remained complete in fresh host regeneration and callback-level native coverage passed.
- Clang-format check and `git diff --check` passed.
- The maintained 1,274-level corpus baseline was regenerated and then passed exactly.
