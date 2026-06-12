# Mission JSON stale level name diff

## Goal
- Explain why `game_data/mission_files/outerrch11.json` still shows a bad `level_name` diff toward `Militari`.
- Find whether the checked file, generated artifact, or git index is stale.
- Fix the current worktree state so the full `Militaries` title is preserved.

## Plan
- [x] Inspect `outerrch11.json` and its git diff
- [x] Check whether generated metadata artifacts still contain the truncated title
- [x] Confirm the native metadata build contains the header-peek fix
- [x] Restore or regenerate the correct baseline if needed
- [x] Run focused verification

## Result
- The bad checked diff matched `android/temp/mission_zip_batch/20260611_202500/metadata/outerrch11.json`.
- That artifact was written before the committed native `read_level_display_name()` fix.
- Restored `Defense Outpost for Orcean Militaries` in the checked baseline while preserving `mission_name = The Outer Reaches`.
