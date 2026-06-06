# Sectorgame Zip Mission Import Plan

## Goal
- [x] Sketch support for importing sectorgame mission zip files without mandatory extraction, using `game_data/levels/Uneasy4.zip` as the prototype input.

## Steps
- [x] Inspect existing launcher import, mod listing, metadata popup, storage/link inspector, and game launch packaging paths.
- [x] Inspect the prototype zip structure and mission metadata enough to answer game detection feasibility.
- [x] Draft an implementation plan that fits existing Android launcher and game handoff patterns.

## Prototype Findings
- `game_data/levels/Uneasy4.zip` is 32,030,975 bytes, so it fits the proposed under-100 MB in-memory path.
- It contains:
  - `Uneasy4.dxa`, 30,794,814 bytes
  - `Uneasy4.hog`, 3,714,035 bytes
  - `Uneasy4.mn2`, 122 bytes
- `Uneasy4.mn2` contains:
  - `name = Uneasy 4`
  - `type = normal`
  - `num_levels = 1`
  - `Uneasy4.rl2`
  - `author = Blarget 2 and Nightsurfer`
  - `editor = Inferno 1.0.22`
- Game detection is feasible for this file. `.mn2` is a D2 mission descriptor, and its `.rl2` level entry is also D2-specific. D1 mission descriptors use `.msn`, and D1 levels are normally `.rdl`.

## Existing Integration Points
- `SetupActivity.processPickedUris()` currently routes `.zip` files into the generic temp extraction flow. Sectorgame mission zips should be detected before that generic branch.
- `ModManager` owns the mod list, manifest, delete, details, compatibility preflight, and `.active_mod_paths` generation.
- The native launcher handoff writes:
  - `<filesDir>/d1x-redux/.active_mod_paths`
  - `<filesDir>/d2x-redux/.active_mod_paths`
- `d1/misc/physfsx.c` and `d2/misc/physfsx.c` mount each enabled path from `.active_mod_paths`.
- The current game mission loader expects the mission descriptor and sibling HOG to be visible as mountable real files. For a mission `Uneasy4.mn2`, it opens `Uneasy4.mn2`, then swaps the extension to `Uneasy4.hog` and calls `PHYSFSX_contfile_init()` on that HOG.

## Proposed Data Model
- Extend `ModManager.ModInfo` with a type/kind field, defaulting existing entries to `dxa`.
  - `dxa`: current single archive behavior
  - `mission_zip`: sectorgame-style container
  - optional future values: `extracted_mission_zip`, `mission_bundle`
- Add `mods/mission_zip_manifest.json` or embed extra fields in `mod_manifest.json`:
  - top-level filename, display name, size, sha256, enabled, order, game
  - category, initially `levels`
  - import mode: `stored_zip`, `extracted_bundle`
  - constituent list: name, extension, size, compressed size, sha256 if available, role
  - primary mission descriptor path
  - detected mission title, author, editor, level count
  - extracted bundle root if over threshold and expanded
- Keep compatibility with existing mod manifest loading by treating missing kind as `dxa`.

## Import Flow
- Add a lightweight scanner, likely `SectorgameMissionZip.kt`.
- During file picker processing:
  - For `.zip`, inspect the central directory or stream entries enough to decide whether it is a mission container.
  - Match if it has exactly or at least one `.mn2` or `.msn`, plus associated `.hog` and/or `.dxa`, and no required base-game asset signature that would make it a normal installer package.
  - If matched, route to `ModManager.importMissionZip()` instead of `extractZipContents()`.
- For the prototype, import the outer zip into `filesDir/mods/Uneasy4.zip`, enabled by default.
- Category rule: any mission zip with `.mn2` or `.msn` is categorized as `levels` in the mod list/details.

## Game Detection
- Prefer mission descriptor extension:
  - `.mn2` means D2
  - `.msn` means D1 unless contents prove otherwise
