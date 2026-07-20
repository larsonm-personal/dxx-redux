# Metadata dual progress bars

## Goal

Show stable mission-wide metadata analysis progress separately from the progress of the current level, so internal analysis phases do not make the primary progress indicator jump back to zero.

## Plan

- [x] Inspect the metadata progress model, checkpoint parser, and Compose UI call sites.
- [x] Add distinct overall and current-level progress values while preserving adaptive timeout activity tracking.
- [x] Render two clearly labelled, rate-limited progress indicators during metadata analysis.
- [x] Add or update focused tests for progress parsing and monotonic overall behavior.
- [x] Run targeted tests and the relevant Android build checks.

## Notes

- Preserve the existing native checkpoint cadence and timeout semantics.
- Keep unrelated working-tree changes intact.
- The overall indicator is driven by completed-level counters and never consumes task-local percentages.
- The current-level indicator continues to show detailed native phase progress, including resets when a new task begins.
- Validation passed: scoped code quality, focused metadata progress/deadline tests, Kotlin compilation, and `:app:assembleDebug` for all configured ABIs.
