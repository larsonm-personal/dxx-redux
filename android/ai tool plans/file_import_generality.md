# File Import Generality

## Status: Complete (build + lint verified)

## Goals

1. Make the main launcher "select game files or archive to import" button handle JSON config files
2. Add demo files to the file readiness view tree under a "Demos" line in the mods section area
3. Check for other unhandled file types

## Issue 1: Config JSON import via main button

### Current state
- Main import button classifier in SetupActivity.kt (line ~2590) routes by extension
- `.json` files fall through to `unhandledFiles` -> "file type not recognized" toast
- Config import is only available through AdvancedSettingsPage.kt "Import Config" button
- `ConfigImportExport.importFromUri()` handles the actual import
- `HumanReadableConfig.detectConfigType()` detects type by `"type"` field or structural keys

### Plan
- In the file classifier `when` block, add a `.json` case
- For `.json` files: read the content, try to parse as JSON, call `detectConfigType()`
- If type is recognized (touch_layout, controller_config, combined_config), add to a new `configJsonUris` list
- Single-file case: if only one file was picked and it's a recognized config, show an "Import game config?" confirmation dialog
- Multi-file case: if config JSON comes within a mix of other files, add to `unhandledFiles` (per requirement)
- On confirmation: call `ConfigImportExport.importFromUri()` and toast the result

### Files to modify
- `SetupActivity.kt`: add `.json` handling in classifier, add config import dialog state, add dialog composable

## Issue 2: Demo files in readiness view

### Current state
- Demo .dem files are imported to `setDir/demos/` directory
- No UI exists to view/manage imported demos
- The readiness view has: D2 section, D1 section, Music section, Mods section

### Plan
- Add a `DemosSection` composable after `ModsSection` (or within the same area)
- Uses `GameSectionHeader` pattern: "Demos" title, collapsible, summary shows "N demos, M MB"
- No selection checkbox on the header line, but a delete "X" to delete ALL demos
- When expanded, show a scrollable list of demo filenames, each with a delete "X" button
- Scan `setDir/demos/` for `.dem` files on each refresh
- Delete: individual demo delete removes the file; bulk delete removes directory contents
- Only show the Demos section if demo files exist (any)

### Files to modify
- `SetupActivity.kt`: add `DemosSection` composable, call it after `ModsSection`

## Issue 1a: Other unhandled file types

- `.plr` (pilot/player files) - currently only in `ALL_GAME_FILENAMES` if listed there
- `.iso` - not handled (only CUE+BIN disc images)
- `.wav` audio - not handled (only mp3/ogg/flac)
- `.mid`/`.midi` - not handled

These don't need changes right now -- just document the gap.

## Execution Order

1. Config JSON import via main button
2. Demo files in readiness view tree
3. Build + lint
