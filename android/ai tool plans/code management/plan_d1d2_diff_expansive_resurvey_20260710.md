# Plan: D1/D2 Diff Expansive Resurvey 2026-07-10

## Goal
- Refresh the cleanup backlog after recent feature work and continue with the highest-value behavior-preserving extraction from upstream-owned D1/D2 files

## Scope
- Modified files inherited from `upstream/main`
- Branch-owned or Android-owned helper bodies embedded in those files
- Existing shared helper surfaces under `android/app/src/main/cpp/shared/`
- Out of scope: branch-added D1/D2 files unless they reveal a natural shared boundary, substantive feature redesign, broad upstream formatting, and unrelated mission metadata edits

## Existing user changes
- Preserve the pre-existing modifications to `game_data/mission_files/af-d2x.json`
- Preserve the pre-existing modifications to `game_data/mission_files/anachron.json`

## Steps
- [x] Re-read the prior diff-shrink studies, execution plans, and completed tranche notes
- [x] Refresh modified-versus-added inventory and per-file churn against `upstream/main`
- [x] Classify the leading modified files by feature ownership and extraction safety
- [x] Select one bounded extraction with a clear before/after metric and existing focused validation
- [x] Implement the extraction with mirrored D1/D2 hooks where applicable
- [x] Run scoped code quality, Android native builds, Windows host builds, and focused integration tests
- [x] Record results, remaining candidates, and the next recommended tranche

## Baseline
- Branch: `cmake`
- `android/helpers/diff_vs_upstream.ps1`: 343 files, 154 D1 files, 189 D2 files, `+51363/-3909`
- Direct helper inventory: 34 branch-added files and 309 inherited modified files under D1/D2
- `upstream/main...HEAD` status inventory: 34 added files and 304 modified files under D1/D2; the count differs because the three-dot comparison uses the merge base
- Previous completed newmenu tranche ended at the same `+51363/-3909` baseline

## Resurvey findings
- The old phase-3 progress checklist is stale: `coop_save`, `coop_warp`, `auto_net`, major input-demo cores, many OGL helpers, playsave helpers, state rewind/save orchestration, and three newmenu slices have already moved to shared implementations
- Branch-added sinks such as `input_demo_hooks.c` and the D1-in-D2 files remain excluded from the upstream-owned priority ranking even when their raw line counts are large
- `d2/main/escort.c` is the largest modified file, but its current growth is mostly substantive D2 route/coop behavior and is not the safest diff-only extraction target
- Remaining OGL churn is still dominated by the interleaved ETC2/KTX2 upload path that prior plans intentionally deferred
- `state.c` is the largest paired upstream-owned persistence target and now mixes engine-level deterministic save-format work with smaller Android-only adapter blocks
- The deterministic runtime-state serializers in `state.c` are engine-level format changes and should stay local for compatibility and upstream review
- The Android metadata trailer readers are exactly identical in D1 and D2, total 76 lines per file, use only the existing rewind/PhysFS and coop-metadata shared APIs, and naturally belong in `state_android_shared.c`
- D2-only escort-owner remap and raw PhysFS copy adapters may also move later, but are excluded from the first slice to keep validation and failure attribution narrow
- The old queued `net_udp.c` welcome-player recommendation is stale after subsequent feature work; revisit it only when a fresh, narrow boundary is identified
- `playsave.c` already has a substantial shared bridge; the remaining bodies need a fresh file-layout ownership audit before another move
- `songs.c` contains the largest remaining nearly identical Android-owned block, approximately 650 to 700 combined lines, and is the highest-payoff next extraction if paired with focused music-control automation
- `physfsx.cpp` contains a smaller duplicated Android initialization path, approximately 180 to 210 combined lines, with lower behavioral risk than the songs work
- `gamecntl.c` has approximately 290 to 305 combined lines of duplicated save/load dispatch that fit a shared Android control helper but mutate menu and game state

## Selected implementation
- Move `state_android_read_android_metadata_trailer` and `state_android_read_coop_metadata_trailer` from both `state.c` files into `state_android_shared.{c,h}`
- Keep all call sites and trailer semantics unchanged
- Validate both disk-backed saves and memory-backed rewind/checkpoint paths through builds plus focused save/replay tests

## Implementation outcome
- Added the two reader declarations to `state_android_shared.h` and their single shared implementations to `state_android_shared.c`
- Removed the exact duplicate reader bodies from both `d1/main/state.c` and `d2/main/state.c`
- Kept existing call sites unchanged and used the underlying `rewind_file_*` API so disk-backed and memory-backed reads retain the same behavior
- Left deterministic runtime-state serialization and all cross-platform save-format logic in the engine files
- No build-system change was needed because `state_android_shared.c` was already linked into both Android game targets

## Validation
- Scoped code quality passed for `state_android_shared.c` and `state_android_shared.h`
- Android `:app:externalNativeBuildDebug` passed for D1 and D2 on arm64-v8a, armeabi-v7a, and x86_64
- Windows `run-windows-build.ps1 -Target both` passed, including the normal and headless metadata targets
- The rebuilt debug APK installed successfully on the emulator
- D1 `test_autosave_resume_missing_pilot_unified.json5` passed 46 of 46 steps
- D2 `test_autosave_resume_missing_pilot_unified.json5` passed 45 of 45 steps
- A filtered `run_all_tests.ps1` probe selected zero runnable tests, so it is recorded as non-applicable rather than a validation pass
- `git diff --check` passed

## Result
- Aggregate D1/D2 churn changed from `+51363/-3909` to `+51207/-3909`
- `d1/main/state.c` changed from `+1375/-170` to `+1296/-170`
- `d2/main/state.c` changed from `+1879/-91` to `+1802/-91`
- The extraction removed 156 additions from upstream-owned engine files without adding hooks or changing file count

## Ranked backlog after this slice
1. Extract the nearly identical Android track-control block from both `songs.c` files, with focused next/previous/specific-track automation
2. Extract duplicated Android initialization from both `physfsx.cpp` files into a shared helper with small game-specific directory adapters
3. Extract the paired Android save/load dispatch from `gamecntl.c` after confirming the shared menu-state boundary
4. Re-audit small playsave music-source and bounds helpers before touching any binary player-file layout code
5. Consider the D2 input-demo state diagnostic, launcher thumbnail capture, and raw PhysFS compatibility adapters as isolated state follow-ups
6. Preview the paired Android HMP memory-playback path and smaller songs helpers if the main songs block proves too coupled
7. Revisit `net_udp.c` only after identifying a fresh narrow boundary; the historical queued slot-selection recommendation is no longer current

## Deferred areas
- Keep substantive escort route and coop behavior local
- Keep deterministic runtime-state serialization local
- Do not reopen the interleaved ETC2/KTX2 OGL upload path in a diff-only tranche
- Do not revisit completed newmenu slices without new branch-owned growth
- Defer automap and multiplayer state-machine extractions until their mutable-state boundaries are demonstrably narrow
