# Extracted mission music preview fix

## Goal
- Fix music previews for mission archives that are imported as extracted bundles, with KCXF2RM as the test case

## Plan
- [x] Reproduce or isolate the extracted-directory preview failure
- [x] Make extracted music catalogs produce stageable preview tracks
- [x] Add regression coverage for extracted-directory music preview staging
- [ ] Run focused tests, assemble, and scoped code quality

## Notes
- Extracted music catalog song-list matching now falls back to leaf filenames, so references such as `music/level01.ogg` resolve to playable extracted tracks
- Preview staging now avoids reusing empty cached files for unknown-size tracks
- Added `MissionZipMusicExtractedPreviewTest` to stage a DXA-contained OGG from an extracted directory-backed catalog
- Verification is pending because the local PowerShell tool is currently failing to start with process exit code `-1073741502`
