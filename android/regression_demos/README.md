# input-based demo file regression tests
* these are demo files which consist of player inputs rather than game element positions (which is what the classic .dem files encode)
* these are meant to form a set of regression tests so we can replay them and check the final state produced. with a large enough body of demo files we should be able to lock down game engine behavior and enable larger refactoring passes, such as to de-duplicate d1/ and d2/

# todo
* remove debug state tracing from the early demos

# Watch one recording

Run `android/run_all_tests.ps1` without arguments and choose **R. Replay a demo**.
Choose D1, D2 or D1-in-D2, then the recording, then headed or headless playback.
Headed playback uses normal speed and a visible game window. Headless playback
runs accelerated; D1 and D1-in-D2 currently use the no-present windowed runner.
This menu starts only the selected replay, not the test suite

From the repository root, jump straight to the picker:

```powershell
.\android\run_all_tests.ps1 -ReplayDemo
```

The existing single-demo helper also works directly. To watch the new level-14
recording in native D1:

```powershell
.\android\tests\run_input_demo_replay.ps1 -DemoPath android\regression_demos\d1_descent_level14_20260920_184354.dximdemo -Game d1 -Runner visual -Mode realtime -RenderProfile default -ReplayRobotLabels hide
```

Use `-Game d2 -D1InD2` to watch that D1 recording in D2. The helper's
`-Interactive` switch opens the same picker; add `-SearchRoot <directory>` for
recordings outside this corpus. Result comparisons still run after playback,
so viewing a divergent recording can end with a nonzero test result

# D1-in-D2 verification

From the repository root, the existing runner can execute the D1 corpus in
both engines:

```powershell
.\android\tests\test_input_demo_regressions.ps1 -RecordedGame d1 -Game d1 -ResultArchiveRoot temp/d1-parity/native
.\android\tests\test_input_demo_regressions.ps1 -RecordedGame d1 -D1InD2 -ResultArchiveRoot temp/d1-parity/imported
```

Keep native and imported results separate, including failed results. Both
commands currently compare against the recordings; they do not yet provide a
strict, paired native-versus-imported state comparison. The imported runner
also currently requires a combined D1/D2 asset directory

The [finishing plan](../ai%20tool%20plans/asset%20management/d1-in-d2-consolidation-plan.md#0-stocktake-and-plan-to-finish)
defines the next gate: repeatable native reference, unchanged checkpoint/input
replay in D2, exact frame-zero/per-frame/terminal semantic state and RNG
comparison, and separate recorded-versus-native and native-versus-imported
verdicts. Do not use fresh-level startup or expected-result substitutions to
make checkpoint demos pass
