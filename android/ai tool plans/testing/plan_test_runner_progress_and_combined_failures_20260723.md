# Test Runner Progress and Combined-Test Failure Plan

## Goal

Use the newest completed full-test report as historical timing input for
weighted progress estimates, and diagnose the five failures in the first full
suite run after emulator-test consolidation.

## Plan

- [x] Parse the latest report format into per-test elapsed seconds
- [x] Add a reusable report-runtime reader with focused tests
- [x] Show test index, total elapsed time, weighted completion percentage, and
  estimated remaining time before each full-suite test
- [x] Identify all five failed tests and their durable failure evidence
- [x] Fix consolidation regressions without weakening assertions
- [x] Run focused runner, catalog, quality, and emulator validation
- [x] Record validation results and any failures unrelated to consolidation

## Failure findings

- `test_quick_record_classic_sidecar`,
  `test_title_music_skip_pref_unified`, and
  `test_trine2_d1_in_d2_custom_textures` resumed SetupActivity on its
  configuration page, so their page-dependent `Descent 2` button taps failed.
- `test_gog_installer_redbook_unified` observed CD playback at 874 ms, then
  failed its immediate assertion that playback had advanced beyond 1000 ms.
- `test_validate_extract_regression_specs` found two level-pack specs that had
  not been regenerated after extraction-oracle enforcement was added.

## Validation results

- The report-runtime parser test passed against synthetic reports and parsed
  85 timed entries from `report_20260723_135454.md`.
- A filtered `run_all_tests.ps1` run matched its historical timing, printed
  `Test 1/1, 00:00:00 elapsed, estimated 100% remaining`, passed, retained its
  artifacts, and exited 0.
- Automation catalog validation passed with 35 standalone JSON tests, 18
  support scripts, and 54 PowerShell entries.
- Extraction regression validation passed for all 34 CD specs.
- Focused code quality and PowerShell parse validation passed.
- `test_quick_record_classic_sidecar` passed all 36 steps.
- Both D1 and D2 phases of `test_title_music_skip_pref_unified` passed.
- `test_trine2_d1_in_d2_custom_textures` passed all 27 steps.
- `test_gog_installer_redbook_unified` passed all 51 steps; its delayed CD
  position assertion observed 3562 ms of playback.

The extraction-spec failure was not introduced by test consolidation. Oracle
enforcement was committed after the preceding full run had already executed
its validator, leaving two older level-pack specs with null expected-file
lists. The first later full run correctly exposed them.

## Constraints

- Use the most recent completed report before the current run starts
- Fall back cleanly when a report or individual test runtime is unavailable
- Weight progress by the sum of historical test runtimes, not test count
- Exclude skipped and not-run entries from historical runtime estimates
- Preserve every assertion absorbed during test consolidation
- Keep emulator validation sequential and clear logcat before each run
