# Obsidian Zip Feature Support Plan

## Goal
- [x] Inspect `game_data\Obsidian.zip` and plan support for its mission features without changing implementation code.

## Survey Steps
- [x] Inventory the zip and nested mission archives.
- [x] Identify soundtrack files and activation path for first-open/default music mode behavior.
- [x] Identify custom robots, textures, player ship, HUD, briefings, and objective-related content.
- [x] Trace D2 and Android launcher/mod metadata paths that would activate or expose those features.
- [x] Propose implementation phases, verification, and metadata/UI improvements.

## Findings
- Outer zip files: `obsidian.mn2`, `obsidian.hog`, `obsidian.sng`, `obsidian.txt`, `obsidian.bat`.
- Mission descriptor: D2 normal mission, 15 levels, 3 secrets, author `Ellusion Design`.
- HOG contents: 122 entries, including 18 `.rl2`, 15 `.hxm`, 18 `.pog`, 10 `.hmp`, 10 `.hmq`, 10 `.mid`, 38 `.pcx`, `obsidian.txb`, `obsidian.ctb`, and `demo.zip`.
- Each `.hxm` declares 36 robot records. The `.pog` files declare 126 to 147 bitmap replacements each.
- `obsidian.txb` contains standard D2 briefing commands and objective pages using `objec*.pcx`.
- `demo.zip` contains `DEMO.TXT`, but uses an old/passworded ZIP method that .NET and normal unzip did not extract.

## Support Assessment
- Mission zip import should accept and stage the pack, since it has a top-level `.mn2` and `.hog`.
- HOG mounting should activate the custom levels, HMP/HMQ/MID assets, PCX briefings, TXB briefing text, CTB cockpit data, HXM robot/model replacements, and POG texture/cockpit/ship texture replacements through existing D2 paths.
- Non-standard objectives appear to be briefing/level-design objectives implemented with keys, triggers, reactors, bosses, and exits, not a separate objective metadata format.
- The soundtrack needs work. `songs_init()` reads only `dxx-r.sng` or `descent.sng`; Obsidian ships `obsidian.sng` and a batch file that installs it by copying to `descent.sng`.
- Current launcher override for mission zip built-in music only looks for song lists inside nested `.dxa` files and only recognizes `.mid`, `.ogg`, `.mp3`, and `.flac`, so it will not detect Obsidian's top-level HMP song list.

## Proposed Phases
- [x] Phase 1: Enhance mission zip staging so a single top-level mission-specific `.sng` can be exposed as `descent.sng` or `dxx-r.sng` for that staged mission only.
- [x] Phase 2: Broaden mission zip built-in music detection to inspect top-level `.sng` files, nested HOG song/music entries, and `.hmp` references.
- [x] Phase 3: Persist music mode in Android save metadata and expose it through resume-save JSON; native restore should apply saved `GameCfg.MusicType`, and launcher resume should write matching config before launch.
- [x] Phase 4: Enhance mission zip metadata details by summarizing nested HOG feature counts on the mission detail page: levels, secrets, HXM robot patches, POG texture overrides, HMP/MID music, briefing text/images, cockpit data, and unreadable embedded ZIP docs.
- [x] Phase 5: Add focused tests for Obsidian-style mission zip import/staging, HMP `.sng` detection, save metadata music mode round trip, and metadata summaries.

## Implementation Notes
- `ModManager` now treats `.hmp` as mission zip built-in music, scans top-level `.sng` and `.hog` files, and keeps the existing nested `.dxa` scan.
- Mission zip staging now recreates the generated directory each launch and copies one top-level mission-specific song list to `descent.sng` when the pack did not already provide `descent.sng` or `dxx-r.sng`.
- Android save metadata version is now 4 and stores `music_type`. D1 and D2 Android restore paths apply the saved type after reading the trailer.
- Resume/save explorer JSON and Kotlin models now carry `music_type`; resume launches pass that value into launch-time config writing.
- Mission zip details now add nested HOG feature notes, and file type labels recognize `.ctb` cockpit data and `.hmq` HMI MIDI music.

## Verification
- `.\android\run-code-quality.ps1 -Fix -Paths ...` passed for the changed Kotlin, C/C++, and tests.
- `.\android\gradlew.bat -p android :app:testDebugUnitTest --tests com.dxxredux.app.ModManagerMissionZipTest --tests com.dxxredux.app.MissionZipTest --tests com.dxxredux.app.GameFileMetadataTest --tests com.dxxredux.app.ResumeSavePanelTest --tests com.dxxredux.app.SaveExplorerTest` passed.
- `.\android\gradlew.bat -p android :app:assembleDebug` passed.
