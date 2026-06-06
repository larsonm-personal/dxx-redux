# Enemy Within 15th ZIP Support Plan

## Goal
- [x] Inspect `game_data\ewithin-versions.zip`, identify the D2X-Rebirth package, and plan launcher support without implementation code changes in this tranche.

## Survey Steps
- [x] Read project instructions and existing mission ZIP import/staging code.
- [x] Inventory the parent ZIP and Rebirth child ZIP.
- [x] Inventory the Rebirth DXA and HOG contents.
- [x] Map the advertised features to formats already supported by DXX-Redux/Rebirth paths.
- [x] Identify current launcher gaps for parent ZIP selection and nested `missions/` paths.

## Archive Findings
- Parent `ewithin-versions.zip` contains only `ewithin-rebirth.zip` and `ewithin-xl.zip`.
- `ewithin-rebirth.zip` contains:
  - `ewithin.dxa`, 403,853,970 bytes
  - `ewithin.txt`, 8,429 bytes
  - `missions/ewithin.hog`, 28,737,491 bytes
  - `missions/ewithin.mn2`, 961 bytes
- `ewithin.mn2` declares D2 mission `Descent: The Enemy Within`, 26 normal levels, and 6 secret levels.
- `ewithin.hog` has 230 entries:
  - 32 `.rl2` levels, 32 `.hxm` robot patches, 32 `.pog` texture overrides
  - 27 `.txb` text/endlevel data files, 66 `.pcx` images, 27 `.bbm`, 5 `.clr`, 4 `.lgt`
  - `exit.ham`, `ewithin.ctb`, `ewithin.txb`, `oldbrief.txb`, and `endlevel.hmp`
- `ewithin.dxa` has 35 entries:
  - 30 `.ogg` tracks, including briefing, endlevel, delta, pad, and level01 through level26
  - `descent.sng` pointing to those OGG files
  - `descent2.ham`, `descent2.s11`, `descent2.s22`
  - `UUD2SP.rtf`
- No executable, shared library, VST, VST3, or script payload was found in the Rebirth ZIP or DXA. The VST mention appears to describe how the soundtrack was produced, not runtime code.

## Support Assessment
- Mission selection should be supportable after staging path fixes. The `.mn2` and `.hog` are valid D2 mission files, but they are already under `missions/`.
- Levels, secrets, briefings, level-specific HXM robot patches, POG texture overrides, cockpit data, and briefing graphics should be supportable through existing D2 mission/HOG behavior once the HOG is staged at the expected path.
- Exit sequences should be supportable by existing Rebirth-derived endlevel code if the HOG is visible. The archive includes `exit.ham`, exit model bitmaps, terrain assets, and per-level `.txb` files used by `load_endlevel_data()`.
- New soundtrack should be supportable through existing built-in/addon music. `descent.sng` references OGG files and D2 already supports OGG via SDL_mixer when `MusicType` is built-in.
- New robot sound effects, boss pilot voices, and restored D1 door sounds should be supportable if the DXA can override `descent2.ham`, `descent2.s11`, and `descent2.s22` before the base game files are read.
- Gameplay tweaks in `.rl2`, `.hxm`, `.pog`, and `descent2.ham` should be supportable as data. No separate runtime code is implied.
- DOS/vanilla segment limit concerns are not a blocker for DXX-Redux: `d2/main/segment.h` sets `MAX_SEGMENTS` to 9000, while the original limit is 900.

## Current Gaps
- [x] Parent package import: current `MissionZip.inspect(InputStream)` sees only nested child ZIPs, so `ewithin-versions.zip` will not be recognized.
- [x] Large input handling: current URI import scans the stream before copying. Parent and Rebirth archives should be copied/spooled to disk and inspected with `ZipFile`.
- [x] Rebirth child selection: parent ZIP support needs to prefer a child whose name contains `rebirth`, and reject or warn if only XL is available.
- [x] Staging path normalization: current staging places every entry under generated `missions/`, so `missions/ewithin.mn2` becomes `missions/missions/ewithin.mn2`.
- [x] Nested DXA path calculation: active path generation must still add the staged `ewithin.dxa` after normalization.
- [x] Details UI should surface the large DXA contents: OGG soundtrack, replacement HAM/S11/S22, and documentation.
- [x] Tests need concrete coverage for parent ZIP selection, direct Rebirth ZIP import, nested `missions/` staging, and large archive stream safety.

## Proposed Implementation Phases
- [x] Phase 1: Refactor mission ZIP import to copy SAF input to a temporary/import file first, then inspect with `ZipFile`. Keep progress reporting and storage guard behavior.
- [x] Phase 2: Add parent ZIP detection. If a ZIP has no mission descriptor but contains child ZIP candidates, select the Rebirth child by filename, stream that child to the final mod file, and register the extracted child as the mission ZIP. Treat multiple Rebirth candidates as an import error until a UI choice exists.
- [x] Phase 3: Normalize mission ZIP staging. If entries are rooted under `missions/`, stage them relative to `stageDir` instead of `stageDir/missions`; otherwise preserve the current top-level-to-`missions/` behavior for existing packs.
- [x] Phase 4: Ensure generated active paths include staged nested DXAs after path normalization and keep the DXA mounted early enough to override `descent2.ham`, `descent2.s11`, `descent2.s22`, and provide `descent.sng`/OGG files.
- [x] Phase 5: Extend mission ZIP details to inspect nested DXA summaries as well as HOG summaries, so Enemy Within shows soundtrack, replacement sound tables, replacement HAM, and docs.
- [x] Phase 6: Add focused JVM tests:
  - parent ZIP with `ewithin-rebirth.zip` and `ewithin-xl.zip` chooses Rebirth
  - direct Rebirth-style ZIP imports as D2 mission zip
  - staged files appear at `d2x-redux/.generated_mission_zips/<name>/missions/ewithin.mn2`, not `missions/missions/ewithin.mn2`
  - active mod paths include the generated root and staged `ewithin.dxa`
  - large child ZIP handling uses file streams and does not call `readBytes()` on child archives
- [ ] Phase 7: Run scoped code quality and targeted tests, then a D2 launch smoke test selecting `Descent: The Enemy Within` and verifying level 1 loads with built-in/addon music selected.

## Verification Ideas
- [x] Use JVM tests for import/staging path behavior.
- [ ] Use setup introspection or mission list automation to confirm the mission appears after import.
- [ ] Use game introspection after launch to confirm `current_level_name` is loaded from level 1.
- [ ] Check debug logs for PHYSFS mount lines showing generated mission root and `ewithin.dxa`.
- [ ] Check track overlay or audio status for `level01.ogg` when built-in/addon music is active.

## Implementation Notes
- `MissionZip.isImportCandidate()` now recognizes direct mission ZIPs and parent ZIPs with a Rebirth child without reading nested archives into memory.
- `ModManager.importMissionZip()` now spools SAF input to disk before inspection and registration.
- Parent packages with one child ZIP whose leaf name contains `rebirth` are imported by streaming that child ZIP into `mods/`.
- Mission ZIP launch staging preserves existing `missions/` roots when present and keeps legacy top-level mission packs staged under `missions/`.
- Active mod path generation now points to the staged DXA path after layout normalization.
- Mission ZIP details now include nested DXA feature summaries.

## Verification
- [x] `.\android\run-code-quality.ps1 -Fix -Paths <changed files>` passed.
- [x] `.\android\gradlew.bat -p android :app:testDebugUnitTest --tests com.dxxredux.app.ModManagerMissionZipTest --tests com.dxxredux.app.MissionZipTest` passed.
- [x] `.\android\gradlew.bat -p android :app:assembleDebug` passed.
