# Guidebot Regression Investigation Plan

## Scope

- Determine why the latest regeneration changed `game_data/mission_files/KCXF2RMv11.json`
- Trace recent Guide-Bot regressions involving doors, unreachable targets, marker 9, spinning, and stalled reachable goals
- Fix confirmed regressions with the smallest cross-platform source changes and add focused regression coverage

## Work

- [x] Compare the generated mission metadata with its prior revision and trace the responsible generator change
- [x] Review recent Guide-Bot pathfinding and goal-selection changes, including nearest-point fallback and marker handling
- [x] Reproduce or isolate the failure with existing analyzers/tests and add diagnostic coverage where needed
- [x] Implement confirmed fixes in both relevant game variants where applicable
- [x] Run scoped formatting, builds, and focused tests
- [x] Record findings and verification here

## Findings

- Both the August 19 Android regeneration artifact and a fresh focused host run reproduce KCXF2 level 1 as a zero-distance partial route ending at trigger 1
- Commit `827e8f91` removed the base laser projectile-radius override around per-level HXM loading from both analyzers. The resulting mission-modified projectile radius selects trigger 1 from segment 181 side 3 instead of the previously valid segment 154 firing position, after which the route is unreachable
- Android direct Guide-Bot actions are injected as raw Shift+digit keys. Repeating an action therefore invokes the legacy repeated-digit marker selection behavior and emits `Marker 9 not placed` for repeated exit commands
- The Android analyzer and live game used different switch-projectile radii in their route-cache analysis profiles. The analyzer completed successfully, but the live game rejected its cache and remained `calculating`; preserving the base radius in both paths keeps their profiles identical
- After a valid route is created, both normal goal-transition branches replace every path shorter than three points with a random five-segment path. New semantic route goals frequently target a nearby door or firing position, so this legacy fallback now causes visible spinning or movement away from the intended waypoint
- Classic direct-goal failure still converts an incomplete path into SCRAM behavior rather than retaining a useful partial path. The semantic route planner can provide a door/frontier target, but only after metadata is ready

## Verification

- Focused host regeneration after the fix: `regenerate_all_mission_metadata_host.ps1 -ArchiveName KCXF2RMv11.7z -NoRegressionCopy`, passed; KCXF2 level 1 is `ok`, distance `4871.719`, five steps, with trigger 1 at segment 154/wall 12
- Fresh Android metadata generation produced the same corrected KCXF2 level 1 route
- Android `assembleDebug` passed for arm64-v8a, armeabi-v7a, and x86_64
- Windows D2 build passed; CTest passed 40/40, including `test_escort_goal_policy`
- `test_kcxf2_guidebot_route_next.json5` passed all 179 steps on emulator-5554, including repeated Exit commands with marker state cleared
- Scoped code quality passed for all changed files
