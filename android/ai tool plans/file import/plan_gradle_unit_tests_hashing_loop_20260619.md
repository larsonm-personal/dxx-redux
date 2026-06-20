# test_gradle_unit_tests hashing loop

Goal: determine why `test_gradle_unit_tests` still emits rapid repeated
hashing messages and fix the real source, whether it is launcher code,
Gradle/unit-test setup, or noisy test fixture generation.

Plan:

1. [done] Confirm what `test_gradle_unit_tests.ps1` actually runs.
2. [done] Run the filtered test and capture the exact repeated hashing
   output.
3. [done] Trace the output to either launcher hash code, unit-test fixture
   setup, or game-data index generation.
4. [done] Patch the real defect or reduce intentional test noise so it is
   not confused with a launcher loop.
5. [done] Re-run the filtered test and scoped code quality.

Notes:

- `test_gradle_unit_tests.ps1` runs only Gradle `:app:testDebugUnitTest`.
  It should not require emulator preflight or visible SetupActivity hashing.
- Filtered `test_gradle_unit_tests` did not reproduce hashing output here, but
  a real launcher defect was identified: duplicate `assets.json` entries can
  make `findStaleFiles()` see one entry while `upsert()` replaces another,
  causing repeated hashing of the same file.
- `AssetManifest` now canonicalizes entries by lowercase filename, prefers
  entries whose size matches the file on disk, and removes all duplicates on
  upsert/save.
- Added `AssetManifestTest` for duplicate manifest entries and reran
  `:app:testDebugUnitTest`, `run_all_tests.ps1 -Filter test_gradle_unit_tests`,
  the manifest writer PS test, and scoped code quality.
