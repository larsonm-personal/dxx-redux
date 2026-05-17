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
- [ ] If no candidate reproduces, update this plan with the fresh pass/non-repro evidence and stop rather than editing scripts speculatively.

## Validated outcomes

- `android/run_all_tests.ps1` now writes `## Non-passing Results`, includes both `FAIL` and `TIMEOUT` rows, and prefers status-aware log excerpts over a blind tail.
- Emulator health was re-established first and stayed healthy for this round on `emulator-5554`.
- Fresh isolated rerun: `test_death` is currently stale and passed on the current build.
- Fresh isolated rerun: `test_axis_mapping` was a real current failure, but only on the D1 path. The D1 phase timed out in `skip_briefing` after difficulty select while D2 passed. Switching the D1 path to a single direct `Escape` fixed the test, and the focused rerun passed in `report_20260517_150522.md`.
- Fresh isolated rerun: `test_dpad_triggers` reproduced the same D1-only `skip_briefing` timeout shape. Applying the same D1-specific post-difficulty `Escape` routing fixed it, and the focused rerun passed in `report_20260517_151014.md`.

## Candidate rerun order

Use a dedicated report directory so this round is easy to separate from the stale May 16 report:

```powershell
$roundReportDir = "temp\test_reports\round_remaining_20260517"
```

Start with cheap and infrastructure-light checks:

```powershell
.\android\run_all_tests.ps1 -Filter test_gradle_unit_tests -ReportDir $roundReportDir -StopOnFail
.\android\run_all_tests.ps1 -Filter test_input_demo_regressions -ReportDir $roundReportDir -StopOnFail
```

Then rerun the stale single-emulator cluster in small filters rather than the whole suite:

```powershell
.\android\run_all_tests.ps1 -Filter test_autosave_resume* -ReportDir $roundReportDir -StopOnFail
.\android\run_all_tests.ps1 -Filter test_axis_mapping -ReportDir $roundReportDir -StopOnFail
.\android\run_all_tests.ps1 -Filter test_dpad_triggers -ReportDir $roundReportDir -StopOnFail
.\android\run_all_tests.ps1 -Filter test_death -ReportDir $roundReportDir -StopOnFail
.\android\run_all_tests.ps1 -Filter test_launch_to_automap -ReportDir $roundReportDir -StopOnFail
.\android\run_all_tests.ps1 -Filter test_pause_menu_return -ReportDir $roundReportDir -StopOnFail
```

Only after single-emulator health is stable, rerun the old timeout candidates:

```powershell
.\android\run_all_tests.ps1 -Filter test_lan -ReportDir $roundReportDir -StopOnFail
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
- If a multi-game test reports overall failure but the tail shows only a later pass, inspect the earlier game phase and durable automation files before choosing a fix.
- If emulator health is failing, record the blocker and work on report-only changes that can be validated without the emulator.

## Done when

- A fresh report directory exists for this round, or emulator health is recorded as the blocker.
- `FAIL` and `TIMEOUT` candidates are both visible in the report triage path.
- Any report-runner edits pass scoped code quality.
- The first fresh reproducible failure is either fixed and rerun, or the round records that the stale candidates did not reproduce.
