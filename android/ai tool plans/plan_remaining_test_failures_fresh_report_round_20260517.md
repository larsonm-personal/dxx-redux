# Plan: Remaining test failures fresh-report round 2026-05-17

## Goal

- Start the next remaining-test-failure round from fresh evidence instead of the stale 2026-05-16 failure list.
- Improve report triage enough that `FAIL`, `TIMEOUT`, and multi-game partial failures are all visible before choosing a code fix.
- Fix only the first locally reproducible failure that survives fresh isolated reruns.

## Source context

- Source plan: [plan_remaining_test_failures_20260516.md](plan_remaining_test_failures_20260516.md)
- Reusable cleanup playbook: [reusable/cleanup.md](reusable/cleanup.md)
- Latest suite report inspected during planning: `temp/test_reports/report_20260516_174632.md`

## Findings from this look

- The source plan says every original report candidate is currently stale or non-reproducing on the current build.
- The latest report still shows the older stale shape: `45` passed, `11` failed, `2` timed out, `6` skipped, total `00:46:15`.
- The report's `## Failures` section includes only `FAIL` statuses. `TIMEOUT` rows are present in `## Results` but do not get their own detail sections, which makes timeout triage easy to miss.
- Some multi-game JSON5 logs show a later `PASS for D2` in the tail even though the overall test failed earlier, so report tails can be misleading without the full per-test log or durable automation result.
- `android/run_all_tests.ps1` already has useful infrastructure: tiering, report dirs, per-test logs, focused `-Filter`, `-StopOnFail`, launcher preflight, and single-emulator recovery.
- Recent terminal context shows emulator startup attempts failing, so this round should begin with emulator health instead of assuming fresh Android reruns are available.

## Local hypothesis

The next productive tranche is not to chase the stale named failures directly. The likely root of confusion is stale report data plus report summarization gaps. Once a fresh report or focused reruns are captured, there may be one real remaining failure, but it should be selected from new evidence.

## Scope

In scope:

- Report triage workflow around `android/run_all_tests.ps1` output.
- Small, behavior-preserving report improvements in `android/run_all_tests.ps1` if they help surface current failures and timeouts.
- Fresh focused reruns of stale report candidates only after emulator health is confirmed.
- One local fix for the first fresh reproducible failure, if one appears.

Out of scope for this round:

- Broad game-code refactors.
- Large automation API redesign.
- Reworking known-stale test scripts without a fresh reproducer.
- Dual-emulator or LAN fixes before a healthy emulator baseline exists, except for report classification of their timeouts.

## Work items

- [x] Confirm emulator and ADB health before any Android rerun.
- [x] Parse the latest report into a short candidate table that includes both `FAIL` and `TIMEOUT` rows.
- [x] Inspect full logs, not just report tails, for the first few candidates and identify whether the failure is D1-only, D2-only, both-game, timeout, infrastructure, or report stale.
- [x] Patch `android/run_all_tests.ps1` so markdown detail sections include both failures and timeouts.
- [x] If cheap, add a concise status-specific snippet selection for failure details, preferring `automation_result.json`, `automation_log.jsonl`, `ASSERT_FAIL`, `TIMEOUT`, `FAIL for`, `SetupActivity not responding`, and runner kill lines over a plain last-20-lines tail.
- [x] Run `android/run-code-quality.ps1 -Fix -Paths android/run_all_tests.ps1` after report-runner edits.
- [x] Generate a fresh focused report in a new report directory, starting with no-infra tests and then the highest-signal stale candidates.
- [x] If a candidate reproduces, fix the smallest local cause and rerun that exact test.
- [x] Because candidates reproduced, update this plan with the fresh pass/fix evidence rather than editing scripts speculatively.
- [x] Continue into the autosave cluster, separate suite-timeout issues from real launcher handoff failures, and rerun the cluster after the harness fix.
- [x] Revisit `test_launcher_dpad` from fresh evidence, fix the current focus and stale-picker causes, and rerun it after formatting.
- [x] Investigate `test_lan` from fresh evidence, separate harness issues from the real multiplayer launch failure, and rerun both the focused script and unattended suite after the fix.
- [x] Re-audit the `state.c` GameTime64 restore change from the save contract instead of trusting the old replay demos.
- [x] Stop using the old rewind-era level 10 and level 11 replay demos once the user marked them invalid repros.
- [x] Continue fresh isolated reruns from `report_20260517_190238.md` until a current failure survives or the remaining rows are shown stale.

