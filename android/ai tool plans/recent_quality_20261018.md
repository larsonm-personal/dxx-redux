# Recent-change quality validation, 2026-10-18

Scope: current checkout at 9302a5a4b, including pre-existing modified mission metadata. Preserve user changes and baseline assertions.

1. Build current Windows D1 and D2 and run both complete native CTest suites, emphasizing networking, recovery, save validation, parsers and matcen lifecycle
2. Run targeted host Python and PowerShell checks for recent matcen, security, nightly tooling and public first-play changes, recording skips separately
3. Build current Android APK and JVM tests; run focused current-source device basic-use, save compatibility and serial two-emulator co-op recovery/pickup/door checks
4. Investigate failures against previous validation evidence without weakening tests; record commands, exit codes, logs and limits in temp/recent_quality_20261018
5. Publish a concise review with actionable failures and precise coverage

Run device owners serially. Avoid metadata regeneration and baseline acceptance. Reuse connected emulators after identifying them and ensure no other active test runner is using them.

## Completed

- Current D1/D2 native builds and all 169 CTest registrations passed
- Current Android build and JVM suites passed with 1,214 passes and 2 explicit skips
- All 25 recently changed Python modules assessed across Windows/WSL: 412 passes and 2 remaining exclusions
- All 12 selected host owners, 6 Android owners and 8 focused co-op scenarios passed
- Controlled public matcen historical production, repeat/negative controls and fresh-process save continuation passed
- Initial tool-cache/compiler/symlink prerequisites and seven reboot-time co-op setup failures were resolved with original evidence retained
- Pre-existing canonical metadata diff stayed byte-identical; no product code, assertions or regression baselines changed
- Dedicated ports 5580/5582 shut down; original shared 5554/5556 remained connected

Review: [recent_quality_20261018_results/REVIEW.txt](recent_quality_20261018_results/REVIEW.txt)

The review records four remaining exclusions and earlier Android findings outside this focused run
