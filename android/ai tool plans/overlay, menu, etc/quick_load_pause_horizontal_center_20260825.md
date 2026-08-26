# Quick-load and pause overlay horizontal centering

## Goal

Center the Kotlin quick-load prompt and paused indicator on the same horizontal
display axis, including landscape devices with asymmetric system insets.

## Plan

- [x] Trace the prompt window and paused-indicator positioning paths
- [x] Make both overlays use the full display's horizontal center
- [x] Add or extend focused layout-policy coverage
- [x] Run scoped formatting, Kotlin tests, and the required build validation

## Findings

- The paused indicator is centered in `TouchOverlayView`'s full canvas.
- The quick-load `Dialog` uses Android's default centered window gravity, which
  centers within the inset-adjusted application area and can shift horizontally
  on landscape devices with asymmetric side insets.
- The prompt now uses a full-display, edge-to-edge dialog root and centers its
  card within that root. Both overlays also share one width ratio.

## Validation

- `run-code-quality.ps1 -Fix` passed for all touched Kotlin, test, and plan files
- `testDebugUnitTest --tests com.dxxredux.app.QuickSaveLoadActionTest` passed
- `assembleDebug` passed, including CMake builds for arm64-v8a, armeabi-v7a,
  and x86_64
