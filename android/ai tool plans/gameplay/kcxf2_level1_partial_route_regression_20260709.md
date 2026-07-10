# KCXF2RM level 1 route-status regression

## Goal
Restore KCXF2RM level 1 metadata routing from `partial` to `ok` without making
the analyzer optimistic about an edge the guidebot cannot execute, and verify
that the Kotlin metadata consumer and guidebot use the same route result.

## Plan
- [x] Identify the first unresolved route step and compare it with the previous
  generated route.
- [x] Trace that step through static topology, live edge classification, trigger
  dependencies, and executable guidebot path creation.
- [x] Review Kotlin route presentation/consumption and the guidebot adapter for
  duplicated or divergent path semantics.
- [x] Fix the shared route model at the narrowest correct ownership boundary.
- [x] Add a focused native regression fixture for the failing topology.
- [x] Regenerate KCXF2RM metadata and confirm level 1 returns to `ok`.
- [x] Run D1/D2 native tests, Windows builds, Android tests/build, scoped quality
  checks, and the focused KCXF2 guidebot integration script as applicable.

## Findings
- The strict directional traversal correctly rejected the old route's implicit
  reverse-side flyability. That exposed a real missing dependency rather than a
  nonexistent route.
- The partial result was a false trigger cycle. Trigger 2's source route first
  encountered trigger 1, while trigger 1's source route first encountered
  trigger 2. The scanner returned to dependency recursion before checking
  whether either shoot switch was visible from the currently reachable mine.
- Trigger 1 wall 12 is shootable from reachable segment 60. Once trigger 1 is
  fired, trigger 2 and the exit are reachable normally.
- Kotlin has no separate path solver. `LevelMetadata.kt` invokes the native
  analyzer and parses/serializes its route result, so the native scanner is the
  Kotlin metadata source of truth.
- Guidebot and metadata already use the shared directional edge classifier.
  Guidebot's shoot-switch ray accepts either an unobstructed ray or an exact hit
  on the intended wall. Metadata previously used only the generic unobstructed
  target-position ray, which rejected the expected terminal hit on the switch
  wall.
- Guidebot's wall firing-position search omitted the near-vertex samples used by
  metadata. That could still let metadata accept a switch position the guidebot
  would not consider.

## Changes
- Added a wall-specific visibility callback to the shared scan view. Metadata
  and guidebot now call the same game-adapter function for the wall center,
  `FQ_TRANSWALL` ray, and exact target-wall hit rule.
- Changed shoot-switch planning to search all currently reachable firing
  positions before recursing through optimistic trigger dependencies.
- Added near-vertex wall samples to guidebot, matching metadata's segment center,
  near-side, and near-vertex candidate set.
- Added a native regression fixture in which a visible shoot switch appears to
  be behind a different trigger dependency. The expected route fires the visible
  switch first and remains `ok`.
- Regenerated the mission metadata. KCXF2RM level 1 now records:
  `Start -> trigger 3 -> trigger 1 -> trigger 2 -> Exit`.

## Validation
- Final direct KCXF2RM host analysis: level 1 `route_status: ok`, empty
  `route_problem`.
- Full host metadata regeneration: 109 archives passed, 0 failed; the one skip
  was `ewithin-versions.zip`, which has no mission descriptor.
- `run-windows-build.ps1 -Target both`: passed for D1 and D2.
- D1 CTest: 13/13 passed.
- D2 CTest: 14/14 passed.
- `:app:testDebugUnitTest :app:assembleDebug`: passed with JDK 21.
- Final incremental `:app:assembleDebug`: passed for all configured Android ABIs.
- Scoped `run-code-quality.ps1 -Fix`: passed.
- An exact guidebot playthrough was not run because KCXF2RM level 1 has no placed
  guidebot. The parity review instead covered the shared edge classifier, ray
  semantics, and firing-position sample set used by the live guidebot path.
