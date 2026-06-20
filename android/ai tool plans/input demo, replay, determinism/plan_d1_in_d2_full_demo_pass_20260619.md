# D1-in-D2 full demo pass

Goal: run the full D1 input-demo corpus through the D2 executable in D1-in-D2
mode, then flag recordings that fail for likely deletion or recreation. Older
recordings may have been produced before the current D1 demo sync and
D1-in-D2 semantics fixes.

Follow-up goal: enable final-result comparison for D1-in-D2 in the replay
harness itself, then redo the full corpus query through the normal batch
runner.

Scope:

- Corpus: `android/regression_demos/d1_*.dximdemo`
- Runner: existing input-demo regression wrapper
- Mode: D1 recordings replayed by the D2 executable with `-D1InD2`

Plan:

1. [done] Inventory the D1 `.dximdemo` corpus.
2. [done] Run the full corpus under D1-in-D2.
3. [done] Record pass/fail output and identify demos to flag for deletion.
4. [done] Verify no stale replay windows remain after the sweep.
5. [done] Run scoped quality checks if any tracked files are edited.
6. [done] Enable D1-in-D2 final-result comparison in
   `run_input_demo_replay.ps1`.
7. [done] Rerun the full corpus using the built-in D1-in-D2 comparison.
8. [done] Update deletion candidates from the built-in batch result.
9. [done] Re-run scoped code quality for touched files.
10. [done] Delete the flagged demo triplets from
    `android/regression_demos`.

Results:

- D1 corpus count: 13 recordings.
- Batch run command:
  `.\android\tests\run_input_demo_regressions.ps1 -RecordedGame d1 -Game d2 -RunMode graphics -D1InD2 -NoRender -Mode accelerated -TimeoutSeconds 420 -KeepSandbox`
- Batch run result: PASS for completion, 13/13 demos wrote actual results.
- Important harness note: `run_input_demo_replay.ps1` skips final-result
  comparison in `-D1InD2` mode, so a second manual comparison was done against
  each embedded D1 expected result, ignoring only `result.game` and
  `result.mission`.
- Strict D1-in-D2 final-result matches:
  - `d1_descent_level15_20260617_154210`
  - `d1_descent_level16_20260618_201843`
  - `d1_descent_level18_20260618_202117`
  - `d1_descent_level5_20260616_202713`
- Flagged for deletion or recreation:
  - `d1_descent_level12_20260617_204120`: first diff
    `result.player0.energy`, expected 85, actual 95.
  - `d1_descent_level13_20260617_204547`: first diff
    `result.player0.energy`, expected 16, actual 83.
  - `d1_descent_level14_20260617_204837`: first diff
    `result.player0.score`, expected 10400, actual 3000.
  - `d1_descent_level14_20260618_091107`: first diff
    `result.player0.score`, expected 15900, actual 10400.
  - `d1_descent_level15_20260618_091355`: first diff
    `result.player0.shields`, expected 89, actual 69.
  - `d1_descent_level16_20260618_150235`: first diff
    `result.player0.energy`, expected 48, actual 46.
  - `d1_descent_level17_20260618_150642`: first diff
    `result.player0.energy`, expected 105, actual 0.
  - `d1_descent_level4_20260616_121645`: first diff
    `result.player0.shields`, expected 97, actual 104.
  - `d1_descent_level6_20260617_153740`: first diff
    `result.player0.energy`, expected 76, actual 75.
- Each flagged recording has all three sidecars present:
  `.dem`, `.dximdemo`, and `.dximdemo.rngtrace.jsonl`.
- No stale `dxx-redux-d*` processes were left after the sweep.
- Harness follow-up:
  - `run_input_demo_replay.ps1` now compares final results in `-D1InD2`
    mode.
  - The comparison normalizes only `result.game` and `result.mission` to the
    actual D2 replay values before using the existing recursive final-result
    diff.
  - Smoke check: level 18 passes built-in D1-in-D2 comparison.
  - Smoke check: level 12 fails built-in D1-in-D2 comparison with the expected
    final-result diff.
- Built-in comparison rerun command:
  `.\android\tests\run_input_demo_regressions.ps1 -RecordedGame d1 -Game d2 -RunMode graphics -D1InD2 -NoRender -Mode accelerated -TimeoutSeconds 420`
- Built-in comparison rerun result: FAIL, with 9 failed recordings and 4
  strict keepers. This matches the earlier manual comparison.
- Built-in strict keepers:
  - `d1_descent_level15_20260617_154210`
  - `d1_descent_level16_20260618_201843`
  - `d1_descent_level18_20260618_202117`
  - `d1_descent_level5_20260616_202713`
- Built-in deletion or recreation candidates:
  - `d1_descent_level12_20260617_204120`
  - `d1_descent_level13_20260617_204547`
  - `d1_descent_level14_20260617_204837`
  - `d1_descent_level14_20260618_091107`
  - `d1_descent_level15_20260618_091355`
  - `d1_descent_level16_20260618_150235`
  - `d1_descent_level17_20260618_150642`
  - `d1_descent_level4_20260616_121645`
  - `d1_descent_level6_20260617_153740`
- Deletion follow-up: verified that each flagged stem has `.dem`, `.dximdemo`,
  and `.dximdemo.rngtrace.jsonl` sidecars before deletion.
- Deleted 27 files total: the 9 flagged stems, each with `.dem`, `.dximdemo`,
  and `.dximdemo.rngtrace.jsonl`.
- Post-deletion D1 regression demo list contains only the 4 strict keepers.
