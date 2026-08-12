# Touch editor reticle preview

## Goal

Show a representative Descent reticle at the game viewport center in the touch layout editor so players can avoid covering it with controls.

## Plan

- [done] Inspect the editor canvas, viewport geometry, existing center marker, and tests
- [done] Decide whether configured reticle data is available without duplicating game settings logic
- [done] Implement the smallest D1/D2-style reticle preview in shared launcher code
- [done] Add or extend focused tests for placement and rendering behavior
- [done] Run scoped formatting, tests, and the required build verification

## Notes

- Prefer launcher-owned drawing over changes in `d1/` and `d2/`
- The preview is a placement guide and should not behave like an editable touch control
- Exact configured reticles would require bridging the native pilot configuration and classic bitmap assets into the launcher
- The preview uses the identical D1/D2 OpenGL classic-reboot geometry as a scalable approximation of the classic reticle

## Verification

- Scoped `run-code-quality.ps1 -Fix` passed
- `:app:testDebugUnitTest --tests com.dxxredux.app.TouchEditorZoneEdgeTest` passed
- `:app:assembleDebug` passed, including CMake builds for arm64-v8a, armeabi-v7a, and x86_64
