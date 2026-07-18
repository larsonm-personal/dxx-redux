# Mission metadata numeric canonicalization

## Goal

Make checked-in mission metadata byte-stable whether it is generated through the Android/emulator path or the host/headless path, and prevent ordinary test runs from modifying source fixtures.

## Plan

- [x] Trace the `anachron.json` decimal-only diff to the command and artifact that wrote it.
- [x] Compare the Android and host serializer output and identify schema-level numeric differences.
- [x] Add a shared mission-metadata numeric canonicalization mode used by both regeneration helpers.
- [x] Make the mission ZIP smoke test explicitly disable regression fixture writes.
- [x] Verify focused normalization, script parsing, and a second-pass byte-stability check.

## Findings

- `game_data/mission_files/anachron.json` was written at `2026-07-18 03:50:50`, exactly when `android/temp/mission_zip_batch/20260718_035002` completed its third sample.
- `android/tests/test_mission_zip_batch.ps1` called the reusable batch helper without `-NoRegressionJson`, so the smoke test copied generated results into `game_data/mission_files`.
- The host generator's nlohmann/JSON output preserves integral doubles (`514.0`); Android's org.json reconstruction renders the same double as `514`.
- The values are semantically identical, but generic parsing/pretty-printing cannot reconcile the two lexical forms because Python preserves the parsed integer-versus-float type.
- Existing checked-in metadata consistently treats route coordinates, route distances, mine volumes, and travel distances as floating-point schema fields. Canonicalizing those fields to floats preserves the established fixture format without a repository-wide decimal-removal churn.

## Verification

- `test_mission_metadata_json_normalization.ps1` passes and proves Android-style integers and host-style integral floats converge to identical output.
- A second normalization pass is byte-identical to the first.
- The July 18 Android `anachron.json` artifact and July 17 host artifact canonicalize to identical bytes.
- Canonicalizing the accidentally rewritten checked-in `anachron.json` reproduces the committed file byte-for-byte.
- All modified PowerShell scripts parse without errors and `git diff --check` passes.
