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
