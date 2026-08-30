# Estimated level progress integer percent

## Goal

Render `Estimated level progress` as a whole-number percentage because the value is intentionally approximate.

## Plan

- [x] Locate every metadata-analysis rendering path and its relevant tests
- [x] Change only the estimated-level display to omit decimal places
- [x] Add or update focused regression coverage
- [x] Run scoped formatting and the Android debug build
- [ ] Run the focused unit test after the unrelated lobby test source compiles

## Findings

- The metadata analysis dialog has one shared rendering path for its overall, estimated-level, and current-task bars.
- Only the estimated-level row now uses whole-percent text. Integer truncation keeps an unfinished `999/1000` estimate at `99%` instead of displaying completion early.

## Validation

- Scoped repository quality checks passed.
- `:app:assembleDebug` passed for arm64-v8a, armeabi-v7a, and x86_64, including the native CMake stages.
- The focused `MetadataLoadProgressTest` run could not compile the shared unit-test source set because the pre-existing `LobbyMissionRefreshTest.kt:38` passes a `Long` to a lobby API that currently expects a `String`.
