# Single-player periodic saves on the checkpoint worker

## Requirements

- Replace the five-minute synchronous single-player autosave with submission to
  the existing bounded checkpoint worker in both engines
- Capture a coherent world on the game thread; encode metadata and publish files
  on the worker, without rendering a periodic thumbnail
- D2 jobs retain an immutable secret-companion generation, including absence;
  publish primary and companion with the existing rollback transaction
- Refresh companion ownership on context changes and explicit secret mutations,
  never read its contents from disk during periodic capture
- Rotate slots only after successful publication; retry failures without waiting
  on a busy worker, and ignore callbacks from obsolete gameplay contexts
- Preserve synchronous manual/lifecycle completion and ordering against queued
  saves; preserve desktop behavior

## Implementation and validation

1. Extend the pure file publisher with a paired-file adapter and filesystem tests
2. Extend the shared worker with retained companion ownership and slot submission
3. Convert periodic scheduling and add minimal D2 mutation hooks
4. Build both Android engines and run host publication/transaction tests
5. Run high-level single-player save/restore tests, including secret-companion
   presence, replacement and absence; run relevant shared-worker regressions
6. Remeasure the S21 with the natural five-minute level-6 autosave and compare
   against the 75.320 ms baseline; restore diagnostic device settings

Status: complete; implementation, correctness tests, natural timing observation,
shared-worker regression and device cleanup verified

The real secret-return integration exposed an existing D2 reader bug: secret
restores skipped the saved First_secret_visit word rather than consuming and
overriding it. The runtime extension was then four bytes out of alignment and
failed validation. Consume the word for all restore modes while preserving the
secret-return flag policy; the same integration covers the correction.

## Implementation

Single-player periodic saves call the same bounded state_checkpoint queue used
by rewind and co-op. Capture uses a blank thumbnail. The worker encodes metadata,
stages durable files, publishes the primary/companion pair through
android_file_pair_publish, then updates the launcher pointer. Manual and lifecycle
saves keep their synchronous completion contract and wait for older queued jobs.

The companion cache owns immutable bytes. It is refreshed at gameplay context
boundaries, secret-save writes, slot restores, and secret reactor destruction.
Each queued job retains its captured generation even if the live file is changed
or deleted. Regular periodic submission performs no companion disk read.

Completion rotates the alternating slots only on success. A unique pending token
and gameplay-context checks prevent stale callbacks changing a new session's
schedule. Queue-full submissions defer to the existing retry interval.

## Correctness evidence

- Windows D1 and D2 builds passed; D2 rebuilt after the secret reader correction
- Android ARM64 and x86_64 builds passed for both engines
- Host checkpoint_file tests cover primary replacement, paired replacement,
  companion staging failure, existing-backup refusal, and removal on absence
- Existing injected pair-transaction rollback tests, bounded queue tests, metadata
  snapshot tests and the D2 upstream-compatibility suite passed
- S21 test_async_checkpoint_unified passed 43/43 D1 steps and 57/57 D2 steps
- The integration verifies periodic slot rotation and actual restore, byte-exact
  companion retention after deleting the live source before completion, a real
  secret return and revisit with saved robot state, removal of an old companion,
  and a synchronous lifecycle save immediately following a queued periodic save
- Existing exact-reference memory/disk capture checks and repeated rewind restores
  remain part of that same integration
- D1-in-D2 level-6 co-op regression passed with the S21 hosting emulator-5586:
  different valid local histories followed by two authoritative transfers/restores
- Automation catalogs passed: 95 standalone JSON tests, 381 support scripts,
  219 standalone PowerShell tests; master catalog has 318 top-level entries

Artifacts are in temp/sp-async-20261009. The durable integration result files are
integration-d1.result.json and integration-d2.result.json. Early fixture failures
are retained separately: a modal dialog, an incorrect authored entrance assumption,
and then the real secret-return reader misalignment described above.

## S21 natural five-minute autosave

The final ARM64 diagnostic build ran D1-in-D2 First Strike level 6, Trainee,
stationary with the same invulnerability protection as the baseline. Profiling
and automatic slowdown capture were enabled. Observation lasted 310 seconds,
without per-frame ADB polling. Thermal status remained 0, with AP temperature
28.9 C before and 26.0 C after.

The natural periodic save published player.sg4 at 19:46:34 PDT, with 1,278,412
bytes. The observation captured 62 rewind jobs and one disk job, all successful,
with no sidecar failures, deferrals or pending work at the end.

| Natural periodic disk-save phase | Time ms |
| -------------------------------- | ------: |
| Game-thread capture/submission   |   3.021 |
| Of that, world capture           |   2.999 |
| Completion callback              |   0.003 |
| Worker encoding                  |  15.897 |
| Worker publication               |  14.259 |
| Total worker duration            |  30.156 |

Twenty-nine complete frame windows contained 17,395 intervals, averaging 16.692
ms. No interval exceeded 50 ms; the maximum was 43.965 ms. The ten-second window
containing the autosave had a 34.587 ms maximum. These are whole-frame intervals;
the 3.021 ms figure is the separately measured save contribution, not an entire
frame. The baseline's natural periodic save produced a 75.320 ms interval.

Rewind capture/submission had a 6.020 ms median, 10.867 ms p95 and 14.932 ms
maximum. Its median remains essentially unchanged from the baseline; CPU
scheduling and frequency variation remain visible. Moving additional world
serialization off-thread would be a separate optimization.

This is one natural disk save in a stationary diagnostic run, not a worst-case
latency guarantee or a controlled full-APK benchmark. The recorded frame windows
show that the earlier long periodic-save stall did not recur in this run.
summary.json, single-protected.log, single-protected-debug.txt, before/after
introspection and thermal files, and natural-periodic.sg4 retain the evidence.

The S21 diagnostic preferences were restored byte-for-byte from their pre-test
snapshot, stay-awake was restored to 0, and both diagnostic apps were stopped.
The phone was returned Home; the existing emulator was left running. The
production app was not modified. cleanup.json records verification.