## Validated outcomes

- `android/run_all_tests.ps1` now writes `## Non-passing Results`, includes both `FAIL` and `TIMEOUT` rows, and prefers status-aware log excerpts over a blind tail.
- Emulator health was re-established first and stayed healthy for this round on `emulator-5554`.
- Fresh isolated rerun: `test_death` is currently stale and passed on the current build.
- Fresh isolated rerun: `test_axis_mapping` was a real current failure, but only on the D1 path. The D1 phase timed out in `skip_briefing` after difficulty select while D2 passed. Switching the D1 path to a single direct `Escape` fixed the test, and the focused rerun passed in `report_20260517_150522.md`.
- Fresh isolated rerun: `test_dpad_triggers` reproduced the same D1-only `skip_briefing` timeout shape. Applying the same D1-specific post-difficulty `Escape` routing fixed it, and the focused rerun passed in `report_20260517_151014.md`.
- Fresh isolated rerun: `test_launch_to_automap` is currently stale and passed on the current build with no changes in `report_20260517_151319.md`.
- Fresh isolated rerun: `test_pause_menu_return` reproduced the same D1-only `skip_briefing` timeout shape. Applying the same D1-specific post-difficulty `Escape` routing fixed it, and the focused rerun passed in `report_20260517_151752.md`.
- Fresh isolated rerun: `test_keyboard_defaults` reproduced the same D1-only `skip_briefing` timeout shape. Applying the same D1-specific post-difficulty `Escape` routing fixed it, and the focused rerun passed in `report_20260517_152213.md`.
- Fresh isolated rerun: `test_engine_prefs_unified` reproduced the same D1 post-difficulty `skip_briefing` failure in `report_20260517_152549.md`. Because the script runs D1 and D2 inside one suite test, the later D2 phase was also killed by the outer `run_all_tests.ps1` 120s timeout after D1 consumed time. Applying the D1-specific post-difficulty `Escape` routing and adding a 240s per-test timeout override fixed it; the focused rerun passed in `report_20260517_152919.md`.
- Fresh autosave cluster rerun: `test_autosave_resume_missing_pilot_unified` was first killed by the suite wrapper timeout in `report_20260517_153354.md`. Adding a 300s per-test timeout override exposed that the script itself was healthy; it passed in `report_20260517_153657.md` and again in the cluster report `report_20260517_155844.md`.
- Fresh autosave cluster rerun: `test_autosave_resume_unified` was first killed by the suite wrapper timeout in `report_20260517_154055.md`. After adding a 300s override, `report_20260517_154350.md` exposed a real launcher harness race: SetupActivity consumed `LAUNCHER_CONTINUE` and resumed at step 24, while `Watch-AutomationResult` also saw the handoff file and force-stopped the resumed launcher, losing the executor. `android/test_helpers.ps1` now rechecks whether `automation_result.json` is still an unclaimed `LAUNCHER_CONTINUE` before restarting; if SetupActivity consumed it, the watcher lets the in-process executor continue. The focused rerun passed in `report_20260517_155321.md`, and the full `test_autosave_resume*` cluster passed in `report_20260517_155844.md`.
- Fresh isolated rerun: `test_launcher_dpad` first reproduced as stale save/resume-offer state in `report_20260517_160728.md`. After the test cleared save files and disabled the resume offer at startup, it reproduced the older focus bug in `report_20260517_160937.md`: `DPAD_CENTER` activated the import picker instead of `Define Controls`. The launcher now re-requests main-page focus after the full page settles, the test and shared app-stop helper close stale DocumentsUI pickers, and the focused rerun passed in `report_20260517_161620.md` and again after formatting in `report_20260517_161837.md`.
- Scoped code quality passed after the runner, launcher, and script edits: `android/run-code-quality.ps1 -Fix -Paths @('android/app/src/main/java/com/dxxredux/app/SetupActivity.kt','android/tests/test_launcher_dpad.ps1','android/test_helpers.ps1','android/run_all_tests.ps1','android/game_scripts/test_engine_prefs_unified.json5')`.
- Fresh isolated rerun: `test_lan` first failed before real multiplayer triage because `android/tests/test_lan.ps1` read the optional introspection field `multiplayer` under strict mode during early startup. The script now treats `multiplayer` and `num_connected` as optional until `is_network` is true.
- The same `test_lan` rerun also exposed two shared strict-mode harness defects: `android/test_helpers.ps1` `Write-Status` assumed `script:LogFile` was always initialized during suite preflight, and `android/run_all_tests.ps1` assumed every test catalog entry had an `Arguments` property. Both are now guarded.
- With those harness issues removed, `test_lan` still reproduced in both direct and relay modes: the host stayed in `screen=movie` and never entered network mode while the joiner sent one-way traffic. `android/app/src/main/java/com/dxxredux/app/MainActivity.kt` now forces launch-intro skipping for multiplayer launches when applying skip-intro settings, so `onResume` no longer restores the saved preference and strand auto-host or auto-join in the intro path.
- Focused relay validation passed in `temp/test_lan_relay_probe_after_skipfix.txt`, focused direct validation passed in `temp/test_lan_direct_after_skipfix.txt`, and the unattended suite rerun passed in `report_20260517_185058.md` and again after formatting in `report_20260517_185428.md`.
- Scoped code quality also passed after the LAN tranche edits: `android/run-code-quality.ps1 -Fix -Paths @('android/app/src/main/java/com/dxxredux/app/MainActivity.kt','android/tests/test_lan.ps1','android/test_helpers.ps1','android/run_all_tests.ps1')`.
- Fresh no-infra rerun: `test_gradle_unit_tests` reproduced as two failures in `RemainingKeyTouchActionsTest` from the same local regression. `android/app/src/main/java/com/dxxredux/app/TouchOverlayView.kt` had dropped `META_REWIND` from `remainingBaseActionBindings`, so empty touch layouts no longer exposed rewind in the More menu even though the shared label and native meta action still existed. Restoring `META_REWIND` fixed the focused Gradle class run, and the wrapper rerun passed in `report_20260517_201125.md` and again after formatting in `report_20260517_201301.md`.
- Fresh single-emulator rerun: `test_abort_game_to_main_menu_d2` reproduced as a launcher resume-offer mismatch from `report_20260517_190238.md`, but the root cause was in the automation helper rather than the resume bridge. `Abort Game` already wrote the correct level-1 `AUTO EXIT`, then the script's later `enter_launcher` call wrote a second newer `AUTO EXIT` from the top-level menu with `current_level_num = 0`, so SetupActivity surfaced the wrong save candidate. `android/app/src/main/cpp/shared/game_automate.cpp` now skips the `enter_launcher` autosave unless an active in-level game is still running, and the focused rerun passed in `report_20260517_201737.md` and again after formatting in `report_20260517_202149.md`.
- Fresh single-emulator rerun: `test_levelcomplete_touch_skip` is currently stale on the current build. The same script from the suite report passed unchanged in `report_20260517_202434.md`, with `cutscene_tap_suppress_arms = 1`, `cutscene_tap_suppress_hits = 1`, the first tap swallowed, and the second tap advancing as expected.
- Fresh single-emulator rerun: `test_launcher_dpad` still reproduced on the current build in `report_20260517_202608.md`, again at the initial `DPAD_CENTER` activation step. The main page could finish recomposing after the first focus request and leave no active D-pad target even though the buttons were visible. `android/app/src/main/java/com/dxxredux/app/SetupActivity.kt` now issues a delayed second `initialFocusRequester.requestFocus()` in the controller section, and the focused rerun passed in `report_20260517_202940.md` and again after formatting in `report_20260517_203151.md`.
- Fresh single-emulator rerun: `test_pause_menu_return` is currently stale on the current build. The same script from the suite report passed unchanged in `report_20260517_203338.md`, so the earlier timeout no longer reproduces after the existing D1 post-difficulty routing fix.
- Save-contract audit: the `state.c` timer restore change remains principled even after dropping the old replay demos. Save-all writes `GameTime64 = 0`, `state_player_to_player_rw()` stores only `cloak_time` and `invulnerable_time` relative to `GameTime64`, and `player_rw` narrows only those player timestamps from `fix64` to `fix`, so restore must re-base those two fields on the current `GameTime64`.
- Replay triage update: the previously investigated D2 level 10 and level 11 input demos were removed from active debugging after the user identified both as rewind-era invalid repros. The `state.c` timer re-base fix and the `object.c` `object_signature_seed` restore fix remain in the working tree on code-contract grounds, but there is no longer a trusted replay repro from this report round.
- Fresh single-emulator rerun: `test_autoselect_crash_unified` is currently stale on the current build and passed unchanged in `report_20260517_205432.md`.
- Fresh single-emulator rerun: `test_mine_exit_movie_touch_skip` is currently stale on the current build and passed unchanged in `report_20260517_205614.md`.
- Fresh single-emulator rerun: `test_quick_record_classic_sidecar_stage` is currently stale on the current build and passed unchanged in `report_20260517_205745.md`.
- Fresh single-emulator rerun: `test_resolution_unified` is currently stale on the current build and passed unchanged in `report_20260517_205926.md`.
- Fresh single-emulator rerun: `test_saf_basic` is currently stale on the current build and passed unchanged in `report_20260517_210051.md`.
- Fresh single-emulator rerun: `test_title_music_skip_pref_unified` is currently stale on the current build and passed unchanged in `report_20260517_210215.md`.

