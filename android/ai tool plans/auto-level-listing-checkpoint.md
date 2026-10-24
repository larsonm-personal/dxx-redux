# Automatic level recording listing checkpoint

Scope: fix the recording runner's listing boundary and retain bounded failure evidence, without changing shared ADB/LAN behavior or native recording code

Plan:

- Use the existing Adb-Timeout diagnostic reference; reject timeout, process error, and nonzero/missing exit status before parsing stdout
- Validate two distinct direct recording paths, retain command diagnostics and scenario output, and verify both level headers before replay
- Add host-pure regression cases to public tooling checks; run scoped quality and catalog validation
- Verify the unchanged remote and publish a fast-forward to online-restoration only; keep PR 113 draft

Evidence supplied for run 39670827805 / job 119468255040 / artifact 12379209773: shareware:test_auto_level_cheats#d2 completed 151/151 native steps, then the host runner reported one recording. The old listing counted every nonblank line from Adb, which includes stderr and ignores exit status. This is a proven harness defect, not proof that this defect caused that historical failure. Raw historical listing/scenario artifacts were not retained. A genuine one-file successful listing must still fail. The combined D1/D2 staged count alone does not prove two D2 recordings; the exported headers must identify the expected engine and levels. No evidence supports a filename collision

No D2 online-preview dispatch, live-server probes/deployments, credential changes, merges, history rewrites, or cmake pushes are authorized by this checkpoint. Recording/replay results are not D2 online qualification

Implementation and local validation (2026-10-21):

- Shared test_helpers.ps1, LAN tests, native recording code, and JSONC scenario steps are unchanged
- The local listing helper consumes stdout and the existing diagnostic reference separately; rejects timeout, process error, missing/nonzero exit, invalid/wrong-game/nested paths, duplicates, and counts other than two
- Listing stdout is retained up to 8192 characters with serial, package, command arguments, exit, timeout, bounded stderr/error; the shared API does not expose partial timeout streams, so those remain unavailable rather than fabricated
- On failure, bounded automation result/log tails are retained alongside existing android.txt and guided-rejection.txt. Public CI copies only fixed recording-owner text filenames, up to 65536 bytes each, into results/auto-level-demos
- Both exported engine/level headers must pass before either strict replay starts. No new sleep/retry or weakened two-level/replay checks
- PASS: 14 host-pure listing/retention cases and the existing test_automation_result_evidence.ps1 (real structured ADB API controls)
- PASS: test_validate_automation_catalog.ps1; direct run_all_tests.ps1 -ListTests -HostOnly catalog discovery registers the new test with requires=none
- PASS: scoped mixed-language formatting/lint after installing the repository-pinned PSScriptAnalyzer and Prettier dependencies in this fresh workspace
- BLOCKED locally: test_run_all_tests_catalog.ps1 child supervisor exits 125 because /proc/self/task/<pid>/children is unavailable, including outside the restricted process sandbox. Direct discovery passes, but this does not replace the full catalog regression
- Device recording/replay has not run locally: no Android device or /dev/kvm is available. Public CI status will be recorded separately; local host checks do not establish device or D2 online qualification
- PASS: 72 tests in test_nightly_tooling_sweep.py and pinned actionlint 1.7.12 on the changed workflow
- Publication target: online-restoration, based on verified published 34c11af389643d2ccb51b092a2664565ac1c0bba; PR 113 restored to draft. The ordinary PR sample includes shareware:test_auto_level_cheats#d2 in its validation selection; this is recording/replay coverage, not online-preview
