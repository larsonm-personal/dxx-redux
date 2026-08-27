# Mission Metadata Regression T Failure

## Goal

Determine why the mission level metadata stage failed in the August 26
regression `T` run.

## Plan

1. [Complete] Locate the matching run report and identify the first failure.
2. [Complete] Reconstruct the relevant emulator, archive, and recovery sequence.
3. [Complete] Compare the failure with current scripts and determine whether it
   is data-specific, environmental, or a regression.
4. [Complete] Report the cause and recommend the smallest appropriate follow-up.

## Findings

- Only `Hydro.ZIP` failed. Its foreground 23-level analysis reported a timeout
  after about 27 seconds, while the other archives continued and passed.
- The worker checkpoint was already `done ok`, so the native analysis had
  completed. This was not the 120-second progress deadline expiring.
- The analyzer polling loop checks `resultFile.isFile` and later checks worker
  liveness in the same iteration. If the worker publishes the result and exits
  between those checks, the caller can observe no result first, then no worker,
  and incorrectly break as a timeout without checking the result again.
- The batch's precompute pause is not persistent. Importing the mission restarts
  background precompute, which queued a second Hydro analysis 261 ms after the
  foreground analysis began. This adds contention and makes the completion race
  easier to encounter, but it is not evidence of bad Hydro data.
- A focused `Hydro.ZIP` rerun with regression JSON output disabled passed in 71
  seconds and produced an `ok` result with all 23 levels.

The smallest robust fix is to recheck or consume a just-published result before
treating a stopped worker as failure. The mission batch should also pause route
precompute after import so its intended isolation is effective.

## Implementation Plan

5. [Complete] Close the result-publication versus worker-exit polling race.
6. [Complete] Keep background route precompute paused after mission import in the
   batch automation flow.
7. [Complete] Add focused regression coverage and run the relevant Android and
   script tests.
8. [Complete] Rebuild/install the debug APK and verify `Hydro.ZIP` with regression
   output disabled.

## Validation

- The worker-result grace tests and existing progress-deadline tests pass.
- The complete Android debug unit suite passes.
- The regression regeneration contract passes in PowerShell 5.1 and 7.
- Scoped code quality and direct formatting of the new Kotlin test pass.
- A rebuilt APK installed successfully after its archive was validated.
- The focused `Hydro.ZIP` device run passed all 12 steps in 64 seconds and
  returned all 23 levels.
- Device logs show one Hydro submit, one worker start, and one completion. No
  post-import background Hydro analysis was queued.
