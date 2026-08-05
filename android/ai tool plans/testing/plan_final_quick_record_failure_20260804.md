# Final quick-record failure investigation

## Goal

Determine why `test_quick_record_classic_sidecar` was the only failure in the
latest full batch, including predecessor-state effects, and apply a durable fix
or document a justified disposition.

## Plan

- [x] Create the investigation plan
- [x] Read the detailed report and failure artifacts
- [x] Inspect test setup, cleanup, and recent related changes
- [x] Reproduce with relevant predecessor state
- [x] Implement a narrow fix if the evidence identifies one
- [x] Validate the full test and an ordering-sensitive sequence
- [x] Run scoped quality checks and record the result

## Result

The demo, RNG trace, classic sidecar, and staged metadata all succeeded in the
batch failure. The launcher failed afterward because `Advanced` was below the
viewport and scroll automation scanned only through the highest Compose
semantics ID plus 500. Accessibility provider virtual IDs are not guaranteed
to fit that derived range, so the automation found no actionable scroll node.

Scroll discovery now covers at least the established provider range of 16383
without widening ordinary button scans. The test also no longer asserts that
`Advanced` is already visible before using the scrolling `tap_button` action.
The top reset recognizes an opposite-direction accessibility action as proof
that it has reached the boundary instead of repeating fallback gestures. No
timeout was increased.

Validation completed:

- Automation catalog validation passed
- Focused `SetupAutomationScrollTest` passed under JDK 21
- Debug APK assembly passed
- Quick-record test passed 35/35 from accumulated emulator state
- Exact two-game predecessor passed, followed immediately by another 35/35
  quick-record pass
- Final on-device logs confirmed the top boundary, an accessibility scroll
  with `scrolled=true`, and the `Advanced` tap in about 1.9 seconds
- Scoped code quality checks passed
