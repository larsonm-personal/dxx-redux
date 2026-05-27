# Minimal game data index

## Goal
Limit `game_data/game_data_index.txt` to the minimum hash set needed by tests, instead of indexing every locally present archive or recently extracted CD.

## Plan
- [x] Find every test dependency source that uses SHA-256 game data lookup
- [x] Compare required hashes with the current generated index
- [x] Teach the generator to include only required hashes while still resolving paths from local data
- [x] Regenerate the index and validate stability/lint

## Findings
- `game_data/game_data_index.txt` is used by runtime/game automation dependency provisioning, not by extraction specs. Extraction tests read their own `source_files` entries directly.
- Runtime dependency sources currently declare 15 unique hashes: 11 standard D1/D2 files plus 4 mod-loading DXAs.
- The local checkout can resolve the 11 standard files. The 4 mod DXAs are declared by `test_mod_loading.json5` but are not present locally, so the generated index warns and omits them.

## Implementation
- `game_data/generate_game_data_index.ps1` now defaults to declared test dependencies only.
- `-AllLocalFiles` keeps the previous full local inventory mode for investigation.
- Duplicate path selection now also prefers directories that contain more required hashes, so D2 host replay dependencies stay in a single source directory where possible.

## Validation
- Minimal generation is byte-stable across repeated runs.
- `-AllLocalFiles` still generated the full 307-entry local inventory, then default generation returned the index to 11 present entries.
- D2 host replay hashes resolve to one directory, and D1 host replay hashes resolve to one directory.
- Direct `Invoke-ScriptAnalyzer` on the generator had no warnings or errors.
- Scoped `android/run-code-quality.ps1 -Paths game_data\generate_game_data_index.ps1` passed, though the aggregate script still does not classify this non-android PowerShell file as a PSScriptAnalyzer target.
- `git diff --check` passed for the touched files.
