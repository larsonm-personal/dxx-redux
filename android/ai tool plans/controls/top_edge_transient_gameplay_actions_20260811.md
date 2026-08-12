# Top-edge transient gameplay actions

## Goal

Move transient gameplay action buttons away from the active play area and onto
the top edge so they are less likely to be pressed accidentally.

## Plan

- [x] Audit the Android game overlays for temporary high-impact action buttons
- [x] Move the coop warp and mid-game join acceptance actions to non-overlapping top-edge positions
- [x] Add focused geometry coverage for the shared top-edge placement
- [x] Run scoped code quality, focused tests, and an Android build
- [x] Record the completed validation here

## Audit notes

- `WarpButtonOverlay` is a temporary in-play action currently centered along the left side
- `AcceptJoinButtonView` is the only explicit mid-game join decision button; an unanswered request is rejected by the engine timeout
- `ExitButtonView` and `SkipButtonView` are already on the top edge
- `StartGameButtonView` is bottom-centered, but appears only on the pre-game player-selection screen and is not an in-play nuisance-press risk

## Validation

- `android/run-code-quality.ps1 -Fix` passed for the changed Kotlin, test, and plan files
- `android/gradlew.bat testDebugUnitTest --tests com.dxxredux.app.TopEdgeActionButtonLayoutTest` passed
- `android/gradlew.bat assembleDebug` passed, including CMake builds for `arm64-v8a`, `armeabi-v7a`, and `x86_64`