- Confirm by parsing descriptor level names:
  - any `.rl2` or `.sl2` means D2
  - any `.rdl` or `.sdl` means D1
- If multiple descriptors disagree, mark `both` or `unknown` and show a problem in details.
- For `Uneasy4.zip`, detected game should be `d2`.

## Base Asset Readiness
- Reuse `launchDataReadyForGame(game, setDir, manifest, safManifest)` for a simple top-level readiness flag.
- In the mod row/details:
  - show target game from descriptor detection
  - show base assets ready/missing for that game
  - if the zip is enabled but base game assets are missing, include a problem line similar to existing mod compatibility problems

## Details UI
- Extend `ModDetails` with constituent files for mission zips.
- The top-level details dialog should show:
  - archive path
  - import mode
  - game
  - category `levels`
  - base assets ready/missing
  - mission metadata from `.mn2` or `.msn`
  - constituent files with roles: mission descriptor, mission HOG, DXA add-on, docs/other
- Each constituent row should have an info action.
- For sub-popups:
  - for `.dxa`, reuse the current `getModDetails` archive scanner against that inner file, either from an extracted temp copy or an in-memory parser helper
  - for `.mn2`/`.msn`, show the same info as importing that mission file directly would show, with parsed mission title, level count, level filenames, author/editor if present
  - for `.hog`, show the same file detail fields used by `FileDetailDialog`, plus HOG contents if an existing or small new HOG lister is available

## Launch Handoff
- Do not rely on mounting the outer zip directly. PhysFS would see `Uneasy4.mn2` and `Uneasy4.hog` as files inside the zip, but the mission loader needs the sibling HOG to be mountable by path.
- For mission zips under 100 MB:
  - Add native Android startup support that reads enabled `mission_zip` paths from `.active_mod_paths` or a companion `.active_mission_archives.json`.
  - Decompress constituents into owned native memory at game startup.
  - Present those constituents through PhysFS using `PHYSFS_Io` or a small custom archiver, modeled after the existing SAF archiver.
  - Mount inner `.dxa` and `.hog` constituents with correct priority before mission enumeration.
  - Make the `.mn2`/`.msn` appear at the root or `missions/` path expected by the existing mission loader.
- For mission zips over 100 MB:
  - Offer extraction into the active import root, probably `mods/extracted/<safe-id>/`.
  - Register the top-level mod entry as `extracted_bundle`.
  - Write child files under that bundle root and mount the extracted directory or child archives.
  - Delete of the top-level entry deletes the entire bundle root.

## Storage And Link Inspector
- Add bundle ownership metadata so the advanced storage inspector can group files:
  - top-level mod id
  - child relative paths
  - child roles
  - import mode
- For extracted bundles, annotate child rows as owned by the top-level mission zip.
- For deletion:
  - `deleteMod()` removes the outer zip for `stored_zip`
  - `deleteMod()` removes the bundle root and manifest metadata for `extracted_bundle`
  - child entries are not independently deleted from the mod details UI unless a later feature deliberately supports repair/edit

## Tests
- JVM unit test for `SectorgameMissionZip` detection using an in-memory zip with `.mn2`, `.hog`, `.dxa`.
- JVM unit test using `Uneasy4.mn2` contents to assert `d2`, title `Uneasy 4`, category `levels`, and level `Uneasy4.rl2`.
- ModManager manifest round-trip test for `mission_zip` and `extracted_bundle`.
- Launch manifest writer test that enabled mission zips produce the native handoff file.
- Native or integration smoke test that a small mission zip can be imported, enabled, and appears in D2 mission selection.

## Implementation Tranche 1
- [ ] Add Kotlin scanner/parser for sectorgame-style mission zips.
- [ ] Extend `ModManager` metadata and import/details support for `mission_zip` entries.
- [ ] Route matching `.zip` imports into the mods list before generic archive extraction.
- [ ] Add focused JVM tests for scanner and manifest/details behavior.
- [ ] Run focused Gradle tests and update this plan with results.
