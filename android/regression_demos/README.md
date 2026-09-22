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

The shared reader accepts recordings up to 1 GiB and reads bounded JSON lines
(8 MiB per record), avoiding whole-file text copies. The decoded checkpoint
limit remains 2 MiB. Parsed frame data still occupies memory until replay setup
finishes; this is not a constant-memory replay engine

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
now selects D1 startup explicitly and needs only the same D1 HOG/PIG as native D1

The paired evidence runner executes every selected recording twice in native D1
and once in D1-in-D2, sequentially, against an isolated copy of those D1 assets:

```powershell
.\android\tests\test_d1_replay_parity.ps1
# Restrict an investigation without silently skipping a required filename:
.\android\tests\test_d1_replay_parity.ps1 -DemoFileName d1_descent_level14_20260920_184354.dximdemo
```

It is also available through `run_all_tests.ps1 -Filter test_d1_replay_parity`.
It is an explicit investigation while its coverage gate remains incomplete, so
the routine unattended suite does not run a knowingly incomplete qualification.
The timestamped output under `temp/d1_replay_parity_*` contains a manifest of recording/checkpoint,
asset and executable hashes, recorded player settings, launch commands, raw results,
compressed state/RNG traces and a report with three independent relationships:
recording versus native, native repeatability, and native versus imported. Every
available frame field and diagnostic is compared; only declared engine/mission
aliases and RNG source-code labels are mapped. Terminal player values are never
copied into an expectation. Missing/truncated evidence is not a pass

The output also archives the working source patch and runner sources. State-trace
metadata describes the recording in both engines; terminal results identify the
executing engine. RNG reports retain context differences in their strict verdict
and separately show the first value/timing/count difference, so an annotation
cannot hide a later simulation difference. SIM and effects RNG streams have
independent verdicts; matching simulation draws cannot conceal effects drift

Frame reports also inventory every diagnostic field, including its compared and
differing frame counts and first differing value. These entries supplement the
strict verdict: an early hash/layout difference cannot hide a later mismatch in
weapon orientation or another diagnostic. No diagnostic exclusion list is used

Replay state traces also contain version-1 `object_state` records after each
`frame_state`. Frame zero resets a slot map; subsequent `slots` entries replace a
complete object, or delete it with `null`. Unlisted slots retain their preceding
state. Slot reuse carries the new signature and complete replacement. Each record
also contains the full allocator list, segment heads, simulation clock and both
actual RNG streams. The paired runner compares reconstructed objects by named
field and preserves the first differing slot/frame for every field, including
fields absent from the old hashes such as orientation and weapon hit history

These records expose common object fields, active movement/control/render data,
retained ghost polygon data and per-robot local AI data. They omit ABI padding
and inactive union interpretations. D1 and D2 fields remain in their original
representations: no reactor-ID, AI-layout or unused-capacity mapping is applied
yet. This covers live objects after each replay frame, not the complete world,
player/global state, pre-advance restore or pre-retirement transition snapshots

State-log paths ending in `.gz` are compressed by the engine as it writes. The
paired runner uses `state.jsonl.gz`; the existing PowerShell trace comparator
also accepts plain or gzip input. Original records remain available for auditing

This is still F1 work in progress: current diagnostics contain unmapped engine
representations, and complete pre-advance and pre-retirement semantic snapshots
are not yet emitted. The report keeps these coverage gaps explicit and returns
nonzero for incomplete qualification (2) or observed differences (1). An exact
summary-state match alone cannot qualify full fidelity

For a single result comparison, `run_input_demo_replay.ps1 -StrictComparison`
disables the legacy terminal-exit subset. `-SkipExpectedChecks` is capture-only
and prints `CAPTURED`, not `PASS`. A required missing reference is an error

The [finishing plan](../ai%20tool%20plans/asset%20management/d1-in-d2-consolidation-plan.md#0-stocktake-and-plan-to-finish)
defines the next gate: repeatable native reference, unchanged checkpoint/input
replay in D2, exact frame-zero/per-frame/terminal semantic state and RNG
comparison, and separate recorded-versus-native and native-versus-imported
verdicts. Do not use fresh-level startup or expected-result substitutions to
make checkpoint demos pass
