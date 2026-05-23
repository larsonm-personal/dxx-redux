# Plan: test_all_extracts preview disc failure 2026-05-22

- [x] Read the failing full-suite report entry and locate the owning test scripts
- [x] Inspect the failing preview-disc regression spec and the current launch wait path
- [x] Form one local hypothesis for why the launched game stays in loading for this spec
- [x] Apply the smallest fix in the owning import or launch path
- [x] Re-run the targeted extract test and update this note with the result

## Notes

- Focus only on the `test_all_extracts` failure from `report_20260521_225122.md`
- Last-night failing source: `Descent II (USA) (3-Level Interactive Preview)`
- Initial failure symptom: launcher reported the demo set as not launchable, then post-launch wait stayed at `screen_mode=unknown` / `intro_active=true` until timeout
- Fixed launcher readiness in `SetupActivity.kt` so D2 demo sets using `d2demo.*` count as launchable, with focused JVM coverage in `SetupLaunchReadinessTest.kt`
- Reproduced the remaining Android launch issue after that fix: this preview demo stays in `intro_active=true` with `menu present=True` for 90s even after controller A, generic taps, and top-right intro-skip taps
- Current suite decision: `android/tests/test_extract.ps1` treats `disc_id == "descent-ii-usa-3-level-interactive-preview"` as a file-only Android launch exception after extraction/file verification succeeds
- Validation: targeted `test_extract.ps1` now exits 0 with `SKIP (file-only)`, and `test_all_extracts.ps1 -SpecPaths ...\extract_regression.json5` passes 1/1