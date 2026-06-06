# Game File Metadata Support Plan

## Goal
- [ ] Add launcher info-page awareness for common Descent game files, starting with `.hog` and `.pig`, so users can see contents, scope, editor/built-in metadata when available, and practical compatibility hints.

## Survey Status
- [x] Read project instructions and existing mission-zip plans.
- [x] Inspect `game_data/levels/Uneasy4.zip`.
- [x] Inspect existing launcher mission zip and file detail UI.
- [x] Inspect existing HOG readers and PIG loader format references.
- [ ] Implement metadata parsers and UI.

## Uneasy4.zip Findings
- `game_data/levels/Uneasy4.zip` contains three top-level files:
  - `Uneasy4.dxa`: 30,794,814 bytes, stored uncompressed in the outer zip.
  - `Uneasy4.hog`: 3,714,035 bytes, compressed to 1,235,631 bytes in the outer zip.
  - `Uneasy4.mn2`: 122 bytes.
- `Uneasy4.mn2` parsed metadata:
  - title: `Uneasy 4`
  - type: `normal`
  - levels: 1
  - level file: `Uneasy4.rl2`
  - author: `Blarget 2 and Nightsurfer`
  - editor: `Inferno 1.0.22`
  - detected game: D2.
- `Uneasy4.hog` is a valid HOG archive with magic `DHF` and five entries:
  - `descent.txb`, 14,718 bytes: encoded briefing/text.
  - `descent2.ham`, 1,382,082 bytes: robot, object, weapon, or gameplay data.
  - `Uneasy4.rl2`, 1,283,633 bytes: D2 level.
  - `Uneasy4.ied`, 94,305 bytes: Inferno editor source/project file.
  - `Uneasy4.pog`, 939,209 bytes: texture override pack.
- `Uneasy4.dxa` opens as a ZIP-format DXA with three entries:
  - `descent.sng`, 80 bytes: song list.
  - `descent2.s22`, 5,213,080 bytes: 22 kHz sound effects.
  - `Uneasy4.ogg`, 27,072,617 bytes: Ogg music/audio.

## Existing Support
- `SectorgameMissionZip.kt` already scans mission zips and classifies zip children as mission descriptor, mission HOG, DXA mod archive, documentation, or other.
- `SetupSections.kt` already shows mission zip title, type, author, editor, level count, level names, constituent files, sizes, compressed sizes, and generic child file info.
- Standalone `.mn2` and `.msn` file details already reuse `SectorgameMissionZip.parseMissionDescriptor()`.
- Native MIDI preview code already has HOG helpers in `android/app/src/main/cpp/shared/midi_preview.c`:
  - `hog_read_entry()`
  - `hog_list_entries()`
- Those helpers currently target HMP/MIDI preview and return only filtered entry names and sizes, not a structured general archive summary.

## Gaps
- HOG child popups currently show only generic fields: category, type, role, path, size, compressed size.
- Standalone HOG file details do not list entries or summarize contained file roles.
- DXA child popups in mission zips do not list inner entries, even though DXA archives are ZIP-format in the observed sample.
- PIG file details currently show generic labels and manifest/version hashes, but no bitmap/sound counts, dimensions, flags, or variant clues.
- PIG parsing is trickier than HOG parsing:
  - D2 PIGs start with `PPIG`, version `2`, then bitmap headers.
  - D1 PIGs use variant-sensitive layout and can include bitmap and sound headers after `Pigdata_start`.
  - D1 Mac/shareware/registered behavior is size-dependent in `d1/main/piggy.c` and `d2/main/piggy.c`.

## Proposed Metadata Model
- Add a small launcher-facing summary model, likely Kotlin-first:
  - `GameFileMetadataSummary`
  - `ArchiveEntrySummary`
  - `MetadataProblem`
- Common fields:
  - format name
  - validity status
  - game guess: D1, D2, both, unknown
  - role/scope: base game archive, mission archive, texture pack, sound pack, level file, editor/source file, metadata/descriptor
  - entry count and total unpacked bytes when available
  - category rollups by extension
  - notable entries with names and sizes
  - warnings for malformed magic, truncated entries, too many entries, invalid sizes, unsupported PIG variant
- Keep detailed mission descriptor parsing in the existing Kotlin parser.
- Keep deep engine-specific binary interpretation limited to summaries unless a C helper is clearly safer.

