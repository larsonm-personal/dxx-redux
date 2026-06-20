# Mission tracklist sweep

## Goal
- Find mission archives with custom audio that still need committed tracklist sidecars

## Plan
- [x] Inventory existing mission chromaprint sidecars for unnamed tracks
- [x] Scan mission archives and embedded HOG files for audio files
- [x] Extract/read candidate readme files for tracklist text
- [x] Create `.tracklist.json` sidecars where mapping is clear
- [x] Summarize unclear or unprocessed candidates for manual decision
- [x] Run scoped validation and update results

## Results
- Scanned 112 mission archives for direct audio, nested archive audio, and HOG-contained audio
- Created sidecars for `castaway_redux`, `cererian_1.3`, `nefarious`, `Trine1`, `trine2`, and `ulterior_v1.0.6b`
- Regenerated chromaprint sidecars for those missions with `-Force -SkipAcoustId`
- Updated `known_discs.json5`: 32 albums, 558 tracks, 91 duplicates removed
- Remaining unnamed mission tracks:
  - `ewithin-versions`: 66 tracks, readme found but no title list beyond numbered playlist entries
  - `Trine1`: `briefing.ogg` and `credits.ogg`, not listed in `Trine1 music.txt`
  - `trine2`: `briefing.ogg` and `credits.ogg`, not listed in `Trine2 music.txt`
  - `U3AAH`: `U3AAH.ogg` and `Vapobloc.ogg`, no readme text found
- RAR archives `FFYL.rar`, `reetus.rar`, and `reetus2.rar` could not be opened by the available 7za build, despite having RAR5 headers
