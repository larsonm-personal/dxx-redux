# Game data index canonicalization

## Goal
Explain the current large `game_data/game_data_index.txt` diff and prevent Windows/Linux regeneration oscillation.

## Plan
- [x] Inspect the current diff and generator behavior
- [x] Identify whether changes are real content, ordering, casing, or line endings
- [x] Add or adjust canonicalization if needed
- [x] Run targeted validation and update this plan

## Findings
- The large diff is mostly duplicate-hash path selection, not changed file bytes. The old generator kept whichever duplicate path `Get-ChildItem` returned first.
- `game_data_to_copy_to_emulator/temp` was first in the search list, so copied scratch files could replace stable source paths when the index regenerated.
- The current local data set adds 45 new hashes relative to `HEAD`; no hashes were removed.
- Line endings are not the cause. The repo already normalizes text to LF, and the regenerated index is UTF-8 without BOM plus LF-only lines.

## Validation
- Ran `game_data/generate_game_data_index.ps1` twice; the second run produced a byte-identical `game_data/game_data_index.txt`.
- Ran direct `Invoke-ScriptAnalyzer` on `game_data/generate_game_data_index.ps1`; no warnings or errors.
- Ran scoped `android/run-code-quality.ps1 -Paths game_data\generate_game_data_index.ps1`; all checks passed, though the aggregate script did not classify this non-android PowerShell file as a PSScriptAnalyzer target.

## Follow-up
- Superseded by `plan_game_data_index_minimal_test_deps_20260526.md`: default generation now writes only declared runtime test dependencies, while `-AllLocalFiles` preserves the full local inventory mode.
