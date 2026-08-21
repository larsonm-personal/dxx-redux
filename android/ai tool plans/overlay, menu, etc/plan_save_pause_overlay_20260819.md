# Plan: Save pause and overlay status

## Goal

Make single-player pause state obvious and directly reversible from the Android
overlay, and prevent a completed save from leaving gameplay paused.

## Work

- [x] Trace save, pause-window ownership, and overlay rendering/control paths
- [x] Add a clear paused indicator and direct overlay resume behavior
- [x] Remove the unintended post-save pause using the narrowest native API path
- [x] Add or extend high-level regression coverage
- [x] Run scoped formatting and relevant tests
- [x] Run Android native builds for all configured ABIs and the D1 host CMake build
- [ ] Run the full D2 host CMake target set
  - Blocked outside this change by missing `ReadConfigFile` and `GameCfg` symbols
    in `input_demo_headless_main.cpp`
- [ ] Mark the outstanding bug complete
  - `android/outstanding_bugs.md` explicitly says automated tools must not edit it

## Verification

- Scoped `android/run-code-quality.ps1 -Fix`: passed
- `OverlayVisibilityPolicyTest`: passed
- Android CMake builds: passed for arm64-v8a, armeabi-v7a, and x86_64
- `test_android_saveload_dispatch_unified.jsonc`: passed all 46 steps in D1 and D2
- D1 Windows CMake build: passed
- D2 Windows CMake configure: passed; unrelated headless target compile failed as noted above
