[x] Inspect current save filename, metadata, autosave, and launcher resume-save paths
[x] Decide a save-set key scheme for game, pilot, mission, and coop/manual scope
[x] Plan engine-side save/load changes with minimal D1/D2 edits and Android-only helpers
[x] Plan launcher save explorer UI and native bridge shape
[x] Plan migration, testing, and rollout phases

# Masked Save Sets And Explorer Plan

## Current Shape

- Single-player manual save slots are flat per game and pilot:
  - D1: `d1x-redux/Players/<callsign>.sg0` through `.sg9`
  - D2: `d2x-redux/Players/<callsign>.sg0` through `.sg9`
- Coop/multiplayer saves use `.mg0` through `.mg9`. Android coop autosaves use the sentinel callsign `coopsave` for slots 5 through 9, plus coop metadata/history files.
- Android metadata is appended by `state_android_write_save_metadata()` and already records game id, save kind, callsign, description, mission filename, level, time, difficulty range, and thumbnail.
- The launcher resume panel scans only root and `Players` for `.sg0` through `.sg9`, reads Android metadata, and exposes `ResumeSaveCandidate`.
- Coop launcher UI separately reads `coop_autosave_history.json` and `coop_progress.json`, keyed by mission and client identity.

## Proposed Model

Use a save set key:

```text
game: d1 | d2
scope: single | coop
pilot: callsign for single, coopsave or host callsign for coop display
mission: Current_mission_filename / Netgame.mission_name
```

The game directory already supplies `game`, so the on-disk scoped path can stay under each game root:

```text
Players/save_sets/single/<pilot>/<mission>/<pilot>.sg0
Players/save_sets/single/<pilot>/<mission>/<pilot>.sg1
...
Players/save_sets/coop/<mission>/coopsave.mg5
Players/save_sets/coop/<mission>/coopsave.mg9
```

Keep the classic filename suffixes (`.sg0` to `.sg9`, `.mg0` to `.mg9`) so existing slot parsing, metadata slot extraction, and file-type detection remain simple. Use lowercase sanitized directory segments for `pilot` and `mission`; preserve the real callsign and mission filename in metadata.

## Engine-Side Plan

1. Add a shared Android save-set path helper in `state_android_shared.c`.
   - `state_android_save_set_scope()`: `single` unless `Game_mode & GM_MULTI_COOP`.
   - `state_android_current_save_set_key()`: game, scope, callsign, mission filename.
   - `state_android_save_filename_for_slot_scoped(...)`: builds the path above and creates directories as needed before writes.
   - `state_android_last_save_set_read/write(...)`: small pointer file, probably `Players/save_sets/last_single.json` and `Players/save_sets/last_coop.json`.

2. Keep D1/D2 changes tiny.
   - Replace the Android-only filename construction inside `state_get_savegame_filename()` with a call into the shared helper.
   - Replace Android autosave filename construction in `state_android_save_to_slot()` with the same helper.
   - For coop autosaves in `coop_save.c`, use a helper that scopes by mission and keeps `coopsave.mgN`.
   - Keep non-Android save paths unchanged.

3. Main menu load behavior.
   - When `Current_mission_filename` is available, show that exact set.
   - When no mission is active, use the last edited single-player save set for that game and pilot if available.
   - If no last pointer exists, fall back to the legacy flat slots, then no saves.
   - When saving or loading a scoped slot, update the last pointer for that game/scope.

4. Coop behavior.
   - Scope coop autosave files by mission so different level sets never overwrite each other.
   - Move/replicate coop history/progress sidecars into the same mission scope:
     - `Players/save_sets/coop/<mission>/coop_autosave_history.json`
     - `Players/save_sets/coop/<mission>/coop_progress.json`
     - `Players/save_sets/coop/<mission>/coop_progress_inventory.bin`
   - Launcher readers can keep accepting the current root-side files during migration, but new writes should go scoped.

