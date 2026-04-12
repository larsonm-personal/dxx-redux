# Plan: D1 GOG Installer Regression

Status: Completed 2026-04-12.

## Goal

Turn the placeholder D1 GOG installer regression into a real automated test
that covers both the Windows `.exe` and Mac `.pkg` installers, using the same
launcher-plus-game structure as the D2 unified test but with D1-specific
assertions and no redbook expectations.

## This tranche

- [x] replace the D1 placeholder JSON5 with a real launcher-plus-game D1 test
- [x] add a D1 PowerShell wrapper that stages either D1 installer variant and
  runs the unified D1 script through `run_test.ps1`
- [x] verify launcher-side D1 import expectations match the real 7-file GOG D1
  extraction set for both installer formats
- [x] validate the D1 in-game flow against the real D1 menus and level-load
  behavior on the emulator
- [x] update nearby notes so the new D1 regression and its validation status
  are recorded next to the existing D2 coverage

## Validation target

- `test_gog_installer_d1_unified.json5` can run against both D1 installers via
  params rather than hardcoded paths
- a D1 wrapper script can auto-discover or accept either D1 installer path
- both D1 installer variants pass end to end on the emulator

## Validation completed

- `android/run-psscriptanalyzer.ps1 -Check` passed after adding
  `android/tests/test_gog_installer_d1_unified.ps1`
- `android/run-code-quality.ps1` now passes after fixing the SetupActivity
  indentation block and running the formatter-backed cleanup pass
- `android/tests/test_gog_installer_d1_unified.ps1 -InstallerVariant d1_windows_exe`
  passed end to end against `setup_descent_1.4a_(16596).exe`
- `android/tests/test_gog_installer_d1_unified.ps1 -InstallerVariant d1_mac_pkg`
  passed end to end against `descent_enUS_1_0_35122.pkg`
- `android/tests/test_gog_installer_d1_unified.ps1 -InstallerVariant d1_windows_exe -SkipPush`
  and `-InstallerVariant d1_mac_pkg -SkipPush` both passed again after the
  final formatting pass
- both D1 installer variants imported the same expected 7-file set and reached
  level 1 successfully through the D1-specific pilot and menu flow