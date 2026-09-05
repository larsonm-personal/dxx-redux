# Mission launch cache

- Confirm repeated launch work in mission staging and extraction freshness checks
- Keep large-pack extraction at import and small-pack extraction lazy until launch
- Reuse owner-linked extraction generations with size/time checks; retain explicit full hash verification for integrity diagnostics
- Route mission metadata and Advanced file links through the same extraction owner records
- Add import/launch/relaunch/disable/delete coverage and verify Kotlin tests, scoped formatting, Android build and emulator launch

Evidence: ModManager deletes generated mission ZIP directories on every launch. MissionZipExtractionStore.freshRecord hashes the source archive and every extracted file on each call, including launch, music and metadata queries

Managed payloads are immutable between imports. Import/replacement invalidates the owner cache. Size/time checks detect ordinary external changes; explicit full verification detects edits that preserve both size and modification time

## Implemented design

- Preserve the existing import policy: archives above 10 MiB, non-ZIP archives, and large nested containers use extraction during import
- Small ZIPs use the same owner store lazily on first launch, sharing a generation between D1 and D2 instead of rebuilding per-game directories
- Persist each extracted file's modification time alongside its size and content hash; launch and metadata lookups only stat files, while freshRecord remains a full integrity audit
- Leave soundtrack fingerprinting to RouteMetadataPrecomputeCoordinator; launch mounts its existing music-name sidecar without catalog scanning, decoding, or fingerprinting
- Fix scoped owner lookup: payload ZIPs live in .content/mods, while derived files live in .content/mod_support/mods/.extracted_mission_zips
- Extraction manifest schema changes directly to v4; existing generations are rebuilt on demand under the repository's disposable pre-release format policy
- Log owner, cache hit, file count, expanded size and elapsed milliseconds for each mission launch preparation

## Validation completed

- Scoped mixed-language formatter/lint and git diff --check passed
- Android x86_64 CMake build and debug APK packaging passed
- 106 targeted JVM tests passed, covering mod imports, extraction ownership/reuse/repair, metadata targets and music behavior
- Reusable test_mission_launch_cache.jsonc passed all 16 steps for both D1 and D2, including two launches across launcher process restarts and owner/cache deletion
- Actual ewithin-rebirth.zip from ewithin-versions.zip: 408845824 archive bytes, 432600851 extracted bytes, four owned files; import took approximately 11.4 seconds; both D2 launches and deletion passed
- Enemy Within cache lookup: 1 ms on both launches; total launcher preflight: 229 ms and 442 ms
- Uneasy 4 cache lookup: 1 ms and 3 ms; total D2 launcher preflight: 183 ms and 733 ms
- D1 launcher preflight: 113 ms and 204 ms
- Measurements are from the emulator with background metadata precompute paused by the automation script; they are not phone timings or total time to the game menu
- Build log: android/temp/mission-launch-build-final.log; timing logs: android/temp/mission-launch-ewithin-preflight.log and android/temp/mission-launch-d2-preflight.log