## Implementation Tranche 1: HOG And DXA Listing
- [ ] Add a streaming HOG lister usable from both local files and zip child streams.
- [ ] Recognize HOG magic `DHF`, repeated 13-byte null-padded name, 4-byte little-endian size, then data.
- [ ] Add guards mirroring native `hog_read_entry()`:
  - max entry size, probably 64 MB unless higher is needed.
  - reject impossible sizes relative to known container size.
  - cap displayed entries with "and N more".
- [ ] Classify HOG entries by existing `LauncherFileLabels.kt` extensions.
- [ ] For HOGs with `.rl2`/`.rdl`, show level archive scope and game guess.
- [ ] For HOGs with `.ied`, surface "Editor file: Inferno" as a hint, not as authoritative metadata.
- [ ] For `.txb`, show "briefing/text" and optionally leave decoding for a later tranche.
- [ ] Reuse the same summary in:
  - standalone `FileDetailDialog`.
  - `MissionZipConstituentDialog` for inner HOG children.
- [ ] Add DXA ZIP listing for mission zip children and standalone DXA details using existing `ZipFile` scan patterns from `ModManager`.

## Implementation Tranche 2: Mission Zip Deep Details
- [ ] Extend `SectorgameMissionZip.Constituent` or add a lookup path so the constituent dialog can read the selected child stream from the parent zip.
- [ ] Show nested summaries for:
  - HOG entries in `Uneasy4.hog`.
  - DXA entries in `Uneasy4.dxa`.
  - descriptor details for `Uneasy4.mn2`.
- [ ] Keep child extraction temporary and bounded; prefer streaming and central-directory reads where possible.
- [ ] For `Uneasy4.zip`, expected details should show:
  - mission title/author/editor from `.mn2`.
  - HOG contains 1 D2 level, 1 HAM, 1 POG, 1 IED editor file, 1 TXB.
  - DXA contains Ogg audio/music, S22 sound effects, and SNG song list.

## Implementation Tranche 3: PIG Summary
- [ ] Add focused PIG parser tests against known sample files:
  - D1 registered/shareware/demo PIGs.
  - D1 Mac PIGs.
  - D2 registered/demo PIGs.
  - D2 Mac group PIGs such as `groupa.pig`, `alien1.pig`, `fire.pig`, `ice.pig`, `water.pig`.
- [ ] Start with safe top-level summaries:
  - detected variant when known by magic/header/size.
  - bitmap count.
  - sound count when the layout includes sounds.
  - total bitmap data bytes.
  - examples of bitmap names and dimensions.
  - flags rollup: RLE, transparency, super transparency, no lighting.
- [ ] Use `d1/main/piggy.c` and `d2/main/piggy.c` as the source of truth for layouts and variant handling.
- [ ] Avoid decoding or displaying image previews in the first tranche.
- [ ] If the Kotlin parser becomes too close to engine internals, move PIG parsing into a tiny native metadata helper and document duplicated constants.

## UI Plan
- Add a "Contents" section to file detail dialogs only when the parser has meaningful data.
- Keep the top fields stable:
  - Category
  - Type
  - Format
  - Scope
  - Game
  - Status
  - Size/hash/import metadata
- Add compact rollups before examples:
  - `5 entries: level, HAM, POG, editor file, briefing text`
  - `3 DXA entries: audio/music, sound effects, song list`
  - `N bitmaps, M sounds`
- Use short expandable or capped lists for entries so dialogs stay readable on TV/mobile.

## Tests
- [ ] JVM unit test for HOG listing using an in-memory HOG matching `Uneasy4.hog` entries.
- [ ] JVM unit test for malformed HOG magic/truncated entry/bogus size.
- [ ] JVM unit test for mission zip child details using an in-memory `Uneasy4`-style zip.
- [ ] JVM unit test for DXA ZIP summary.
- [ ] JVM unit tests for representative PIG variants before enabling PIG UI details.
- [ ] Focused Gradle test run for new parser/detail tests.

## Open Questions
- Should `.txb` text be decoded in the launcher, or is identifying it as briefing/text enough for the first pass?
- Should `.ied` be labeled specifically as Inferno editor data based only on extension, or should the parser inspect a signature/header first?
- Do we want child detail popups to read nested files directly from the original mission zip every time, or cache lightweight summaries in the mod manifest at import time?
- For PIG, is a Kotlin summary parser acceptable if it mirrors engine constants, or should native metadata helpers own all PIG layout knowledge?