5. Metadata extension.
   - Bump `ANDROID_SAVE_META_VERSION`.
   - Add compact fields:
     - `scope`: single or coop
     - `set_key`: sanitized stable key or compact string
     - maybe `mission_display_name` only if there is room, otherwise leave display names to launcher mission metadata.
   - Keep mission filename as the canonical restore key because the save file format itself already stores only a short mission identifier.

## Launcher Save Explorer Plan

Add `SaveExplorerBridge` next to `ResumeSaveBridge`, backed by native metadata scanning rather than Kotlin save parsing.

Native APIs:

```text
nativeListSaveFilters(filesDir) -> JSON
nativeListSaveSet(filesDir, game, pilot, mission, scope) -> JSON array of 10 slots
nativeListAllSaveSlots(filesDir) -> JSON array of every discovered slot plus orphan notes
nativeListRecentSaves(filesDir, limit=10) -> JSON array
nativeDeleteSaveSlot(filesDir, relativePath, expectedGame, expectedSlot, expectedTimestamp) -> JSON result
nativeReadThumbnailRgb6(path) -> reuse existing helper or share implementation
```

Data shape:

```json
{
  "game": "d2",
  "scope": "single",
  "pilot": "neuma",
  "mission_name": "d2",
  "slot": 3,
  "path": "...",
  "relative_path": "...",
  "description": "level 5",
  "save_kind": "manual",
  "save_time_unix_seconds": 1780000000,
  "level_num": 5,
  "level_name": "Quartzon",
  "has_thumbnail": true
}
```

UI:

- Add a launcher action, probably near the existing resume panel / advanced save tools: `Save Explorer`.
- Dialog layout:
  - top row selectors: game, pilot, level set, scope
  - a mode selector with `Save Set`, `Ten Most Recent`, and `All Slots`
  - 10 dense rows, each with small thumbnail, slot number, description, mission/level, time, and kind
  - `All Slots` shows every discovered save slot grouped by game, scope, pilot, and mission; it should include populated slots, legacy flat slots, scoped slots, and orphaned/corrupt files that look like saves but cannot be assigned to a normal set
  - load action on each populated row using existing `onLaunchGame(game, candidate)`
  - delete action on each populated or orphan row, with confirmation showing game, pilot, mission, slot, timestamp, and relative path
- Reuse `ResumeSaveThumbnailFrame()` but add a smaller row-oriented variant instead of the current large-card chooser.
- Friendly level-set labels can be resolved in Kotlin from existing mission descriptor/manifest metadata, but the native bridge should only require the canonical mission filename.

Deletion rules:

- Delete from the launcher only through the native bridge or a shared Kotlin helper that restricts paths to the app's D1/D2 game roots.
- Require the UI to pass the relative path plus expected game, slot, and timestamp so the delete operation can refuse if the file changed after the dialog was opened.
- For normal single-slot saves, delete only that save file.
- For coop scoped sets, deleting a slot deletes only the `.mgN` file by default; deleting a whole orphaned coop set may also remove its scoped history/progress sidecars after a second confirmation.
- After deletion, remove empty scoped directories and stale `last_*` pointers that reference the deleted set.
- Never delete imported mission data, pilots, configs, or unrelated files from this explorer.

Orphan handling:

- Mark a save as orphaned when it is under a save-set directory but has no valid Android metadata, has a game/scope/slot mismatch, references a missing pilot name, references a mission that is no longer present in launcher mission metadata, or is in a legacy flat location that cannot be confidently assigned.
- Orphaned rows should still show filename, path, size, modified time, and any partial metadata or classic header fields the native scanner can safely read.
- Orphaned saves should be unloadable by default unless the native scanner can produce a valid `ResumeSaveCandidate`; the primary action should be delete, with a secondary details view.
- Provide an `orphan_only` filter within `All Slots` so cleanup is not a scavenger hunt.

## Migration

