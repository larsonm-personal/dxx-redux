# Report 20260727 215931 failures

## Goal

Investigate and dispose of the four failures from the full test batch without
weakening assertions, extending timeouts as a substitute for correctness, or
relying on isolated passes.

## Plan

- [x] Read repository instructions and create this plan
- [x] Inspect complete failure logs and relevant predecessor state
- [x] Group failures by root cause and choose dispositions
- [x] Implement shared or narrowly scoped fixes where justified
- [x] Validate fixes with representative ordering and state
- [x] Run scoped quality checks and record results

## Findings and dispositions

### test_extract_all_cds_batch

The production batch script recently began calling
`Test-ExtractionCompletionManifest` and `Publish-ExtractionDirectory`, but its
isolated helper fixture did not implement either function. The fixture also
did not create an extracted file, which the strengthened production script now
requires before publication. The fixture now models all three parts of that
contract.

### test_fingerprint_audio_enumeration

The native fingerprint tool emits UTF-8 JSON. The Windows test harness used
the process API's default redirected-output encoding, corrupting the Unicode
filename before JSON validation. The harness now explicitly decodes stdout and
stderr as UTF-8.

### test_pilot_long_hold_delete_unified

The report contains an empty two-byte log after about twelve minutes, with no
test or device diagnostic to identify a failed assertion. The test passed for
both D1 and D2 after its original graphics-test predecessor and passed again
before quick-record with rotation forced. No test assertion or timeout was
changed. This run is disposed as an unconfirmed emulator or adb infrastructure
hang; a repeated failure needs preserved device diagnostics before a code
change is justified.

### test_quick_record_classic_sidecar

The earlier accessibility fix was incomplete. When Compose exposes no
scrollable accessibility node in the landscape launcher, the fallback swipe
used the screen center, which is the divider between the two independent
panes. The shared fallback now swipes the center of both landscape panes while
retaining a single centered swipe in portrait. The test keeps its original
`Advanced` and `Add to Game` assertions and timeout.

## Validation

- `test_extract_all_cds_batch.ps1` passed.
- `test_fingerprint_audio_enumeration.ps1` passed, including its Unicode case.
- `SetupAutomationScrollTest` passed with coverage for both landscape pane
  centers and the portrait center.
- `assembleDebug` passed and the resulting APK was installed.
- The original device order
  `test_ogl_runtime_texture_options_unified`,
  `test_pilot_long_hold_delete_unified`,
  `test_quick_record_classic_sidecar` passed.
- A second pilot-to-quick-record sequence with rotation forced passed.
- Quick-record completed 36/36 steps, including `Advanced` and `Add to Game`.
- Scoped code-quality checks passed.
