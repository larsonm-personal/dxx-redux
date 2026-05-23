# Fix Test Suite -- Multiple Failures and Improvements

## Status: COMPLETE

---

## Phase 1: Not-standalone marker system  [x]
Add `"_standalone": false` to `_info` in support/template JSON5 scripts.
Update runners to skip/mark them.

### JSON5 scripts to mark:
- test_lan_mp.json5 (support for run_lan_test.ps1)
- test_autoselect_crash_repro.json5 (support for test_autoselect_crash.ps1)
- test_controller_compare.json5 (support for test_controller_compare.ps1)
- test_extract_regression_template.json5 (template with placeholders)

### Runner changes:
- test_helpers.ps1: add Get-ScriptStandalone helper
- Run-TestMenu.ps1: read _standalone, show [support] tag, dim in list
- run_test.ps1: warn if non-standalone script invoked directly

---

## Phase 2: Fix test_launch_to_automap  [x]
Add automap open/verify/close steps using TAB key + assert automap_active.
automap_active already exposed in introspection. No C++ changes.

---

## Phase 3: Fix test_keyboard_manual startup  [x]
Replace step 4 `select "Change Pilots"` with standard pilot acceptance flow:
1. wait_for screen_mode=menu (timeout 20s)
2. select "Ok" (accept default pilot)
3. wait_for screen_mode=menu (main menu)
4. select "Change Pilots" (now accessible from main menu)

---

## Phase 4: Fix test_env.ps1 StrictMode crash  [x]
Line 13: `if (-not $script:_testEnvLoaded)` -> `if (-not (Test-Path variable:script:_testEnvLoaded))`
Fixes: test_dual_emu_setup, test_dual_emu, test_mp

---

## Phase 5: Fix test_dpad_triggers throttle_time  [x]
STEP_SEND_AXIS re-injects axis every frame during post_delay_ms.
joy_axisbutton_handler needs a 0->high transition to fire a button event.
Fix: inject axis only once (phase 0), then wait (phase 1). Same pattern as STEP_SEND_BUTTON.
Files: game_automate.cpp STEP_SEND_AXIS handler, d2/arch/sdl/joy.c

---

## Phase 6: Fix test_controller_compare  [x]
6a. PS1 uses Send-AutomationScript (raw push) -- need Resolve-TestScript to filter _info/when
6b. D2 "Ok" not found: pilots exist (patched), so select "Ok" fails. Use key enter or select "player"
6c. D1 20-item mismatch: known launcher introspection limitation (note, don't block)

---

## Phase 7: Fix test_resolution  [x]
PS1 pushes raw json5 (no Resolve-TestScript). Also the "select Ok" failure may be
from stale state. Use Resolve-TestScript for the push and ensure thorough reset.

---

## Phase 8: Fix test_autoselect_plx  [x]
Line 80: .Trim() on null array. Test is distinct from test_autoselect_crash (tests .plx format
vs JNI save path). Fix null handling, add SKIP guards for missing .plx files.

---

## Phase 9: Fix test_bot_client auto-server  [x]
Remove -AutoServer switch. Always auto-start server if not reachable.

---

## Phase 10: Fix test_saf_archiver  [x]
stat path wrong: files/$TEST_FILE should be files/sets/default/$TEST_FILE.
Also handle ErrorRecord from stat. Fix all file path refs in the test.

---z

## Phase 11: Dual-emulator auto-start deps  [x]
test_lan.ps1 + test_mp.ps1: auto-start missing emulators instead of failing.
Extract emulator launch + boot-wait from test_dual_emu_setup.ps1 into shared helper.

---

## Phase 12: Improve Resolve-GameDataDeps  [x]
Add file size comparison for mismatch detection (fast, no transfer needed).
For each dep on device, compare size vs local source. Re-push on mismatch.
Document the function.

---

## Execution order
Independent (parallel): 1, 2, 3, 4, 7, 8, 9, 10, 12
Needs C++ investigation: 5 (dpad triggers)
Depends on Phase 1: 6 (controller compare json5 gets _standalone)
Most complex, last: 11 (dual-emu auto-start)

## Verification
- Parse-check all modified PS1 files
- run-code-quality.ps1 --fix
- Build APK if C++ changed (Phase 5)
- Run key tests: test_launch_to_automap, test_keyboard_manual, test_dpad_triggers,
  test_resolution, test_bot_client
- Verify Run-TestMenu [support] tags
