# Compute faster precompute dialog

## Goal

Remove the redundant manual refresh control from the Advanced precompute status,
and add a Compute faster dialog that temporarily runs precompute work at full
available CPU while open, retains the existing progress summaries, and shows the
latest eight work-log items.

## Work plan

- [x] Confirm automatic refresh behavior and current coordinator concurrency
- [x] Add a scoped full-CPU mode that restores normal background behavior on close
- [x] Add the Compute faster dialog with progress bars and eight recent log items
- [x] Add focused regression coverage for concurrency and retained log behavior
- [x] Run scoped code quality, focused tests, and the Android debug build
- [x] Rename the two user-facing labels to Foreground compute

## Status

The status section already polls its monitor once per second, so its manual
Refresh button was redundant. The full log remains on disk; only the displayed
tail is bounded.

Compute faster changes launcher route analysis from 20% to 100% CPU duty and
removes the ordinary delay between items while its dialog is composed. Opening
and closing checkpoint and restart the isolated worker at the appropriate duty.
Existing power-save, thermal, game-launch, and metadata-viewer pauses remain in
effect. The dialog reuses both progress summaries and displays the latest eight
log lines.

## Verification

- Scoped `run-code-quality.ps1 -Fix` passed
- `RouteMetadataSchedulingTest` and `RouteMetadataPrecomputeMonitorTest` passed
- Fresh `:app:assembleDebug` completed successfully
- Emulator smoke test `test_compute_faster_dialog.jsonc` passed all five steps
- Emulator coordinator log recorded 100% duty when the dialog opened and 20%
  after it was dismissed with Android Back
