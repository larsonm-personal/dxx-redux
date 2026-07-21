# Active debug log export visibility

## Goal

Make the launcher Advanced tab list and export the current debug log immediately,
without requiring an application relaunch, while preserving bounded logging cost.

## Plan

- [x] Trace debug-log file creation, buffering, launcher listing, and export refresh
- [x] Reproduce the one-file-behind behavior with a focused launcher or unit test
- [x] Flush and refresh only at user-visible listing/export boundaries
- [x] Add regression coverage for exporting the active log
- [x] Build Android all ABIs, run focused tests and code quality, and document the fix

## Findings and implementation

- The file writer already flushes individual lines and completed profiling
  batches. The stale behavior was in the Advanced page, which retained a file
  list snapshot and had no direct refresh action.
- While the Debug Logging section is visible, it now rescans the bounded log
  directory once per second. A path, length, and modification-time fingerprint
  avoids recomposition when nothing changed and detects both newly created logs
  and growth of the active log.
- A `Refresh` button performs the same scan immediately. Listing first flushes
  the launcher process writer, but game logging remains batched and unchanged.
- The fingerprint unit test passed. The launcher integration scenario opened
  Advanced, found and activated Refresh, and passed 5/5 steps.
- Android `assembleDebug` passed for arm64-v8a, armeabi-v7a, and x86_64. Scoped
  code quality and `git diff --check` passed.
