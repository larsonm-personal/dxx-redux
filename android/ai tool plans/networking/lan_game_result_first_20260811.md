# LAN game result-first layout

## Goal

Place discovered LAN games at the top of the multiplayer LAN view so the common
single-game path is visible and quick to join without scrolling.

## Plan

- [x] Trace the LAN discovery screen layout and existing UI coverage
- [x] Move discovered games ahead of secondary host and direct-join controls
- [x] Confirm the project has no Compose UI test harness for direct layout coverage
- [x] Run scoped formatting, relevant tests, and the required CMake build
- [x] Record verification results and mark this plan complete

## Notes

- Preserve existing unrelated changes in `android/outstanding_bugs.md` and the
  untracked coop death-spew plan

## Verification

- Scoped `android/run-code-quality.ps1 -Fix`: passed
- Android `:app:testDebugUnitTest`: passed
- Android `:app:assembleDebug`: passed, including CMake builds for arm64-v8a,
  armeabi-v7a, and x86_64
- Windows build wrapper: attempted, but D1 is blocked in unchanged
  `android_save_set.c` because MSVC does not define `PATH_MAX`
- `git diff --check`: passed
