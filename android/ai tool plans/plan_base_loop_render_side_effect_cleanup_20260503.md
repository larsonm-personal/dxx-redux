# Plan: base loop render side-effect cleanup (d1+d2)

## Goals
- Remove replay-only render state save/restore wrappers from d1 and d2 EVENT_WINDOW_DRAW
- Move robot warning updates to simulation-time logic so behavior is consistent with and without rendering
- Keep changes general-purpose for normal gameplay on PC and Android
- Validate with focused host builds and replay test on new failing demo artifact

## Work items
- [x] Audit current warning/update call paths in d1 and d2
- [x] Update d2 to run warning update from simulation path for all modes
- [x] Remove d2 replay-only draw wrapper and related temporary guard paths
- [x] Add equivalent simulation-time warning update path in d1
- [x] Remove d1 replay-only draw wrapper
- [x] Build d1 and d2 and run targeted replay verification against android/temp_game_logs artifact
- [x] Mark this plan complete with final notes

## Final notes
- d1 and d2 now update robot warning danger state from simulation-time code paths instead of draw-time object rendering
- replay-only EVENT_WINDOW_DRAW state snapshot/restore wrappers were removed from both d1 and d2
- targeted replay verification for android/temp_game_logs/d2_descent2_level2_20260502_175246.dximdemo returned RESULT: PASS
- host builds succeeded via existing build trees: cmake --build buildd1 and cmake --build buildd2
