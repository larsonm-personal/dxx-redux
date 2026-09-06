# Disc import content

- Trace CD extraction and file-set content ownership
- Filter extracted installer files, consumed archives, and recordings before publication
- Publish disc add-on content as one durable owner while preserving base files and mission launch paths
- Cover import, reconciliation, projection, toggle, and deletion with integration tests
- Run scoped formatting, Android unit tests, and relevant build checks

## Implementation

- BIN/CUE and ISO publication filters isolated extraction output before it reaches the active set
- Consumed SOW archives, executables, configuration, recordings, documentation, and standalone screenshots are discarded; explicitly referenced mission assets are retained
- Existing content ownership stores the disc add-ons as one group, preserving each mission's launch paths
- Base files remain in the set root; existing CD audio registration remains separate
- Publication holds the content lock so reconciliation cannot adopt a partially written group
- Content-derived identities preserve enabled state on identical re-imports without replacing unrelated content state
- Existing loose imports are not migrated; use a fresh set to replace an old import

## Verification

- `android/tests/test_disc_content_import.ps1`: 40 passing tests covering publication, filtering, ownership, re-import, toggling, and deletion
- `android/tests/test_cue_iso.ps1`: relevant CMake build and all 44 native tests passed
- Scoped mixed-language formatting/lint passed; formatted the new JVM test directly because the wrapper restricts Kotlin checks to main sources
- Debug x86_64 APK built successfully; current AGP output is `android/app/build/intermediates/apk/debug/app-debug.apk`
- Final real BIN/CUE import passed `test_extract.ps1 -SpecPath 'game_data/CD images/Descent II Infinite Abyss/extract_regression.jsonc' -KeepFiles` and launched Ahayweh Gate
- Setup introspection confirmed exactly one `DESCENT II ABYSS` D2 group containing only `D2-2PLYR.HOG`, `D2-2PLYR.MN2`, `D2CHAOS.HOG`, and `D2CHAOS.MN2` under `missions/`
- Final device assertion artifact: `android/temp/disc-content-device-verification.json`; launch log: `android/temp/disc-content-device-test.log`
