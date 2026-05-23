# Plan: GOG Installer Variant Coverage

Status: Completed 2026-04-12 for installer-variant test coverage cleanup.

## Goal

Make the unified D2 GOG installer regression test work with either the Windows
`.exe` installer or the Mac `.pkg` installer, and make every current D2-exe
test path explicitly carry the equivalent D1 exe, D2 pkg, and D1 pkg paths so
all four installer variants are represented wherever the D2 exe is referenced.

## This tranche

- [x] parameterize the D2 unified JSON5 test so it can import either D2 GOG
  installer format without duplicating the test logic
- [x] update the D2 unified PowerShell wrapper so it can auto-discover or
  accept either D2 installer variant and push the correct device-side path
- [x] add the cross-platform data-equivalence note to the D2 test and the
  future D1 test file comment block
- [x] add a placeholder D1 installer test file comment block for the later
  D1-specific regression phase
- [x] survey every current D2-exe-only path and add the matching D1 exe, D2
  pkg, and D1 pkg paths so all four variants are listed together

## Validation target

- the D2 unified test resolves the installer path through script params rather
  than a hardcoded `.exe` path
- the wrapper can run the D2 unified test against either installer variant
- every current D2-exe reference in active test and plan files carries all four
  installer paths together for coverage planning

## Validation completed

- `Resolve-TestScript` now resolves the unified D2 test to
  `/data/local/tmp/setup_descent_2_1.1_(16596).exe` for `d2_windows_exe` and
  `/data/local/tmp/descent_2_enUS_1_0_51877.pkg` for `d2_mac_pkg`
- `android/run-psscriptanalyzer.ps1 -Check` passed after reindenting the
  updated wrapper
- `android/app/src/main/java/com/dxxredux/app/AssetManifest.kt` now treats
  launcher-side hashing races as non-fatal, which prevents SetupActivity from
  crashing if `clear_set` deletes a file while the startup hash pass is still
  reading it
- `android/app/src/main/java/com/dxxredux/app/SetupActivity.kt` now bumps the
  launcher `refreshTrigger` after broadcast-driven set mutations (`create_set`,
  `switch_set`, `clear_set`, `import_gog`, `import_sow`, `import_cd`) so the
  Compose button enabled state stays in sync with `setup_introspect.json`
- a repository survey of `setup_descent_2_1.1_(16596).exe` usage under
  `android/` showed only the unified D2 test, its wrapper, and active GOG plan
  notes, and those files now list the D1 exe, D2 pkg, and D1 pkg alongside the
  D2 exe
- `android/tests/test_gog_installer_redbook_unified.ps1 -InstallerVariant d2_mac_pkg`
  now passes end to end against `descent_2_enUS_1_0_51877.pkg`
- `android/tests/test_gog_installer_redbook_unified.ps1 -InstallerVariant d2_windows_exe`
  still passes end to end against `setup_descent_2_1.1_(16596).exe`
- `android\gradlew.bat :app:installDebug` completed successfully after the
  launcher fixes, confirming the Kotlin changes compile and install cleanly
- full `android/run-code-quality.ps1` still reports an unrelated pre-existing
  formatting issue in `android/app/src/main/cpp/extract/mac_hfs_extract.c`