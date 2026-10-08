# Graphics confirmation frame flash

Trace the main-view draw and EGL presentation boundary around confirmation preparation. Verify whether a frame skipped by graphics safety is nevertheless presented, then retain the last complete frame while preparation/rollback blocks drawing

1. Add targeted skipped-draw presentation diagnostics and reproduce on an isolated emulator
2. Fix the shared Android presentation path, preserving the current immediate apply and confirmation deadlines
3. Extend the existing Video Info regression to reject incomplete presentations; build and test both engines

Preserve the concurrent derived-pause work in the shared coordinator and engine files

## Findings and implementation

- Both engines skip `game_render_frame()` when graphics confirmation is preparing, but their event loop still calls `gr_flip()`
- Instrumented D2 reproduction recorded four successful EGL swaps of skipped main views between 10:45:13.074 and 10:45:13.180, immediately after confirmation preparation began
- Added a shared EGL presentation gate that retains the last complete frame when graphics safety skips the draw; immediate application and both confirmation deadlines are unchanged
- Exposed incomplete/withheld presentation counters through existing graphics introspection and extended the Video Info regression assertions
- Regression check against the diagnostic build fails as intended: expected zero incomplete presentations, observed two
- Scoped formatting and compilation of both changed native units for both engines pass

## Validation

- Native D1/D2 CMake/Ninja targets pass after the concurrent input-file repair
- Full APK assembly remains blocked by concurrent Kotlin output directory access (`Unable to delete ... compileDebugKotlin/classes`)
- For device validation, replaced both engine libraries in the successful diagnostic APK and re-signed that temporary test APK; Kotlin code is unchanged from the diagnostic reproduction
- D1 confirmation probe passes with zero incomplete presentations
- The first full D1 edit run missed the 2.5-second live phase: the controller action consumed 2979 ms before its next assertion. Failure state still reports zero incomplete presentations and three withheld frames
- Retry passes both complete Video Info regressions: D1 probe 37/37 and edits 85/85; D2 probe 39/39 and edits 87/87. Covers immediate live application, coalesced edits, accept, cancel, Back, timeout rollback, baseline/all-off, and zero incomplete presentations
- D1 final edit state records 24 withheld presentations, zero incomplete presentations, and an idle safety coordinator
- Logs: `android/temp/graphics-flash-after-retry.log` and `android/temp/graphics-video-overlay-20261007-105627/`
- Automation catalog validation passes; master catalog discovery timed out at its existing 45-second limit

Status: complete. Both native engines build and the fixed libraries pass emulator regression in the diagnostic APK; full Gradle APK assembly remains unverified due to the unrelated concurrent Kotlin output-directory lock
