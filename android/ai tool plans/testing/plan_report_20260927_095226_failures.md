# Report 20260927_095226 failures

1. Inspect the seven failed owners and their execution evidence
2. Reproduce and fix fingerprint process output, dependency/SDK publication, and runner fixture failures
3. Fix multiplayer polling against optional introspection state and run the multiplayer integration test
4. Investigate D2 replay divergence using existing checkpoints and diagnostics without weakening comparisons
5. Run scoped formatting, relevant builds, and affected regression tests; record remaining limitations

Initial evidence: two fingerprint assertions, two atomic replacement errors, one missing runner evidence function, one optional lobby property exception, and nine D2 replay result mismatches

## Changes and validation

- Both fingerprint tests pass on the current checkout; commit b0e4291b already fixed inherited Windows worker output after the reported revision 0e4a9004
- Runner fixture now loads the real evidence writer and creates an isolated evidence context, then verifies final TIMEOUT/FAIL/PASS observations; test_test_runner_result passes
- Multiplayer polling checks whether the optional lobby property exists before reading it under strict mode; test_mp -SoakSeconds 0 passes through both emulator launches, lobby, chat, ready state, and game launch (temp/report_failures_mp.log)
- Reproduced the dependency cleaner's Windows replacement error during retirement; both cleaners now share bounded retries for sharing/lock/replacement-denied errors, preserving atomic replacement and failing after three seconds
- test_managed_dependencies and test_sdk_package_cleanup pass, including forced-termination recovery, ownership protection, and repeated cleanup (temp/report_failures_dependencies.log and temp/report_failures_sdk.log)
- Extended test_host_metadata_workspace with real Windows file handles: transient readers allow eventual publication; persistent readers retain the old content and leave no scratch files; test passes
- Scoped formatting/lint passes
- Evidence writer concurrency/master-runner integration passes; PowerShell 5.1 parses all 428 scripts and passes compatibility helper checks; git diff --check passes

## Remaining D2 replay failure

The complete current D2 corpus still has the same nine historical failures and two passes. All 11 current terminal JSON results exactly match the supplied report's results (temp/report_failures_d2_comparison.json); full replay log is temp/report_failures_d2.log

The existing investigation in input demo, replay, determinism/d2_replay_routing_policy_20260926.md identifies legacy Guide-Bot behavior in these May recordings and explains why neither forcing Original mode nor adopting replay outputs is valid. No new engine defect was established by this repeat. Recordings, expectations, routing defaults, and failure classification remain unchanged; replacement recordings still require the documented capture/playback qualification
