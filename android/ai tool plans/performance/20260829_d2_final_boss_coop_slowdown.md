# D2 final boss coop slowdown

## Goal

Identify the cause of the Android slowdown recorded in
`debuglog_20260829_101315.txt`, implement the smallest safe fix, and verify it
without regressing host builds or tests.

## Plan

- [x] Extract profiling captures and rank the worst simulation, render, GPU,
  network, and frame-gap events
- [x] Correlate expensive frames with object/projectile types and relevant D2
  final-boss or Earthshaker code paths
- [x] Implement a focused optimization or add targeted diagnostics if the log
  cannot isolate the cost safely
- [x] Run scoped formatting, tests, and the Windows CMake build
- [x] Record results and mark this plan complete

## Findings

- Level 24 severe frames were CPU-render bound. The worst captured frame spent
  279137 us rendering and 859 us on the GPU while drawing 346 objects
- Busy frames issued 550 to 900 texture binds and reported zero reuse because
  commit `0498798f` disabled the old scalar bind cache on 2026-08-11
- Earthshaker children amplify the regression by adding visible polygon models,
  but missile simulation and network processing were not the primary costs
- The replacement cache tracks bindings independently for texture units 0, 1,
  and 2. Merged-wall rendering now updates the same state, preserving the
  cross-unit correctness requirement that motivated the cache disablement

## Validation

- Scoped `android/run-code-quality.ps1 -Fix`: passed
- `python -m unittest android.tests.test_android_renderer_contracts`: 7 passed
- `android/gradlew.bat :app:assembleDebug`: passed for arm64-v8a,
  armeabi-v7a, and x86_64
- `android/tests/test_native_host_unit_tests.ps1`: D1 and D2 Windows builds
  passed; all 44 configured D2 CTests passed; the D1 build currently exposes no
  CTest entries
