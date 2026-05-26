# D2 Redbook MP3 Manifest Plan

## Goal
- [x] Add D2 redbook MP3 files to the game data test manifest
- [x] Update `game_data/manage_data.ps1` so list, verify, regenerate, and export include those files
- [x] Validate the updated script and manifest against local files

## Notes
- Kept the scope limited to the two fixture directories used by `test_fpcalc_and_acoustid.ps1`
  - `game_data/music/D2 infinite abyss redbook mp3/`
  - `game_data/music/D2 redbook mp3 rips/`
- Use a distinct manifest kind so the audio regression fixture set is easy to filter later
- `manage_data.ps1 -Action Regenerate` wrote 264 entries, including 22 `d2_redbook_mp3` entries
- `manage_data.ps1 -Action Check` verified all 264 entries
- Export smoke-tested with a temporary 22-entry MP3-only manifest
