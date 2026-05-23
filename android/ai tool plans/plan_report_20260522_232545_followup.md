# Plan: report 20260522_232545 follow-up

- [x] Create tranche plan file and capture the new full-run report path
- [x] Inspect the failed sections from `temp/test_reports/report_20260522_232545.md`
- [x] Decide whether the current `test_all_extracts` failure is the same preview-disc limitation or a new regression
- [x] Reproduce the narrowest failing slice locally
- [x] Apply the smallest fix in the owning path
- [x] Run focused validation for the touched slice
- [x] Update this note with the outcome and any remaining blockers

## Notes

- Report under review: `C:\local\dxx-redux\temp\test_reports\report_20260522_232545.md`
- New full run reports three failures: `test_input_demo_runtime_smoke`, `test_dpad_triggers`, and `test_all_extracts`
- Immediate focus was `test_all_extracts`, to determine whether it was still the preview-demo limitation or a new source
- Result: this `test_all_extracts` failure was a different regression from the preview-demo case. The report failed on `Descent I and II - The Definitive Collection (Europe) (Disc 2)` with `classification = d2_full`, `game = d1d2`, and startup crash
- Root cause in `android/tests/test_extract.ps1`: readiness already normalized `d1d2` to `d2` for D2-classified specs, but the actual launch still used `if ($spec.game -match 'd1') { 'd1' } else { 'd2' }`, which routed every `d1d2` spec through the D1 launch path
- Fix: launch now reuses the already-normalized `gameKey`, so D2-classified `d1d2` discs load the D2 engine and D1-classified discs still load D1
- Focused validation passed:
- `android/tests/test_extract.ps1` for `Descent I and II - The Definitive Collection (Europe) (Disc 2)` now reaches `Ahayweh Gate` and exits 0
- `android/tests/test_all_extracts.ps1 -SpecPaths` for that exact spec now passes 1/1 and exits 0
- Shared-path validation passed:
- `android/tests/test_all_extracts.ps1 -SpecPaths` for the Europe and USA Disc 2 specs now passes 2/2 and exits 0
- Remaining report items not addressed in this tranche:
- `test_input_demo_runtime_smoke`: host build failure before runtime smoke executes
- `test_dpad_triggers`: D2 assertion failure for `joystick_controls.items[50].value`, expected `150`, got `255`
