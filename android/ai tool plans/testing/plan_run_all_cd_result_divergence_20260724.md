# Run-All CD Result Divergence Investigation

## Goal

Explain why the general test suite changed the Quartzon 3D regression result
from file-only pass to `skip/not_ready` after the dedicated CD regression suite
passed.

## Plan

- [x] Preserve and inspect the generated spec diff
- [x] Identify how each runner selects and invokes CD regression tests
- [x] Compare setup, APK, cleanup, and result-persistence behavior
- [x] Trace the `skip/not_ready` exit path for this exact spec
- [x] Determine whether the difference is product state, runner arguments, or
      stale APK behavior
- [x] Record the cause and recommend the smallest correction

## Findings

- The dedicated CD runner invokes `test_all_extracts.ps1 -All -SkipLaunch`
  unless `-FullLaunch` is explicitly requested
- The general runner invokes the sampled extraction suite with
  `-SampleCount 1 -Seed 1403533912` and does not pass `-SkipLaunch`
- That seed selected the Quartzon 3D Europe spec in the 2026-07-24 run
- Extraction succeeded and all three expected files were present
- Setup introspection reported `d2.ready=false` because `alien1.pig`,
  `alien2.pig`, `fire.pig`, and `ice.pig` were absent
- File-only mode records that state as a pass before checking
  `state.can_launch`; full mode reaches the readiness check and records
  `skip/not_ready`
- The skip branch explicitly records `test_mode=file_only` because no launch
  occurred, so the single `last_test_result` field overwrites the earlier
  file-only pass even though the runners were requesting different coverage
- The general suite also runs standalone `test_extract.ps1`, which selected
  `d1 mac 2nd bin+cue`; it did not rewrite the Quartzon result

## Conclusion

This diff is not evidence of an extraction or parsing regression. It is caused
by the runners using different acceptance modes and sharing one persisted
`last_test_result`. The most useful correction is to preserve file-verification
and launch-verification results separately, or at minimum prevent a launch
readiness skip from replacing a successful file-only extraction result.

## Follow-up: Partial D2 Data Launch Support

- [x] Trace launcher readiness for retail, demo, preview, and OEM D2 data
- [x] Compare launcher file requirements with engine mission/data selection
- [x] Determine whether Quartzon is misclassified by readiness or imported
      under filenames the launcher does not recognize as a partial data set
- [x] Audit all uses of `-SkipLaunch` and result persistence
- [x] Recommend the runner and persistence behavior that best preserves nightly
      canary evidence without weakening full regression coverage

### Partial-data findings

- `detectD2FileList` recognizes only full retail data and `d2demo.*` data
- Quartzon correctly imports the six files needed by its eight-level OEM set:
  `descent2.hog`, `descent2.ham`, `descent2.s11`, `descent2.s22`,
  `groupa.pig`, and `water.pig`
- The launcher sees `descent2.hog`, selects the full retail requirements, and
  incorrectly demands `alien1.pig`, `alien2.pig`, `fire.pig`, and `ice.pig`
- The engine recognizes the 6,132,957-byte OEM `descent2.hog`, names the
  built-in mission `D2 Destination:Quartzon`, and defines eight normal levels
  plus two secret levels
- The engine also explicitly supports three-level PC shareware and four-level
  Mac shareware D2 mission layouts
- The three-level interactive preview disc has a different HOG size from the
  supported PC shareware layout and remains a known Android launch exception
- D1 readiness already accepts ordinary demo and OEM layouts because all of
  them use the same two required filenames; it separately rejects the known
  non-runnable Test Flight data by identity

### Persistence and runner recommendation

- Keep `-SkipLaunch` as an explicit fast diagnostic mode, but make
  `test_extract.ps1` suppress regression-spec persistence whenever that switch
  was supplied
- Continue persisting intrinsic file-only results for sources whose specs have
  no launch expectation; only an explicit reduced-coverage request should
  suppress persistence
- Make `run_all_cd_regressions.ps1` full-launch by default and offer an explicit
  `-SkipLaunch` option instead of the current inverse `-FullLaunch` option
- Keep sampled general-suite runs in full mode so each nightly canary can
  replace the prior result with current full-strength pass, fail, or skip
- Add an OEM readiness layout selected by the engine's OEM HOG identity and
  require the six OEM files instead of the full retail elemental PIG set

## Implementation

- [x] Added partial-D2 readiness for both known Quartzon HOG layouts
- [x] Required both sound files, `groupa.pig`, and `water.pig` in the
      extraction oracles in addition to the HOG and HAM
- [x] Suppressed regression-spec writes inside `test_extract.ps1` whenever
      `-SkipLaunch` is explicitly supplied
- [x] Made `run_all_cd_regressions.ps1` run full launches by default and
      replaced `-FullLaunch` with explicit `-SkipLaunch`
- [x] Corrected the unverified `Drec Sphere` oracle to the emulator-observed
      `Ahayweh Gate` for all Quartzon releases
- [x] Corrected Quartzon 3D's mission oracle to
      `Descent 2: Counterstrike!`; its outer HOG contains a full `d2.mn2`
      wrapper around the partial A/B-level data

## Verification

- [x] `SetupLaunchReadinessTest` passed with standard OEM, Quartzon 3D,
      incomplete partial-set, demo, retail, and D1 readiness coverage
- [x] `test_cd_regression_runner.ps1` passed
- [x] `test_extract_regression_workflow.ps1` passed
- [x] All 34 CD regression specs passed structural validation
- [x] Scoped code-quality checks passed; the directly invoked Kotlin formatter
      also covered the test source omitted by the scoped wrapper
- [x] An emulator `-SkipLaunch` run passed file verification and preserved the
      Quartzon 3D spec byte-for-byte
- [x] Quartzon 3D passed a full emulator run and reached `Ahayweh Gate`
- [x] Standard Logitech Quartzon OEM passed a full emulator run and reached
      `Ahayweh Gate`

## Constraints

- Do not revert or regenerate the changed spec during diagnosis
- Do not implement a fix until the differing behavior is established
- Preserve unrelated workspace changes
