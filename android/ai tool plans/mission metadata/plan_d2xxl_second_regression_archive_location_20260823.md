# D2X-XL second regression archive location

Goal: include `game_data/mission_files/d2xxl_downloads` as a second mission archive source for regression runs while tracking its generated regression JSON and ignoring downloaded archives.

- [x] Identify the authoritative mission archive source configuration and current ignore rules.
- [x] Add the D2X-XL download directory as a second regression source.
- [x] Update ignore rules so archives remain local and generated regression JSON is tracked.
- [x] Add or update focused discovery coverage.
- [x] Run scoped tests and code quality, then record the verified result.

Result: both the emulator and host metadata regeneration wrappers use the same
two-source configuration. Each source writes regression JSON beside its own
archives, preventing duplicate archive names from colliding. Local discovery
found 110 primary archives and 18 D2X-XL downloads. A focused host run of
`harqyjia.7z` passed and produced `d2xxl_downloads/harqyjia.json`; Git reports
that JSON as untracked while the neighboring 7z remains ignored.
