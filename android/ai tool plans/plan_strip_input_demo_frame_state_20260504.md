# Plan: Strip Input Demo Frame State (2026-05-04)

## Goal
- Add a small script that strips replay-irrelevant per-frame `state` fields from `.dximdemo` files so git fixtures can stay minimal
- Default that script to `android/temp_game_logs` and keep the scan to the top directory only

## Local Hypothesis
- The replay loader already accepts frame records without `state`, so a line-by-line rewrite that removes only the top-level `state` property from `type=frame` records should keep the file replayable
- The cheapest check is to strip a synthetic demo in a temp fixture, confirm nested demos are untouched, and replay the stripped top-level file through the existing headless replay helper

## Execution Plan
- Add a PowerShell script under `android/` that rewrites only immediate `.dximdemo` children of the target directory in place
- Add a narrow PowerShell test under `android/tests/` that creates a temp fixture, strips the top-level demo, leaves a nested demo unchanged, and replays the stripped file
- Run the new test, then run a scoped PowerShell code-quality pass on the new scripts and rerun the test if formatting changes either file

## Status (2026-05-04)
- Phase 1 completed
  - added `android/strip_input_demo_frame_state.ps1`, which defaults to `android/temp_game_logs`, scans only immediate `.dximdemo` children, removes only top-level `state` fields from `type=frame` records, and rewrites changed files in place
- Phase 2 completed
  - added `android/tests/test_strip_input_demo_frame_state.ps1`, which builds a temp fixture with one top-level demo and one nested demo, strips only the top-level file, and confirms the nested file stays unchanged
- Phase 3 completed
  - validated with `android/tests/test_strip_input_demo_frame_state.ps1`, which passed after replaying the stripped top-level demo through `run_input_demo_replay.ps1` using the explicit `windowed-no-present` runner in smoke mode
  - ran `android/run-code-quality.ps1 -Fix -Paths @('android/strip_input_demo_frame_state.ps1','android/tests/test_strip_input_demo_frame_state.ps1')`, then reran the same test to another pass after formatting