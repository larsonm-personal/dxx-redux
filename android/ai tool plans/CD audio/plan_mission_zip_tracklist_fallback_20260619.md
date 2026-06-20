# Mission ZIP tracklist fallback

## Goal
- Allow committed mission soundtrack tracklist sidecars to name tracks when AcoustID has no match

## Plan
- [x] Define a small `.tracklist.json` format for mission ZIP soundtracks
- [x] Add the KCXF2RMv11 sidecar from the readme track list
- [x] Teach `fingerprint_mission_zip_music.ps1` to apply tracklist names after failed AcoustID lookup
- [x] Regenerate KCXF2RMv11 chromaprint data and update known discs
- [x] Run scoped validation and mark the plan complete

## Results
- Added `game_data/mission_files/KCXF2RMv11.tracklist.json` using schema `dxx-mission-tracklist-v1`
- Tracklist entries can match tracks by `slot`, `level`, `filename`, `source_path`, or `original_name`
- `fingerprint_mission_zip_music.ps1` now uses the sidecar only after cached or online AcoustID naming fails
- `update_known_discs_albums.ps1` carries `tracklist_name` and `name_source` into `known_discs.json5`
- Forced `KCXF2RMv11.7z` fingerprint pass completed with 8 tracklist fallback names and 0 failures
- Scoped code quality, PowerShell parse checks, and direct ScriptAnalyzer run completed
