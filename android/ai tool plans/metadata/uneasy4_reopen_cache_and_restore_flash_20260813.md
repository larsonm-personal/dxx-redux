# Uneasy4 Metadata Reopen Cache and Restore Flash

## Reported behavior

- Reopening Uneasy4 metadata is faster than the first open but still takes 3 to 5 seconds on a flagship phone.
- Restoring the launcher with the metadata viewer open still visibly renders twice.

## Investigation plan

1. [complete] Trace the viewer reopen path from persisted metadata lookup through archive staging, cache validation, and native analysis.
2. [complete] Measure which work repeats on a cache hit and distinguish required archive I/O from avoidable recomputation.
3. [complete] Trace restored viewer state emissions and Compose/navigation reconstruction to identify both visible renders.
4. [complete] Add narrowly scoped profiling and Game Logs for cache decisions and restored-view transitions where existing evidence is incomplete.
5. [complete] Reproduce through launcher automation or a focused test, then document root causes and the smallest safe fixes.

## Constraints

- Do not change cache validity or metadata output during diagnosis.
- Preserve resumable background computation and the shared launcher/game cache format.
- Avoid masking lifecycle problems with arbitrary delays or animation suppression.

## Findings

- The completed `LevelMetadataResult` is stored only in an ephemeral request directory and deleted after every analysis. The persistent cache covers route analysis, not the viewer's complete result.
- Reopening a durably extracted Uneasy 4 mission avoids ZIP staging and reuses route data, but still starts a worker, initializes the native runtime, loads the large level, rescans metadata, serializes JSON, and parses it in the launcher.
- An x86_64 emulator diagnostic measured the first foreground analysis at 11.59 seconds. The repeated analysis spent 4.54 seconds in the worker and 6.80 seconds total; 2.27 seconds of the total was worker preemption/queue delay caused by concurrent background precomputation.
- Every successful result was then logged as `result_deleted=true`, confirming that a third open would repeat the same viewer-level work.
- `SetupActivity.onResume()` still increments the global refresh immediately and schedules three more increments at 250 ms, 1 second, and 2.5 seconds. Each increment clears and reloads open mod details.
- In the emulator trace, the three delayed callbacks fired within 129 ms after the main thread became available. Compose can coalesce those into fewer visible changes, which explains why the reported flash count fell from three to two without the redundant refresh source being removed.

## Smallest safe follow-up changes

1. Add an atomic persisted full-result cache keyed by source content identity, game, metadata schema, native cache generation, and required base-data identity. Return that result directly before starting a worker.
2. Keep route-cache validation in the worker for misses. Do not change route output or allow stale full results to bypass identity checks.
3. Replace the three blind post-resume refreshes with one coalesced refresh owned by the data that actually needs it. While a metadata dialog is open, retain the last successful mod-detail value instead of clearing it to null.

## Implementation plan

1. [complete] Define a stable full-result cache identity and atomic on-disk format, including source and required base-data fingerprints.
2. [complete] Read valid cached results before worker startup and publish successful complete results without changing route-cache behavior.
3. [complete] Coalesce resume refreshes and retain loaded mod/file details while a replacement value is computed.
4. [complete] Add unit coverage for cache identity, invalidation, atomic publication, and refresh coalescing.
5. [complete] Run focused tests, assemble the APK, and repeat the Uneasy 4 two-open emulator measurement.

## Implementation results

- Full successful results are atomically stored under `files/level_metadata_results` and keyed by SHA-256 content identities for the mission inputs and the base HOG/HAM/PIG inputs that affect analysis.
- Reads reject corrupt entries, incomplete routing, changed inputs, schema/generation mismatches, and results whose referenced native route-cache file is no longer present.
- Cache size is bounded to 512 entries and 256 MiB with least-recently-used pruning by file timestamp.
- Resume now schedules one coalesced refresh after 250 ms instead of immediate plus 250 ms, 1 second, and 2.5 second refreshes.
- Open mod, constituent, and file metadata details retain their previous successful value while the one replacement refresh runs.
- The maintained `test_level_metadata_result_cache_reuse.json5` emulator test passed. Uneasy 4's first analysis took 8.65 seconds; the second analyzer call returned the cached result in about 50 ms, including 36 ms to hash and validate 17.5 MiB of inputs.
- The emulator resume trace emitted one refresh trigger. The focused unit tests, automation catalog validation, scoped code-quality checks, and debug APK assembly passed.
