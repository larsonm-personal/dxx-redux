# Counterstrike level 20 post-boss exit grate

## Goal

Prevent the Guide-Bot from routing from the level 20 boss to the exit through a non-passable grate.

## Plan

- [x] Inspect the generated boss-to-exit semantic path and its non-flyable wall edges.
- [x] Distinguish a canonical route-search defect from live Guide-Bot path generation or certification.
- [x] Compare the legacy openable-door rule with current navigator flyability rules.
- [x] Implement the smallest general correction without level-specific identifiers or recurring broad searches.
- [x] Add focused Counterstrike level 20 path-passability regression coverage.
- [x] Verify quality checks, native tests, and D1/D2 builds.

## Constraints

- Transparent surfaces may permit weapon shots without permitting player or Guide-Bot traversal.
- Reuse prepared route topology and cached paths on the phone.
- Preserve current-state certification and avoid mission-specific segment, wall, or trigger exceptions.

## Findings

- The shared planner and live certifier selected the correct objective and certified a current-state path.
- `escort_create_path_to_goal` kept only the path terminal segment and asked the legacy AI to search again.
- The legacy search treats any controlled door as openable while pursuing an objective, including a closed grate whose trigger has not fired.
- Route-goal path creation now reuses the legacy bounded breadth-first search but filters each edge through `guidebot_route_side_passable_current`.
- The extra work is a constant-time lookup per edge already visited by path creation. Clearance data and topology remain precomputed.
- The level 20 emulator regression advances to the boss, starts Guide-Bot in segment 206, destroys the boss, and verifies every edge of the 66-point exit path against live wall state.
- Canonical metadata output did not change, so route metadata regeneration and a cache generation bump are not needed.

## Verification

- Scoped code quality checks passed.
- Counterstrike level 20 post-boss emulator regression passed twice, including after the final edge-validation adjustment.
- Android debug APK assembled successfully for arm64-v8a, armeabi-v7a, and x86_64.
- D2 Windows build passed and all 43 D2 native tests passed.
- D1 Windows build passed. The D1 build tree has no registered CTest tests.
