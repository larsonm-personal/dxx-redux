# Plan: test_abort_game_to_main_menu_d2 follow-up from report 20260523_114346

- [x] Create tranche plan file and capture the failing report context
- [x] Inspect the failing report section and current failing log
- [x] Inspect the owning test script and one nearby baseline to form a local hypothesis
- [x] Apply the smallest fix in the owning launcher or test path
- [x] Run focused validation for `test_abort_game_to_main_menu_d2`
- [x] Update this note with the outcome and any remaining blockers

## Notes

- Report under review: `C:\local\dxx-redux\temp\test_reports\report_20260523_114346.md`
- Current failure: `test_abort_game_to_main_menu_d2` exits after about 42s
- The failing log reaches launcher script step 5 (`tap_button`) but shows no successful tap or later steps, so the first working hypothesis is a launcher-side selection/blocking issue before the game ever launches
- Root cause: the script assumed D2 was already the selected launcher game after `reset_state`; under the current launcher state that assumption is not reliable, so `tap_button "Launch Descent 2"` can stall before the game launches
- Fix: add an explicit `{"action": "tap_button", "text": "Descent 2", "post_delay_ms": 1000}` before `Launch Descent 2` in `test_abort_game_to_main_menu_d2.json5`
- Validation:
	- `android/run_test.ps1 -ScriptName test_abort_game_to_main_menu_d2.json5`
	- `android/run_all_tests.ps1 -Filter test_abort_game_to_main_menu_d2`
- Outcome: both validations now pass; the filtered suite wrapper reports `PASS (00:32)` with `Passed: 1 Failed: 0 Timeouts: 0` and writes `C:\local\dxx-redux\temp\test_reports\report_20260523_130856.md`