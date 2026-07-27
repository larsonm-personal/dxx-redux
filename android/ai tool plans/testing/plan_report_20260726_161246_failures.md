# Report 20260726 161246 failures

## Goal

Investigate and dispose of the two failures from the full test batch while
preserving order-sensitive state boundaries and avoiding timeout-only fixes.

## Plan

- [x] Identify the failed tests and preserve unrelated worktree changes
- [x] Inspect complete logs and adjacent-test state
- [x] Determine root cause and disposition for quick-record sidecar
- [x] Determine root cause and disposition for D1 GOG installer
- [x] Implement narrow product, helper, or test fixes
- [x] Validate each fix after representative predecessor state
- [x] Run focused quality and record results

## Findings

Both tests completed their substantive setup successfully and failed only when
the launcher automation searched for a control in the right-side launcher
pane. `SetupAutomationApi` chose one largest accessibility scroll container.
The landscape launcher has two equally sized scroll panes, so the helper could
repeatedly scroll the wrong pane and never reveal `Advanced` or
`Launch Descent 1`.

The shared helper now scrolls every accessibility scroll container tied for
the largest positive area. The individual tests retain their original
assertions and timeouts.

## Validation

- `assembleDebug` passed and the resulting APK installed successfully.
- `SetupAutomationScrollTest` passed, covering equal largest panes and
  rejection of zero-area nodes.
- `test_pilot_long_hold_delete_unified` followed by
  `test_quick_record_classic_sidecar` passed; quick-record completed 36/36
  steps, including `Advanced` and `Add to Game`.
- `test_extract.ps1` followed by `test_gog_installer_d1_unified` passed; the
  GOG test completed 28/28 steps, including `Launch Descent 1`.
- Both failing tests passed again with device rotation forced to landscape.
- Scoped code-quality checks passed.
