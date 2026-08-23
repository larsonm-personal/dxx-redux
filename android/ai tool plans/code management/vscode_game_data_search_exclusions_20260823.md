# VS Code game data search exclusions

## Plan

- [x] Inspect workspace exclusions, Git ignore rules, and large `game_data` file types
- [x] Replace broad directory exclusions with targeted binary and generated-log exclusions
- [x] Verify JSON and JSONC files remain searchable while large payloads are excluded

## Follow-up verification

- [x] Reproduce the `midi track 0` query with JSONC-only and JSON-plus-JSONC filters
- [x] Confirm the expected matches are JSON files and are not excluded by workspace settings