## Likely next candidate if continuing

- The D1 post-difficulty briefing cluster, autosave resume cluster, launcher D-pad test, `test_lan`, `test_gradle_unit_tests`, `test_abort_game_to_main_menu_d2`, `test_levelcomplete_touch_skip`, `test_launcher_dpad`, `test_pause_menu_return`, `test_autoselect_crash_unified`, `test_mine_exit_movie_touch_skip`, `test_quick_record_classic_sidecar_stage`, `test_resolution_unified`, `test_saf_basic`, and `test_title_music_skip_pref_unified` are cleared for this round's focused reruns.
- `report_20260517_190238.md` no longer has a trusted surviving repro. If cleanup continues, start from a new suite report instead of returning to the deleted rewind-era input demos or the now-stale non-passing rows from that report.

## Candidate rerun order

Use a dedicated report directory so the next tranche starts from fresh evidence rather than the now-cleared `report_20260517_190238.md`:

```powershell
$roundReportDir = "temp\test_reports\round_remaining_20260517"
```

Start with a fresh unattended report or a narrow cluster that has not already been retired in this round:

```powershell
.\android\run_all_tests.ps1 -ReportDir $roundReportDir -StopOnFail
```

If the next report still contains non-passing rows, rerun only the surviving candidates in isolation before editing code:

