# Mission metadata slow-host hardening

## Goal

Allow mission metadata regeneration to tolerate sustained host and emulator slowdown without accepting a hung analyzer or exhausting recovery capacity because of unrelated earlier incidents.

## Plan

- [x] Inspect the four failed mission artifacts and classify analyzer versus harness failures
- [x] Trace metadata progress deadlines and launcher readiness recovery accounting
- [x] Make active analysis progress extend the deadline under slow execution
- [x] Reset or scope emulator recovery accounting after successful work
- [x] Extend existing focused coverage without duplicating regression-output tests
- [x] Run focused validation and scoped code quality
- [x] Record the failure explanation and rerun guidance

## Findings

- Uneasy4 reached `level_progress` but the analyzer used a 10-second progress extension and rejected forward progress that arrived slower than a configured minimum rate. Reduced CPU throughput could therefore turn active work into `Analysis timed out`.
- SetupActivity readiness used the shared 30-second default. Both failed launch preparations reached that boundary during a run where install, restore, and mission processing times were also substantially elevated.
- The five-recovery counter covered the entire batch. A recovery at mission 92 still consumed capacity at mission 98, and each persistent readiness failure could consume one retry recovery plus one post-failure recovery.
- Sirius2 and TEW were readiness failures. Trainng did not fail its own analysis; it inherited an already exhausted batch-wide recovery budget. Uneasy4 was the only native analysis timeout.

## Resolution

- Native metadata analysis now permits 120 seconds of inactivity at startup and after each new activity or forward checkpoint. Any increase in completed work extends the deadline regardless of how slowly it arrived. The native request budgets and the harness's 900-second per-mission timeout remain in force.
- Mission ZIP batch preparation now waits up to 120 seconds for SetupActivity by default through the configurable `SetupReadyTimeoutSeconds` parameter.
- Emulator recovery is now limited to five consecutive recoveries. A successful mission preparation resets the counter, so old incidents cannot starve later missions of recovery capacity.
- The existing `LevelMetadataProgressDeadlineTest` now covers small and slow forward progress. Its focused JDK 21 run and the existing PowerShell mission ZIP recovery test passed before and after scoped formatting. Scoped Kotlin and PowerShell code quality passed.

## Rerun guidance

- A focused batch over `sirius2.zip`, `TEW.zip`, `trainng.zip`, and `Uneasy4.zip` is sufficient to replace the four failed artifacts before another full regeneration.
