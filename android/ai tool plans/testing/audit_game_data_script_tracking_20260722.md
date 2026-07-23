# Audit game_data script tracking

## Goal

Identify scripts and helpers under `game_data/` that should be saved in source
control but may currently be excluded by repository ignore rules.

## Plan

- [x] Create the audit plan
- [x] Read repository instructions and applicable ignore rules
- [x] Inventory scripts and helper-like files under `game_data/`
- [x] Classify authored workflow code versus generated or local-only files
- [x] Identify files whose ignore treatment conflicts with their role
- [x] Report files to save without running Git commands or changing tracking state

## Findings

- No additional authored script appears to require saving after
  `run_all_cd_regressions.ps1`
- All established top-level PowerShell tools appear in the completed
  adversarial-review source inventory, which confirms they were already part of
  the maintained repository source set
- Maintained scripts and libraries under `game_data/mods/d2x-xl`,
  `game_data/mods/xfing`, and `game_data/mods/xfing/d2x_sp` are covered by
  explicit root and nested `*.ps1` exceptions
- Scripts under extracted `data_tracks` trees are original disc artifacts, not
  repository tooling
- `game_data/mods - backup` duplicates the maintained mod conversion scripts
- The two `inspect_xfing_*.ps1` files live under an ignored `tmp` work tree and
  are one-off inspection utilities rather than maintained workflow dependencies
- No Git command or ignore/tracking mutation was performed during this audit
