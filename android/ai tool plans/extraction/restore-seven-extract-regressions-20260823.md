# Restore seven extraction regressions 2026-08-23

## Plan

- [x] Map each changed spec to its detailed failing run and diagnostics
- [x] Group failures by runner, importer, or automation root cause
- [x] Add focused regression coverage for each confirmed runner defect
- [x] Implement the smallest fixes without weakening extraction assertions
- [x] Run focused host tests and representative emulator extraction cases
- [x] Confirm all seven specs record natural passing evidence and leave the diff
- [x] Record results and any remaining infrastructure limitations

## Findings

- The four file-only failures were stale results recorded before managed-content logical paths were included in extraction verification. Fresh runs passed without weakening their file assertions.
- The older D2 Alt import timeout was an ADB daemon outage being flattened into an empty result. Direct ADB helpers now promote timeouts and transport failures to retryable infrastructure errors. Its fresh full run passed after staging recovery.
- Automation results and persisted automation state could leak between test processes. Generated runs now carry a unique run ID, reject mismatched result files, and remove persisted automation state during sanitization.
- The extraction runner embedded the shared automation template directly and never applied the normal test runner's `when` filtering. D1 therefore executed D2-only menu selections. `Get-ExtractAutomationScriptText` now removes steps for the other game and strips `when` from retained steps before staging the generated script.
- Destination Saturn then showed that a raw Enter action did not activate D1's starting-level input. The D1 step now selects its visible `1` value through the menu-aware selector.
- A slow cleanup sequence could reach the canary with `default` active even after the earlier switch. Sanitization now reasserts `regression_test` after all asynchronous setup cleanup commands.
- Destination Saturn's picker label does not match its internal `expected_mission`. Optional mission selection now uses `select_mission` with an empty target, selecting the current picker entry when present and using the engine's existing start-level-menu skip when no picker is shown. Required mission selection still uses the exact expected mission.
- Dependency updates had left simple Gradle values single-quoted in the shared Java Properties and shell manifest. Safe assignment formatting now leaves simple values unquoted and quotes only query-string punctuation. This restored the APK build needed for the emulator reruns.

## Verification

- All seven requested specs now contain a passing result: four `file_only` and three `full`.
- `git diff --name-only` reports zero of the seven requested specs as changed.
- `android/tests/test_extract_regression_workflow.ps1`: pass.
- `android/tests/test_get_deps_runtime_updates.ps1`: pass.
- Scoped `android/run-code-quality.ps1 -Fix`: pass.
- Debug APK build and install: pass (`BUILD SUCCESSFUL in 5m 58s`).
- Emulator instability remains environmental: ADB restarted during one large CD staging operation, and an unrelated package install killed SetupActivity once. The runner recovered the former and no infrastructure failure remains recorded in the requested specs.
