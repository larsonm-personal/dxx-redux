# Remove mod detail MD5 plan

## Goal
- Remove MD5 fields and display from the launcher mod details and known-version database
- Keep SHA-256 as the single compatibility and diagnostic hash
- Preserve the non-hash popup fixes: full path, wrapping, first-three examples, and patch problem wording

## Tasks
- [x] Inspect current MD5-related code and generated data
- [x] Remove MD5 from KnownVersions, ModManager, SetupActivity, and tests
- [x] Revert known_versions.json5 and hash_assets.ps1 to SHA-256-only generation
- [x] Run formatting and focused validation

## Validation
- `android\stop-stale-formatters.ps1`
- `android\run-code-quality.ps1 -Fix`
- `android\run-code-quality.ps1`
- `git diff --check`
- `cd android; .\gradlew.bat ':app:testDebugUnitTest' --tests 'com.dxxredux.app.ModManagerDetailsTest'`