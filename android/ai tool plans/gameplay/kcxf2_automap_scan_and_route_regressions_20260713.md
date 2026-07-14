# KCXF2 Automap, Scan, and Route Regressions

## Scope

Resolve four related KCXF2 regressions while preserving the 1996 Guide-Bot movement, steering, collision, flare, and RNG behavior:

- Show the current route objective and up to two following objectives in red at the automap upper left.
- Prevent high-level Guide-Bot routes from using passages that the player's ship cannot traverse.
- Fix the mission metadata scan that stalls at progress step 3/5 for KCXF2's normal and secret levels.
- Make the automap objective overlay show every metadata objective, including KCXF2 level 6 objectives 1-7.

## Constraints

- Shared C++ route semantics remain the source of truth for metadata and live Guide-Bot goals.
- The Guide-Bot route query must model where the player can travel; no changes may be made to classic Guide-Bot movement or local path-following behavior.
- Automap additions are presentation-only and must consume the existing route/objective model.
- Scan progress must account for normal and secret levels and must expose failures instead of appearing frozen.

## Plan

- [x] Trace and reproduce the four regressions from the shared planner through the automap and Android scan UI.
- [x] Use a player-sized navigator profile for live Guide-Bot high-level route selection and cover narrow-passage avoidance with regression tests.
- [x] Repair objective projection so the objective cheat renders all route objectives, then add the red current-plus-two automap list.
- [x] Repair mission scan enumeration/progress for normal and secret levels and add regression coverage for KCXF2.
- [x] Run scoped formatting and quality checks, native route tests, D1/D2 builds, Android tests/builds, metadata regression checks, and focused emulator coverage.

## Notes

- The worktree contains the accepted directed-crossing and metadata-regeneration changes from the preceding route-planner tranche. This work builds on them without reverting generated metadata or unrelated edits.
- Scoped quality checks, D1/D2 Windows builds, native route/scan tests, Android JVM tests, and the debug APK build passed.
- The regenerated metadata corpus matched all 1,274 comparable checked-in level statuses with zero degradations or improvements; 109 of 110 archives passed and the descriptor-less `ewithin-versions` archive was skipped as expected.
- Focused emulator tests passed for KCXF2 level 6 objective projection and the KCXF2 level 5 Guide-Bot route, including the full trigger 11/15/19/22 progression.
