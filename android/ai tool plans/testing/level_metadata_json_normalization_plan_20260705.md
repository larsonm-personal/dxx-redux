# Level Metadata JSON Normalization Plan

## Goal
Keep generated mission metadata JSON stable and readable so future diffs show content changes instead of serializer churn.

## Tasks
- [x] Locate the generation path that writes `game_data/mission_files/*.json`.
- [x] Replace PowerShell `ConvertTo-Json` pretty output for mission metadata with a first-pass canonical writer using fixed two-space indentation.
- [x] Replace the first-pass writer with a dedicated JSON normalizer/formatter script.
- [x] Preserve producer key order and array order so future diffs stay close to the Kotlin metadata schema.
- [x] Reformat existing touched metadata files through the same formatter.
- [x] Run focused metadata generation/format checks.
- [x] Update this plan with completed work and remaining risks.

## Notes
- The Kotlin launcher metadata producer already emits fields in a stable schema order. The churn came from the PowerShell batch helper reparsing the app JSON and writing it back with `ConvertTo-Json`.
- The first pass used `ConvertFrom-Json` plus `ConvertTo-Json`, which caused array indentation to drift under long property names.
- The batch helper now calls `android/helpers/normalize_json.py`, which uses Python's standard JSON parser and serializer with two-space indentation.
- This intentionally avoids alphabetical key sorting for now because that would create a large one-time rewrite and make schema-oriented diffs harder to read.
- `Descent.json`, `Obsidian.json`, and `Counterstrike.json` were run through the formatter. `KCXF2RMv11.json` already matched it.
