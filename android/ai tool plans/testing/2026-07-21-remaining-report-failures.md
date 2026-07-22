# Remaining test report failures

## Plan

- [x] Inspect the three remaining failure logs and their test or baseline inputs
- [x] Select one or two failures with a concrete root cause
- [x] Implement minimal fixes without extending timeouts
- [x] Run focused validation and the Windows host build/test
- [x] Record findings and verification

## Findings

- Selected `test_kcxf2_level6_objectives`. The Guide-Bot spawn meta action is
  queued to the game thread. The test's fixed post-action delay could expire and
  fire trigger 15 before the Guide-Bot had an active route goal, so the route
  stayed on trigger 15 and the subsequent expectation for trigger 16 failed.
- Added a state-based wait for an active Guide-Bot route targeting trigger 15
  before firing that trigger. This synchronizes on readiness and does not
  extend a timeout.
- `test_kcxf2_level5_guidebot_route` produced an empty per-test log in the
  report, so the report does not contain enough evidence for a targeted fix.
- `test_mission_route_corpus` differs in more than 229 generated route records.
  Its baseline is already documented as pending while unrelated metadata edits
  affect projection, so the baseline was not regenerated as a speculative fix.

## Verification

- Reproduced the original level 6 failure before the change.
- The corrected emulator test passed all 44 steps after the change.
- `android/tests/test_validate_automation_catalog.ps1` passed.
- Scoped `android/run-code-quality.ps1` passed.
- Android `assembleDebug` passed during diagnosis.
- `run-windows-build.ps1` completed successfully for D1 and D2.
- `git diff --check` passed.

## Follow-up: level 5 Guide-Bot failure

- [x] Reproduce the level 5 failure or identify the vulnerable asynchronous boundary
- [x] Replace fixed-delay assumptions with observable state synchronization
- [x] Run the focused emulator test and required validation
- [x] Record the root cause and verification

### Findings

- The isolated test reproduced the failure after firing trigger 6: the event
  matched the active switch objective, but the test immediately dispatched a
  redundant Guide-Bot `Next` command before the event-driven completion monitor
  had consumed and replanned the route.
- Removed the manual `Next` commands after completing route objectives. The
  test now waits for the production event-driven route transition itself.
- Added an observable readiness check for the spawned Guide-Bot before the one
  initial `Next` command that is still required to select the first goal.
- No timeout values were extended.

### Verification

- Reproduced the original level 5 failure at the trigger 6 to red-key transition.
- The corrected emulator test passed all 48 steps.
- Automation catalog validation and scoped code-quality checks passed.
- Android `assembleDebug` passed after temporary diagnostics were removed.
- The Windows D1 and D2 build completed successfully.
- `git diff --check` passed.

## Follow-up: mission route corpus

- [x] Confirm the failure is deterministic and independent of emulator state
- [x] Reconcile corpus deltas with committed planner and metadata regeneration changes
- [x] Refresh the reviewed baseline only if the new corpus is internally consistent
- [x] Run the corpus test and required validation
- [x] Record findings and verification

### Findings

- This is a deterministic host-only comparison, so emulator state cannot affect it.
- The baseline predated the committed key-routing, Guide-Bot pathing, full metadata
  regeneration, and pinned DOS D1 data changes.
- The reviewed corpus still contains exactly 1,274 records. There are 259 changed
  records, no additions, and no removals.
- Status totals improve from 1,114 `ok`, 83 `partial`, and 77 `failed` to 1,142
  `ok`, 64 `partial`, and 68 `failed`.
- Three D1 routes downgrade. Their regenerated metadata uses the pinned DOS data
  hashes used by Android instead of the formerly preferred Mac data; this can
  change texture-dependent route visibility as well as boss definitions. The
  other transitions and route-step changes align with the committed planner work.
- Refreshed `mission_route_baseline.json` to the reviewed committed corpus.

### Verification

- Mission route corpus passed for all 1,274 records.
- Mission metadata numeric normalization passed.
- Mission metadata travel-time validation passed for all 1,274 records.
- Automation catalog validation and scoped code-quality checks passed.
- The Windows D1 and D2 build completed successfully.
- `git diff --check` passed.
