# Plan: D1/D2 Songs Diff Extraction 2026-07-10

## Goal
- Reduce upstream-owned D1/D2 churn by moving the duplicated Android music control implementation from both `songs.c` files into one shared implementation

## Existing work to preserve
- Preserve the completed state metadata extraction in `state_android_shared.{c,h}` and both `state.c` files
- Preserve the pre-existing mission metadata edits in `game_data/mission_files/af-d2x.json` and `game_data/mission_files/anachron.json`
- Preserve all unrelated working-tree changes

## Steps
- [x] Audit the exact D1/D2 songs block, call sites, globals, headers, and existing shared music surfaces
- [x] Define the narrowest shared API that keeps game-specific state and upstream code changes minimal
- [x] Move only behavior-equivalent Android-owned implementation and retain mirrored lightweight hooks
- [x] Run scoped formatting and inspect the resulting upstream diff reduction
- [x] Build Android D1/D2 for all configured ABIs and build both Windows targets
- [x] Run focused emulator automation covering Android music control in both games
- [x] Record validation, before/after metrics, deferred code, and the next candidate

## Initial guardrails
- Do not change desktop redbook behavior
- Do not extract original cross-platform music logic solely to deduplicate D1 and D2
- Do not combine this work with player-file format, playlist policy, or launcher redesign
- Keep D1 and D2 hooks symmetric unless a documented engine difference requires otherwise
- Prefer existing JNI, JSON, and shared helper interfaces over new wrappers

## Baseline
- Aggregate D1/D2 diff after the state metadata tranche: `+51207/-3909`
- Expected candidate: the nearly identical `__ANDROID__` track-control block around the latter half of both `songs.c` files
- D1 baseline: `songs.c +427/-6`, `songs.h +12/-1`
- D2 baseline: `songs.c +450/-6`, `songs.h +12/-1`

## Audit result
- The D1 tail contained 369 lines and the D2 tail contained 373 lines
- The implementations were identical except for one four-line D2 comment
- The soundtrack-preference global and accessors were also identical and belonged with the same shared Android API
- D2-only normal-playback overlay hooks and the differently placed D1/D2 breadcrumbs were intentionally left local
- `BIMSongs` and `Num_bim_songs` already had external linkage; only `Song_playing` required an Android-only linkage seam

## Implementation
- Added `songs_android_shared.{c,h}` and linked the implementation into both Android game targets
- Moved the shared next, previous, specific-track, track-info, track-list, JSON escaping, redbook navigation/resume, and soundtrack-preference code into that implementation
- Kept `Song_playing` file-static on desktop builds and link-visible only for Android builds
- Removed the Android API declaration blocks from both upstream `songs.h` files and routed Android callers through the shared header
- Kept both the engine and Android headers in `playsave.c` because it uses original `songs_set_volume` plus the shared preference API
- Added a debug-only `music_control` automation action that executes on the game thread
- Added parsed `music.tracks` data to introspection so track-list generation is directly testable
- Added `test_music_track_controls_unified.json5` for maintained D1/D2 built-in music coverage

## Result
- Aggregate D1/D2 diff changed from `+51207/-3909` to `+50427/-3909`
- Net reduction: 780 additions in upstream-owned D1/D2 files
- D1 `songs.c` changed from `+427/-6` to `+49/-6`
- D2 `songs.c` changed from `+450/-6` to `+68/-6`
- Each `songs.h` changed from `+12/-1` to `+1/-1`
- The two required shared-header includes in `playsave.c` account for the two additions between the gross 782-line songs reduction and the net 780-line result

## Validation
- Scoped clang-format, CMake format/lint, and UTF-8 BOM checks passed
- `git diff --check` passed
- Android `:app:externalNativeBuildDebug` passed for D1 and D2 on arm64-v8a, armeabi-v7a, and x86_64
- Windows `run-windows-build.ps1 -Target both` passed for normal and headless targets
- The rebuilt debug APK installed successfully on the emulator
- D1 `test_music_track_controls_unified.json5` passed 36 of 36 steps
- D2 `test_music_track_controls_unified.json5` passed 35 of 35 steps
- A focused follow-up against the staged three-track SAF redbook fixture passed 8 of 8 steps, covering list enumeration, specific-track selection, next, and previous
- The older full `test_saf_redbook.json5` flow was not counted as a pass because its optional `player` selection partially matched `Multiplayer` and failed before its existing in-level assertions; that unrelated script was left unchanged

## Deferred behavior
- Custom jukebox navigation is compile-validated but still lacks a focused emulator fixture
- D1 redbook control lacks a maintained disc fixture; the shared D2 fixture exercised the common implementation
- Buffer hardening for caller-supplied track-list and track-info buffers remains outside this behavior-preserving extraction
- Original desktop music logic, D2-only overlay hooks, RNG changes, and player-file layout code remain local

## Next recommended tranche
1. Extract duplicated Android initialization from both `physfsx.cpp` files into a shared helper with small game-specific directory adapters
2. Re-audit the paired Android save/load dispatch in `gamecntl.c`
3. Consider the isolated state diagnostics and compatibility adapters identified in the expansive survey
