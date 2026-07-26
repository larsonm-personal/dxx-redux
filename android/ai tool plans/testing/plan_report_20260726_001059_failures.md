# Report 20260726 001059 failure investigation

## Goal

Investigate and dispose of one or two failures in
`temp/test_reports/report_20260726_001059.md`, accounting for full-batch order,
stale state, and load-sensitive behavior.

## Plan

- [x] Record the report and scope
- [x] Extract all three failure signatures and their immediate predecessors
- [x] Select one or two failures with actionable evidence
- [x] Identify product, test, state-leak, or infrastructure causes
- [x] Implement narrow fixes when supported by evidence
- [x] Validate in report-order sequences
- [x] Run scoped quality and relevant validation
- [x] Record final dispositions and verification

## Selected failures

### test_intro_skip_inputs_unified

The D1 variant passed. During D2, the result monitor remained inside a nominal
600 second timeout for about 81 minutes and produced no automation failure.
`Watch-AutomationResult` performs periodic health checks, but
`Test-EmulatorHealthy` used the unbounded `Adb` wrapper for `adb devices`.
When the ADB transport hung, the health check itself prevented the stopwatch
loop from enforcing its deadline.

Use `Adb-Timeout` for the device-list probe. This disposes the report entry as
an infrastructure failure and prevents the same hang from consuming the batch.
There is no evidence in this report of an intro-skip product failure.

Validation:

- `test_test_helpers_process_wait.ps1` passes
- Added a source guard that requires the device-list health probe to remain
  bounded

### test_obsidian_level1_objective_markers

The prior bounded route wait was placed after opening the automap. Automap
pauses game simulation, so an in-progress door transition could never complete
and the route remained at trigger 8 for the full wait.

Expose the number of active door and cloaking-wall transitions through debug
introspection. Wait for that count to reach zero before opening the automap,
then retain the exact route assertion. This synchronizes on engine state rather
than adding a fixed delay or extending the route timeout.

Validation passed in report order on the rebuilt APK:

1. `test_newmenu_render_paths_unified.json5`, both D1 and D2 variants
2. `test_obsidian_level1_objective_markers.json5`, 194/194 steps

The transition predicate waited about 1.05 seconds before the exact
`6: Reactor` assertion passed. The full 86-test batch was not rerun.

## Follow-up: test_all_extracts

- [x] Extract the complete failure and runner recovery evidence
- [x] Inspect setup-launch and extraction cleanup behavior
- [x] Classify the failure and implement a narrow fix when supported
- [x] Validate with relevant predecessor state
- [x] Run scoped quality and regression checks
- [x] Record the final disposition

### Finding

The Descent II (USA) extraction completed successfully, all five expected files
were present, and the saved oracle status remained `pass`. The failure occurred
only after `test_extract.ps1` force-stopped the responsive SetupActivity and
started it again immediately before launching automation.

An exact-case diagnostic rerun passed, but the redundant restart took about 48
seconds before automation could be sent. This explains why the same restart
failed under full-batch load. Both direct-import and file-push paths already
restart or retain SetupActivity as needed and obtain fresh set introspection
before this point.

Remove the final redundant force-stop, activity restart, and readiness wait.
Keep the already verified SetupActivity process alive for the automation launch.
This avoids process churn and a complete rescan of the extracted set rather than
increasing the readiness timeout.

### Validation and disposition

Disposition: unreliable extraction-test infrastructure, fixed.

Validation passed in report order:

1. `test_xcrash_native_report.ps1`
2. `test_all_extracts.ps1` with the exact Descent II (USA) spec selected by the
   failed report

The validation run also encountered and recovered from an ADB daemon outage
while staging track 8, preserving useful full-batch-style transport pressure.
After extraction, the test sent launcher automation one second after file
verification and reached `Ahayweh Gate`. The prior diagnostic run with the
redundant restart took about 48 seconds at the same boundary.

Additional checks passed:

- `test_extract_suite_device_preflight.ps1`
- `test_extract_regression_workflow.ps1`
- Scoped code quality
- `git diff --check`

The full 86-test batch was not rerun.

## Follow-up: clean launch boundary review

- [x] Trace why the final force-stop and SetupActivity restart were introduced
- [x] Inventory state that can survive extraction and affect launch automation
- [x] Decide whether to restore the process boundary or replace it explicitly
- [x] Implement and validate the corrected clean-launch approach
- [x] Update the final disposition

### Revised finding

The force-stop and restart were present in the original per-disc test and were
explicitly described as providing a clean launch. Although later file-push
changes added an earlier restart so Java could observe shell-created files, the
final boundary still prevents cached launcher data, import work, or a stale
launcher automation executor from leaking into game verification.

Restore the final process boundary. Handle its rare infrastructure failure in
`test_all_extracts.ps1`: on child exit code 98, run the existing full
launcher/emulator recovery and rerun the entire extraction spec once. A second
infrastructure failure remains terminal. This preserves clean-state coverage,
does not extend a timeout, and does not resume halfway through a failed spec.

### Corrected validation

The report-order sequence passed with the clean boundary restored:

1. `test_xcrash_native_report.ps1`
2. `test_all_extracts.ps1` with the exact Descent II (USA) spec

The extraction run recovered from a track 11 staging mismatch, restarted
SetupActivity before automation, reached `Ahayweh Gate`, and reported
`Attempts=1`. The restart was responsive in this run. The workflow regression
test now guards both the clean process boundary and the one-retry limit.

Final disposition: preserve the clean launch restart and recover a single
infrastructure failure by restarting from the beginning of the extraction spec.
