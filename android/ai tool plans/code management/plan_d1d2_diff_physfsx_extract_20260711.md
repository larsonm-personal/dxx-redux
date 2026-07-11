# Plan: D1/D2 PhysFSX Android Initialization Extraction 2026-07-11

## Goal
- Reduce upstream-owned D1/D2 churn by centralizing duplicated Android PhysFS initialization and search-path setup without changing desktop filesystem behavior

## Existing work to preserve
- Preserve the completed state and songs extractions
- Preserve all unrelated working-tree and mission metadata changes
- Keep game-specific directory names and compatibility behavior explicit

## Steps
- [x] Audit both `misc/physfsx.c` diffs, exact duplicated regions, dependencies, and search-path ordering
- [x] Identify the smallest shared API and any required D1/D2 adapters
- [x] Move only Android-owned behavior while retaining minimal mirrored hooks
- [x] Run scoped formatting and measure the D1/D2 diff reduction
- [x] Build Android D1/D2 for every configured ABI and build both Windows targets
- [x] Run focused launcher/game integration coverage for filesystem initialization in both games
- [x] Record results, deferred behavior, and the next recommended candidate

## Guardrails
- Do not change PhysFS mount precedence or write-directory selection
- Do not mix this extraction with mod-manager, SAF, or mission-loading redesign
- Do not move original cross-platform PhysFS behavior solely to deduplicate D1 and D2
- Keep desktop and headless builds isolated from Android-only helpers
- Preserve diagnostic messages unless a shared game label is the only difference

## Baseline
- Aggregate D1/D2 diff after the songs tranche: 343 files, +50427/-3909 against upstream
- Candidate baseline: `d1/misc/physfsx.c` +122/-7 and `d2/misc/physfsx.c` +132/-8 against upstream
- Shared seam: 115-line Android search-path block in each source, parameterized only by `d1x-redux` or `d2x-redux`
- Post-extraction aggregate: 343 files, +50207/-3909 against upstream, a reduction of 220 added D1/D2 lines
- Post-extraction sources: `d1/misc/physfsx.c` +12/-7 and `d2/misc/physfsx.c` +22/-8 against upstream

## Results
- Added `physfsx_android_shared.c/.h` and compiled the helper into both Android `misc` targets
- Retained only an Android-only include and one game-directory call in each upstream-owned source
- Added `physfs.base_dir`, `physfs.write_dir`, and ordered `physfs.search_path` fields to shared game introspection
- Extended the dual-game launch test to verify the game write directory and active imported set, and the SAF test to verify its manifest mount
- Scoped C/C++, CMake, script, encoding, and whitespace checks passed
- `run-windows-build.ps1 -Target both` passed for D1 and D2; existing unrelated compiler warnings remained, with none in the touched PhysFS sources
- `:app:externalNativeBuildDebug --rerun-tasks` passed for D1 and D2 on `arm64-v8a`, `armeabi-v7a`, and `x86_64`, with fresh helper objects and no helper warnings
- `:app:assembleDebug` passed and produced the debug APK
- `test_launch_to_automap.json5` passed for D1 and D2 with the new PhysFS assertions
- `test_saf_archiver.ps1 -NoBuild` passed and restored its temporarily removed file during cleanup
- `test_mod_loading.json5` passed for D2 at the 128 texture setting

## Deferred and next candidate
- Search-path membership is asserted directly; precedence remains covered indirectly by launching from the active set and loading the selected mod
- Keep the unrelated `PHYSFSX_getRealPath` and `PHYSFSX_fgets` upstream drift out of this extraction
- Recommended next tranche: reconcile that small residual PhysFS upstream drift separately, then resurvey the large mirrored `newmenu.c` Android hooks for another shared-helper seam
