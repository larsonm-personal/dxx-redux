# Fix Android Upload Build 2026-04-28

## Goal

Restore the Android upload/build flow by fixing the current native build breakage reported by `android\0_upload_to_test.ps1`.

## Steps

- [x] Inspect the first concrete compiler stop and identify the local root cause.
- [x] Apply the smallest fix for the D1 native compile failure.
- [x] Re-run the failing Android-native build path and capture the next blocker, if any.
- [x] Continue until the reported upload build path reaches a clean build or a clearly narrower remaining blocker.

## Result

- Restored the clobbered D1 replay-exit/help block in `d1/main/game.c`.
- Added the missing replay helper include in `d2/main/laser.c`.
- Verified `:app:buildCMakeDebug[arm64-v8a]-2` passes.
- Verified `android\1_build-aab.ps1 -BuildType 3` passes and produces an internal AAB.