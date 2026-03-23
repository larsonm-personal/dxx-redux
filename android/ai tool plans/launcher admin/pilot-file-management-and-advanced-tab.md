# Plan: Pilot File Management, Advanced Tab, and active_set_path Fix

## Summary

1. Segregate D1/D2 pilot files so they never appear in each other's in-game pilot selection
2. Fix the `.active_set_path` bug (Kotlin writes to wrong location; C code never finds it)
3. Add an "Advanced Settings" full-screen page to the launcher
4. Add "Delete All Player Files" feature
5. Add "Delete/Clear file set" feature in the set chooser (including default set)

---

## Phase 1: Pilot file segregation + active_set_path fix

### 1a. Force SysUsePlayersDir on Android

**Root cause**: `ReadCmdArgs()` is skipped on Android (early return in physfsx.c).
`SysUsePlayersDir` stays 0. Pilots are stored at pref dir root (e.g. `d2x-redux/pilot.plr`).
`PHYSFS_getBaseDir()` returns `filesDir/` (parent of both game dirs) and is on the search path.
Any `.plr` at `filesDir/` root or in the other game's dir can leak across.

**Fix**: Add `GameArg.SysUsePlayersDir = 1;` in the `#ifdef ANDROID` block of:
- `d2/misc/physfsx.c` (before the `return;` at ~line 113)
- `d1/misc/physfsx.c` (before the `return;` at ~line 106)

Pilot enumeration then searches `Players/` relative to write dir. D2 finds `d2x-redux/Players/`,
D1 finds `d1x-redux/Players/`. No cross-game leakage.

### 1b. Fix .active_set_path bug

**Bug**: `FileSetManager.writeActiveSetPath()` writes to `filesDir/.active_set_path`.
The C code in physfsx.c reads `snprintf(asp, "%s.active_set_path", pref)` which resolves to
`filesDir/d2x-redux/.active_set_path` (or d1x-redux). The C code never finds the file.

**Fix**: Change `writeActiveSetPath()` to write to BOTH game pref dirs:
- `filesDir/d2x-redux/.active_set_path`
- `filesDir/d1x-redux/.active_set_path`

Both contain the same absolute path to the active set directory.

### 1c. Pilot file migration

Add migration in `SetupActivity.kt` startup (alongside existing `migrateDefaultSetIfNeeded()`):
- For each game dir (`d1x-redux/`, `d2x-redux/`) in each set (`sets/*/`):
  - Create `Players/` subdir if needed
  - Move `.plr`, `.plx`, `.eff`, `.ngp`, `.sg?`, `.mg?` from game dir root into `Players/`
- Legacy sweep: also move any stray pilot files at `filesDir/` root into d2x-redux/Players/
  (we can't reliably detect format, so default to d2 since d2 is more common)

### Files to modify
- `d2/misc/physfsx.c` -- 1 line: `GameArg.SysUsePlayersDir = 1;`
- `d1/misc/physfsx.c` -- 1 line: same
- `android/app/src/main/java/com/dxxredux/app/FileSetManager.kt` -- fix writeActiveSetPath()
- `android/app/src/main/java/com/dxxredux/app/SetupActivity.kt` -- add migration call

---

## Phase 2: Advanced settings page

### 2a. Create AdvancedSettingsPage.kt

New full-screen composable following `ControllerConfigPage` pattern:
- `BackHandler` for Android back
- Top bar: "Advanced Settings" + back button
- Scrollable column with:
  - Export All Configs / Import Config buttons
  - Render Resolution picker
  - Restart App button (renamed from "Restart game")
  - Reset All Controls button + confirmation dialog
  - Delete All Player Files button + confirmation dialog (Phase 2c)

### 2b. Update SetupActivity.kt

- Add `showAdvancedPage` state var + if-guard (same pattern as showControllerPage)
- Remove from ControllerSection: Export/Import buttons, Reset All Controls
- Remove from controlsPane: ResolutionPicker, Restart button
- Add "Advanced Settings" button in controlsPane where the removed items were

### 2c. Delete All Player Files

Button on the advanced page:
- Red text, confirmation dialog
- Warning: "Deletes all pilot files (.plr), configs (.plx), effects (.eff),
  new game plus (.ngp), saved games (.sg*, .mg*) for both Descent 1 and 2
  across all file sets. Cannot be undone"
- Implementation: walk sets/*/d1x-redux/ and d2x-redux/ (and Players/ subdirs),
  delete matching extensions. Restart after.

### Files
- **Create**: `android/app/src/main/java/com/dxxredux/app/AdvancedSettingsPage.kt`
- **Modify**: `android/app/src/main/java/com/dxxredux/app/SetupActivity.kt`

---

## Phase 3: File set deletion enhancements

### 3a. Add clearSet() to FileSetManager

Like deleteSet() but works for default:
- Deletes all files in set directory
- Recreates empty directory
- Does NOT remove set from file_sets.json

### 3b. Delete button in SetManagementDialog

- Add delete/clear button for current set (including default)
- Confirmation dialog differentiates:
  - "Imported files (copied to app data) will be permanently deleted"
  - "Files added via file picker (leave-in-place) will be unlinked but not
    deleted from their original location"
- For default: calls clearSet(). For non-default: calls deleteSet().

### Files
- `android/app/src/main/java/com/dxxredux/app/FileSetManager.kt` -- add clearSet()
- `android/app/src/main/java/com/dxxredux/app/SetupActivity.kt` -- update SetManagementDialog

---

## Key files reference

| File | Role |
|------|------|
| `d2/misc/physfsx.c` | Android PHYSFS init, SysUsePlayersDir, active_set_path read |
| `d1/misc/physfsx.c` | Same for D1 |
| `d2/main/menu.c:361` | Pilot enumeration via PHYSFSX_findFiles (reference, no changes) |
| `d1/main/menu.c:359` | Same (reference) |
| `android/app/src/main/cpp/android_gamepad_config.cpp:108` | patch_all_plr_files() -- verify compat |
| `SetupActivity.kt` | Launcher UI, migration, navigation |
| `FileSetManager.kt` | Set lifecycle, writeActiveSetPath, clearSet |
| `ConfigImportExport.kt` | Export/import (used by advanced page) |
| `NativePilotPatcher.kt` | JNI pilot patching |
| `ControllerConfigPage.kt` | Reference for full-screen page pattern |
| `AdvancedSettingsPage.kt` | New file |

---

## Verification

1. Launch D2, create pilot. Launch D1, verify not visible. Vice versa.
2. Pre-populate .plr at pref dir root, launch, verify migration to Players/.
3. Create non-default set, verify .active_set_path written to both game pref dirs.
4. Advanced page opens, all controls work, Android back returns to main.
5. Delete all player files: verify files gone from both game dirs in all sets.
6. Delete file set (non-default): files gone, set removed. Default: files gone, set remains.
7. Android build passes. Windows/Linux cmake unaffected (#ifdef ANDROID).
8. Regression: run_test.ps1 -ScriptName test_launch_to_automap.json5 -Game d2