```powershell
.\android\run_all_tests.ps1 -Filter <surviving_test_name> -ReportDir $roundReportDir -StopOnFail
```

## Validation ladder

For report-only changes:

```powershell
.\android\stop-stale-formatters.ps1
.\android\run-code-quality.ps1 -Fix -Paths android\run_all_tests.ps1
.\android\run_all_tests.ps1 -Filter test_gradle_unit_tests -ReportDir temp\test_reports\round_remaining_20260517 -StopOnFail
```

For any Android automation or native code fix selected after fresh reproduction:

```powershell
.\android\gradlew.bat :app:assembleDebug :app:testDebugUnitTest --console=plain
.\run-windows-build.ps1 -Target both
.\android\run_all_tests.ps1 -Filter <fresh_repro_test> -ReportDir temp\test_reports\round_remaining_20260517 -StopOnFail
```

## Decision rules

- Treat the May 16 report as historical evidence only.
- Do not modify a test script just because it appears in the stale report.
- Prefer runner/reporting fixes before game fixes if the fresh evidence is ambiguous.
- Prefer fresh isolated reruns over stale timeout or failure logs once a few earlier fixes have landed. Old report rows can all go stale within the same cleanup round.
- If a multi-game test reports overall failure but the tail shows only a later pass, inspect the earlier game phase and durable automation files before choosing a fix.
- If emulator health is failing, record the blocker and work on report-only changes that can be validated without the emulator.

## Done when

- A fresh report directory exists for this round, or emulator health is recorded as the blocker.
- `FAIL` and `TIMEOUT` candidates are both visible in the report triage path.
- Any report-runner edits pass scoped code quality.
- The first fresh reproducible failure is either fixed and rerun, or the round records that no trusted current failure survived the stale-report reruns.
