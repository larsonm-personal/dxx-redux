# 7z mission import support

## Goal
- Add 7z archive compatibility for Android mod and mission imports, then generate metadata for `KCXF2RMv11.7z`

## Plan
- [x] Find the launcher archive abstraction and mission import tests
- [x] Add 7z handling in the app and supporting scripts without disturbing zip imports
- [x] Add or update focused tests for 7z mission archives
- [x] Run the relevant tests and metadata generation for `KCXF2RMv11.7z`
- [x] Add music chromaprint/AcoustID metadata if the archive contains audio tracks
- [x] Run scoped formatting or validation and mark this plan complete

## Results
- Added archive reading support for zip and 7z mission imports
- Generated `game_data/mission_files/KCXF2RMv11.json`
- Generated `game_data/music/Mission ZIP - KCXF2RMv11/chromaprint_info.json5`
- Noted that the mission descriptor references missing secret level `kcxf2_s0.rl2`; metadata now records the warning instead of crashing
