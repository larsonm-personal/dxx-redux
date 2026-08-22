# Music editor CD source visibility plan

Goal: make the in-game music editor identify active MacPlay CD audio correctly and include CD audio in its source selector without changing the correct track list.

- [x] Trace source option construction and active-source index resolution
- [x] Add focused regression coverage for CD source visibility and selection
- [x] Implement the smallest source-list/state fix
- [x] Run scoped code quality, focused tests, and the relevant CMake/build verification
- [x] Record validation results and mark the plan complete

Validation:

- `MusicOverlaySourcesTest`: passed, including active-file-set MacPlay CD coverage
- Scoped `run-code-quality.ps1 -Fix`: passed
- `:app:assembleDebug`: passed with CMake builds for arm64-v8a, armeabi-v7a, and x86_64
- `git diff --check`: passed

## Live-test follow-up

- [x] Inspect the connected app's active file set, CD registry, and generated playlist
- [x] Add Game Logs diagnostics for source-option admission and rejection
- [x] Reproduce the missing admission decision with an active-CD/no-registry fixture
- [x] Correct the remaining availability mismatch and extend regression coverage
- [x] Run focused tests, code quality, APK/CMake build, and emulator installation

Follow-up validation:

- Confirmed the connected emulator has no CD registry, so it cannot reproduce the physical MacPlay data path
- Added coverage proving an active native CD source remains in the selector without a launcher registry
- `MusicOverlaySourcesTest`: passed
- Scoped `run-code-quality.ps1 -Fix`: passed
- `:app:assembleDebug`: passed with CMake builds for arm64-v8a, armeabi-v7a, and x86_64
- Installed the rebuilt APK successfully on the connected emulator without clearing app data
