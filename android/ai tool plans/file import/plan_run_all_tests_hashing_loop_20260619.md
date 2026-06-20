# run_all_tests hashing loop

Goal: investigate and fix a `run_all_tests.ps1` startup loop that repeatedly
prints hashing messages for what appears to be a Descent 1 imported file.

Plan:

1. [done] Locate the hashing log source and the run_all_tests test tier
   that triggers it.
2. [done] Reproduce the smallest relevant command path.
3. [done] Patch the import/hash logic so unchanged imported files are not
   repeatedly hashed.
4. [done] Add or update a focused test for the fixed behavior.
5. [done] Run scoped validation and code quality.

Notes:

- Initial search points at `AssetManifest.kt`, which stores imported file
  SHA-256 values and has a re-hash path for entries without hashes.
- Static tracing shows `run_all_tests.ps1` preflight calls
  `Resolve-GameDataDeps`, then starts `SetupActivity`.
- Fix direction: have `Resolve-GameDataDeps` seed `assets.json` in app-private
  dependency targets using the already-known SHA-256 and size. That avoids
  making the launcher re-hash standard test data every unattended test run.
- Added `test_game_data_asset_manifest_writer.ps1` to exercise manifest
  generation without an emulator, including Windows PowerShell JSON array
  behavior.
- `run_all_tests.ps1 -Filter test_game_data_asset_manifest_writer` passes.
- `run_all_tests.ps1 -Filter test_double_launch -StopOnFail` now gets through
  suite preflight and `SetupActivity` readiness without a hashing loop. The
  selected test still fails later on its game screen/PID expectation, which is
  outside this file-import issue.
