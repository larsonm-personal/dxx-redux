# Overnight tests and regression regeneration

## Scope

- Run the exhaustive unattended Android test suite, including full extraction, route, graphics, and multiplayer coverage
- Run all canonical regression regeneration categories serially after the suite
- Investigate failures from durable logs, implement focused fixes, and rerun affected checks
- Review regenerated data and record final results and remaining limitations

## Execution

1. Read repository guidance, inventory active processes, and preserve the existing user edit in `android/android_features.md`
2. Wait for the already-running release build before starting another Gradle build in this checkout
3. Set JDK 21 and explicitly target the managed emulator
4. Run `pwsh -NoProfile -NonInteractive -File android/run_all_tests.ps1 -Serial emulator -FullSuite -FullExtracts -ExtendedGraphics -ExtendedMultiplayer`
5. Run `pwsh -NoProfile -NonInteractive -File android/regenerate_all_regression_data.ps1 -Category All`
6. Inspect failures, fix demonstrated defects, apply scoped code quality, and verify relevant builds and tests

## Progress

- Repository instructions read; full test and regeneration entry points confirmed
- Initial working tree contains only the user's `android/android_features.md` modification
- Preflight found an active release build, a separate store-assets emulator, and approximately 14 GiB free disk space
- Existing release build finished; exhaustive suite started at 2026-10-05 22:13 PDT
- Suite selected 274 runnable entries, with 6 manual and 13 profile exclusions; 368 support scripts execute through their owners
- Full demo coverage includes 36 replays across D1, D1-in-D2, and D2, with both headless and graphics runs
- Serial orchestration and stage logs are under `temp/overnight_20261005_2040`; test reports retain their standard `temp/test_reports` location
- Debug and instrumentation APK build passed in 4m 23s; managed emulator startup, health, game-data provisioning, and launcher preflight passed
- Host execution reached test 38 with 36 passes, one expected Linux-only dependency transaction skip, and no failures
- AcoustID configuration and regeneration checks passed; all Play, GitHub, legacy, and CI APK distributions passed package/SDK/engine/ABI inspection
- DOS MIDI parity, early route checks, extraction safety, and initial fingerprint regression checks passed
- User requested a disk-management break at 22:40 PDT; overnight goal and owned processes were paused/stopped before regression regeneration
- Partial results at interruption: 43 passed, one expected Linux-only skip, no reported failures; test 45 (`test_github_release`) was in progress
- Full partial console evidence is preserved in `overnight-tests-20261005.partial.txt`; old temporary runner/report files are disposable during requested cleanup
- Goal resumed on 2026-10-06 after disk-management work; no previous test/build runner or emulator remains live, with 192.18 GiB free at restart
- Starting a fresh exhaustive run to obtain a complete report on the current worktree; serial orchestration logs are in `temp/overnight_20261006_065911`