- No strict backward compatibility is required for Android before release, but a gentle migration is cheap and useful.
- On first scoped-save scan:
  - Treat existing flat saves as an implicit legacy set.
  - If metadata has a mission filename, offer/copy them into the scoped location.
  - If metadata is absent, keep them visible under a `Legacy` set and leave them loadable.
- Do not delete legacy files automatically until save explorer has a visible delete/migrate affordance.

## Test Plan

1. Unit tests for path/key helpers.
   - Same pilot with two missions resolves to different slot files.
   - D1 and D2 remain isolated.
   - Coop and single-player resolve to different sets.

2. Native metadata scanner tests.
   - Lists exactly 10 rows for a selected set, including empty slots.
   - `Ten Most Recent` sorts across games, pilots, missions, and scopes.
   - `All Slots` returns every scoped, legacy, and orphan candidate with stable grouping.
   - Legacy flat files remain discoverable.
   - Delete refuses paths outside game roots and refuses files whose expected timestamp/slot no longer match.

3. Launcher JVM tests.
   - Filter extraction builds game/pilot/mission/scope selectors.
   - Row formatting fits small/dense save explorer rows.
   - All-slots grouping and orphan-only filtering produce predictable sections.
   - Delete confirmation text includes enough identity to avoid deleting the wrong save.

4. Android integration scripts.
   - Save D2 built-in slot 0, switch to a custom mission, save slot 0, verify both survive.
   - Coop autosave on mission A, host coop mission B, verify mission A autosaves remain available.
   - Main menu load uses the last edited set when no active mission is loaded.
   - Create an orphaned scoped save file, verify it appears in `All Slots`, delete it, and verify normal saves remain untouched.

## Suggested Implementation Order

1. [x] Native save-set path helper and tests.
2. [x] Single-player manual save/load scoping in D1 and D2.
3. [x] Android autosave scoping.
4. [x] Coop autosave/progress scoping.
5. [x] Native save explorer bridge and scanner tests.
6. [x] Save explorer all-slots and delete/orphan cleanup support.
7. [x] Launcher save explorer dialog.
8. Migration/legacy display polish.

## Implementation Progress

2026-06-06 tranche:

- Added `android_save_set.h/c` with scoped path builders for single slots, coop slots, D2 secret companion files, and sidecars.
- Wired Android D1/D2 save/load menus to use scoped slot paths.
- Wired Android single-player autosaves and best-progress autosaves to write/read the active scoped set.
- Added a `last_single.txt` pointer under `Players/save_sets` so the regular load menu can show the most recently edited single-player set when it is not mission-aware.
- Scoped D2 `Nsecret.sgc` companion files with the corresponding Android save slot.
- Scoped coop autosave `.mgN` files and coop autosave history/info/progress sidecars, while still writing the legacy root sidecar copies for the current launcher UI.
- Added `test_android_save_set` to both host native CTest suites.

Validation completed:

- `.\android\tests\test_native_host_unit_tests.ps1`
- `.\android\run-code-quality.ps1 -Fix`
- `.\gradlew.bat :app:externalNativeBuildDebug`

Next implementation target:

- Migration/legacy display polish and on-device verification of the launcher save explorer.

2026-06-06 save explorer tranche:

- Expanded the native resume/save scanner to recurse through game roots and discover scoped save-set files as well as legacy flat saves.
- Taught the scanner to recognize both `.sgN` and `.mgN` save slots while keeping the existing resume chooser filtered to loadable candidates.
- Added `SaveExplorerBridge` with native inventory and guarded delete calls.
- Added launcher `Save Explorer` dialog with `Save Set`, `Ten Recent`, and `All Slots` views.
- Added game, scope, pilot, and level-set selectors for masked save sets.
- Added an `Orphans only` filter in `All Slots`.
- Added per-row delete confirmation for loadable and orphaned save files.
- Added a main-page `Save Explorer` button next to the active file-set controls.

Validation completed:

- `.\android\run-code-quality.ps1 -Fix`
- `.\gradlew.bat :app:assembleDebug`
- `.\android\tests\test_native_host_unit_tests.ps1`
