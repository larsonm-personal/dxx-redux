# Metadata worker timeout recovery

1. Trace the reported batch failure and reproduce the initiating CD scan timeout
2. Restart failed native workers for later requests and retain diagnostics
3. Add process-level timeout/crash recovery tests
4. Rerun affected CD scans and built-in metadata without overwriting unrelated regression data

The run 20260907_161237 timed out on Dimensions 1sttrial.msn after 30 seconds. Invoke-MetadataWorker killed the persistent D1 worker but later requests reused it. This cascaded through Dimensions and Destination Saturn, then aborted at built-in First Strike. Existing Obsidian 11 edits and user-generated CD metadata must be preserved.

## Implementation

Extracted native worker lifecycle functions into host_metadata_worker.ps1. Failed requests terminate and dispose the worker; later requests restart it. Timeouts preserve stdout/stderr and identify their log. Native progress checkpoints now identify the current level and phase. Process-level tests cover timeout, crash, and between-request exit recovery.

The recent Obsidian 11 locked-door prerequisite fix exposed a mutually dependent group of locked trigger doors in 1sttrial.rdl. The preceding planner completed its metadata but skipped that prerequisite. The new planner repeatedly searched the graph while reusing cached visibility, so its existing analysis work budget did not bound this expensive search. Graph searches now charge their segment count to that budget, and dependency recursion stops when it is exhausted. The level returns valid metadata with a partial route and an explicit work-budget reason; its full route remains unresolved. No mission exclusion or timeout increase was added.

Diagnostic source modifications and temporary search-order experiments were removed. Native binaries must be rebuilt with the final source before validation.

## Validation complete

- Windows D1/D2 builds pass
- All 49 D2 CTest tests pass, including graph-search budget coverage
- Process-level timeout, crash, and exited-worker recovery tests pass with preserved diagnostics
- Existing host metadata workspace tests pass
- Dimensions and Destination Saturn regenerate successfully: 2 sources passed, 0 failed
- Built-in First Strike, built-in Counterstrike, and Obsidian metadata regenerate successfully
- Core 88-level simulation statuses are unchanged, including successful Obsidian 9 and 11
- Scoped code quality and git diff checks pass

Validation metadata was written under android/temp with NoRegressionCopy. The full archive corpus and Android device behavior were not tested. The checked-in user-generated CD data and earlier Obsidian edits were preserved.

Evidence: android/temp/metadata_worker_recovery_cd_final, android/temp/metadata_worker_recovery_base_final, android/temp/metadata_worker_recovery_core, temp/metadata_worker_recovery_ctest.log, and android/temp/test_host_metadata_worker
