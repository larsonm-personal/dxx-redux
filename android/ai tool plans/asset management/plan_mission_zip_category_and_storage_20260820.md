# Mission ZIP category and storage fixes

## Goal

Classify mission ZIPs such as Obsidian and Plutonian Shores as levels in the
Mods/Levels chooser, and prevent valid mission ZIP imports such as Castaway
Redux from reporting insufficient storage when the destination has room.

## Work plan

- [x] Reproduce both failures from the import classification and storage code
- [x] Correct mission ZIP category derivation without misclassifying true mods
- [x] Correct mission ZIP storage accounting and preserve rollback cleanup
- [x] Add focused and high-level regression coverage
- [x] Run scoped code quality, focused tests, and the Android debug build

## Status

Mission archives already persisted the `levels` category, but the chooser rendered
every `ModInfo` in its Mods group. The chooser now partitions mission/level rows
and supports ordering within each visible group.

Castaway Redux contains about 183 MiB of uncompressed data, so its extraction
request is not intrinsically too large. Phone logs showed that its mission probe
instead rejected the whole 184 MB archive at the 16 MiB self-extractor preamble
limit, then routed it as generic content. The URI-aware probe now uses the known
archive size, remains capped at the 2 GiB extraction limit, and cleans its staged
copy. Storage checks also record their resolved filesystem path and byte counts
and treat a zero capacity reading as unknown. Transactional temporary-file
cleanup continues to handle genuine write failures without leaving orphaned
files.

The real Castaway emulator check then exposed a file-set path mismatch in
metadata lookup: durable extraction lived under `.content/mod_support`, while
lookup assumed the legacy global mods root. Lookup now resolves the file-set
support root, so metadata uses the durable 192 MB HOG instead of trying the
small-entry ZIP staging path.

## Verification

- Scoped `run-code-quality.ps1 -Fix` passed for all changed Kotlin and plan files
- `ArchiveInputStreamsTest`, `ImportStorageGuardTest`, `MissionZipTest`,
  `ModManagerMissionZipTest`, and `LevelMetadataTargetsTest` passed
- `:app:assembleDebug` passed, including all three Android native ABIs
- The fresh-APK emulator batch imported the real `castaway_redux.zip`, analyzed
  its level metadata through the durable extraction, and passed all 11 steps
