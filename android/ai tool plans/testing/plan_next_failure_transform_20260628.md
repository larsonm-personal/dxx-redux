# Next Failure Transform Plan - 2026-06-28

## Goal
Pick one remaining failure from `temp/test_reports/report_20260628_101133.md` and make a transformational robustness change, avoiding timeout tweaks and one-off script nudges.

## Candidate Failures
- `test_merged_wall_two_pass_probe` / `test_merged_wall_two_pass_debug_mode_probe`: both fail on `merged_wall_snapshot.center_hit_count >= 1`.
- `test_mine_exit_movie_touch_skip`: fails before gameplay because launcher automation cannot find a `Descent 2` launch button.

## Tasks
- [x] Inspect the failing scripts, existing plans, and relevant automation code
- [x] Select the best single test for a contract-level robustness change
- [x] Implement the robustness change with minimal scope
- [x] Run the targeted test
- [x] Run scoped code quality
- [x] Summarize outcome and residual risk

## Selection
Selected `test_mine_exit_movie_touch_skip`. It failed before reaching its subject because launcher automation could not find a `Descent 2` button. The test is about the in-game mine-exit movie touch-suppress path, not the launcher game-selection UI, so depending on launcher button text is the wrong contract. The launcher script executor already supports `enter_game`, which launches a requested game directly and resumes the same script in `MainActivity`.

## Verification
- `android\helpers\run_test.ps1 test_mine_exit_movie_touch_skip.json5 -TimeoutSeconds 600`: passed. The log shows launcher step 6 using `enter_game`, `MainActivity` resuming at step 6, and the endlevel path completing with `cutscene_tap_suppress_arms = 3`.
- `android\run-code-quality.ps1 -Fix -Paths <changed files>`: passed.
