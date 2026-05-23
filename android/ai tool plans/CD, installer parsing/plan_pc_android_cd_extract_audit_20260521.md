# PC vs Android CD extract audit

## Status

- [x] Identify the main PC-side and Android-side extractor entrypoints
- [x] Inventory the PC-side supported source media and regression specs
- [x] Map each source class to the corresponding Android import path
- [x] Compare per-class behavior and identify support gaps or regressions
- [x] Summarize findings and recommended follow-up work

## Notes

- Scope: audit-only pass focused on CD-image extraction support parity between the desktop extractor and Android import flows
- Goal: determine which PC-side supported CD source classes are already supported on Android, which are partially supported, and which still lack a matching Android path

## Findings

- Shared native support is mostly aligned now: desktop `extract_cd` and Android JNI both use the same CUE parser, ISO reader, Mac HFS/STi2 fallback, and recursive `.sow` handling
- Desktop-supported source classes observed in `game_data/CD images/`: CUE-backed raw sector images, standalone ISO images, Mac HFS CDs, recursive `.sow` installer layouts, and multi-FILE CUE sheets with many track files
- User-facing Android disc import is narrower than the native extractor surface:
	- The picker recognizes `.cue`, `.iso`, `.inst`, `.gog`, and `.bin`, but not `.img`; this leaves the CUE+IMG Quartzon sample outside the normal Android import path even though the shared CUE parser and desktop extractor can handle it
	- The picker stores multi-BIN selections in raw picker order and indexes them by `fileIndex`, with no reordering by CUE `FILE` entries; this makes multi-FILE CUE support dependent on selection order
- Android setup-command and filesystem-path import are narrower than the SAF UI path:
	- `import_cd` takes one `cue_path` and one `bin_path`
	- `importDiscImageFromPath()` rejects data tracks whose `fileIndex` is not `0`, so multi-FILE CUEs are not supported through that path even though desktop `extract_cd` supports them
- Android extraction regression coverage is thin:
	- Only the MacPlay spec exercises `setup_cd`
	- Only the standalone ISO spec exercises `setup_iso`
	- The rest of the extraction specs still validate extracted outputs by pushing files directly, so Android extractor parity is not currently covered for most retail and OEM disc layouts

## Follow-up

- First parity tranche should widen Android source discovery and direct-import plumbing to match the shared parser surface: accept `.img` alongside `.bin`, preserve CUE `FILE` order, and make `import_cd` support multiple track-file paths
- Second tranche should add Android direct-import regression specs or harness support for at least one multi-BIN retail disc, the CUE+IMG Quartzon sample, and one additional non-Mac `.sow`-heavy disc such as Test Flight or the 3-Level Preview