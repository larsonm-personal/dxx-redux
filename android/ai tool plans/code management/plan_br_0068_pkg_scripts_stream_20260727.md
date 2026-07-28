# BR-0068 PKG Scripts stream

## Goal

Require the selected XAR Scripts member to have valid bounded metadata and a
complete gzip and CPIO stream before package contents are accepted.

## Plan

- [x] Read repository instructions, review process, and the complete finding
- [x] Trace TOC selection, member bounds, gzip reads, and CPIO termination
- [x] Implement checked member metadata and fail-closed stream completion tests
- [x] Run scoped code quality, extraction tests, native suites, and Android builds
- [x] Record the resolution and move BR-0068 to the done ledger

## Verification

- Scoped code quality passed
- All 17 extraction host tests passed, including the expanded PKG corpus
- All 24 D1 and 28 D2 native host tests passed
- Android debug builds passed for arm64-v8a, armeabi-v7a, and x86_64 with JDK 21
- Both retail Mac PKGs extracted 7 and 15 files respectively, with every size
  and SHA-256 matching `game_data/gog_extraction_results.json`
