# Co-op join with a different built-in edition

The Retroid incident joins a full D1 level 10 host with the seven-level demo. Both editions resolve to the same mission key, so the client reaches LoadLevel's range assertion

1. Fingerprint campaign HOGs with the same MissionContentIdentity SHA-256 implementation used for mission ZIPs; use one size/hash comparison and retain the built-in warning's ability to ready and join
2. Validate the loaded mission's actual native level range before joining and after any join-wait destination update, for both engines; show an actionable error and close the failed connection
3. Extend the mission admission/protocol tests, build both native engines and Android, and reproduce an unavailable-level join on disposable emulators

Existing mission-pack size/hash checks and post-load level checksum rejection remain in place

## Result and validation

Campaign HOGs and mission ZIPs now populate the same content size/hash fields and pass through MissionCompatibilityResolver.resolveContent. Built-in differences preserve the advisory MATCH/Ready policy; mission ZIP mismatches still require synchronization. Removed the level-count wire field and comparison. A bounded size/mtime cache avoids rereading campaign archives on each lobby refresh

LAN and online protocols advertise the campaign's size and SHA-256. Server validation shares the built-in/wrapper identity checks while prohibiting campaign downloads. Online advisory warnings are forwarded with mission status. The native guard independently uses the loaded mission's actual regular and secret level bounds, including join-wait destination changes

Validation of the initial native guard:

- Windows D1 and D2 builds passed with run-windows-build.ps1
- Android x86_64 debug APK and focused mission/protocol JVM tests passed, including LAN metadata round trips
- test_coop_join_missing_level.ps1 -SkipInstall passed for both engines on two disposable emulators: full D1 level 10 host, seven-level demo client, actionable error, native OK dismissal, unchanged client PID, and successful level 10 retry after restoring full data
- Scoped formatting/lint, automation ownership validation, test catalog integration, and git diff --check passed
- Client HOG/PIG restored after testing

Evidence: temp/coop_join_missing_level_run.log, temp/coop_join_missing_level/{d1,d2}-error.json, temp/coop_join_missing_level/{d1,d2}-logcat.txt, temp/coop_join_android_final_build.log, temp/coop_join_windows_build_retry.log

The identity warning requires updated peers; online lobbies also require the updated server. The client-side crash guard independently protects engine joins. The compared campaign container is descent.hog, descent2.hog, or d2demo.hog; this does not fingerprint every separate texture/sound/mod file

Unified identity validation passed:

- 28 focused JVM tests, including equal-size/equal-level-count mutations, D2 full/demo HOG selection, managed mission ZIP identity, and LAN/online metadata serialization
- Android debug and instrumentation APK builds, including both native engines
- Server mission tests: shared builtin/wrapper validation, hashed builtin lobby creation and warning forwarding, rejection of base-game download offers even with a valid identity, and existing mission readiness enforcement
- Extended two-emulator regression passed in both engines: advertised size/SHA-256 matches the actual campaign HOG, demo mismatch warns, Ready remains available, matching data clears the warning, a same-size HOG mutation warns, restoring the original clears it, missing level 10 shows a safe native error, and importing full data allows the retry
- Host migration/adoption and former-host rejoin instrumentation passed
- Scoped formatting/lint, automation catalog validation, test catalog integration, and diff whitespace checks passed
- Test client files restored

The live regression exposed the direct-hosting name-only fallback. It now resolves through the same MissionCatalog/MissionScanner identity path as UI hosts, including migration fallback

Unified evidence: temp/coop_identity_android_final_build.log, temp/coop_identity_server_tests.log, temp/coop_identity_lan_test.log, temp/coop_identity_migration_test.log, and temp/coop_join_missing_level/*-lobby-*.json
