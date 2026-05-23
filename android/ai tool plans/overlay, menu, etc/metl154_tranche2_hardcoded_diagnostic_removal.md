# Metl154 tranche 2: hardcoded diagnostic removal

## Scope

This tranche implements Part A.2 from
`metl154_postfix_cleanup_and_debug_harness.md`:

- remove hardcoded seg/side/face tracking and skip lists tied to the old
  metl154 investigation
- delete the associated focus, tracked-side, and cover-skip diagnostics
- keep the generic merged-wall draw path, generic Android draw-face context,
  and any debug plumbing that remains useful for later generic graphics work

Out of scope for this tranche:

- moving Android-only code into `android/app/src/main/cpp/shared/`
- redesigning the remaining generic debug harness
- crosshair snapshot redesign beyond what is needed to keep the code
  compiling after hardcoded list removal
- changing cached-premerge behavior

## Required validation

1. `android\run-code-quality.ps1 -Fix`
2. `android\gradlew.bat :app:assembleDebug :app:testDebugUnitTest`
3. Emulator smoke run if device availability permits

## Work items

- [x] Audit hardcoded seg/side/face diagnostic structures and their callers
- [x] Remove tracked-side snapshot machinery in D1 and D2 render code
- [x] Remove focus-face machinery in D1 and D2 OGL code
- [x] Remove cover-skip experiment modes and their hardcoded pair lists
- [x] Keep or adapt any generic snapshot plumbing needed after the deletions
- [x] Update this file with findings and results

## Research notes

### T2.R1: minimum useful generic snapshot state after deleting hardcoded lists

Status: answered

- The minimum useful generic snapshot state is the per-draw merged-wall face
  record plus generic cover events, not any hardcoded level-specific side or
  face list.
- The retained `metl154_tracked_faces` path still captures the fields that are
  broadly useful for later graphics debugging: draw order, render pass,
  seg/side/face identifiers, child, side type, doorway flags, texture ids,
  projected bbox, bbox area, and the sampled projected points/uvs already used
  by the snapshot path.
- The retained `metl154_snapshot_cover_events` path is sufficient to explain
  whether a later plain or mask draw overlapped the tracked merged-wall faces.
- Result for tranche 2: keep the generic tracked-face snapshot selection and
  cover-event logging, but delete the hardcoded tracked-side preload snapshots,
  focus-face caches, and pair-specific skip experiments.

### T2.R2: whether any remaining OGL logs still depend on specific seg/side labels

Status: answered

- After tranche 2 removal, no live D1/D2 OGL logging path depends on specific
  hardcoded seg/side/face labels such as `portal83`, `rock8331`, or similar.
- The surviving OGL logs report numeric draw context and generic snapshot
  ranking data, and `ogl_log_metl154_portal()` still derives portal facts from
  the current face context rather than from a fixed investigation table.
- Remaining matches for old focus/overlap labels were limited to historical
  planning markdown and archived debug log files, not live engine code.

## Status

- [x] Audit complete
- [x] Render cleanup complete
- [x] OGL cleanup complete
- [x] Validation complete

## Implementation notes

- Removed the render-side hardcoded tracked-side snapshot system from both D1
  and D2, including the fixed side list, side-signature helpers, side-geometry
  logs, frame-pending snapshot hook, and the load/restore callers in
  `gamesave.c` and `state.c`.
- Removed the OGL hardcoded focus-face and cover-skip machinery from both D1
  and D2, including focus-face tables, focus draw caches, overlap sampling,
  hardcoded cover-skip pairs, skip-decision helpers, and the related logging.
- Kept the generic merged-wall snapshot plumbing that still has value beyond
  the original metl154 investigation: tracked merged-wall faces near screen
  center, generic cover-event capture, Android draw-face context tagging, and
  the landed cached-premerge behavior.
- Deleted the now-dead cover-skip experiment names from the shared Android
  debug plumbing after the renderer-side branches were removed.

## Validation result

- `android\run-code-quality.ps1 -Fix`: passed
- `android\gradlew.bat :app:assembleDebug :app:testDebugUnitTest`: passed
- `C:\local\android-sdk\platform-tools\adb.exe devices`: passed with
  `emulator-5554 device`
- `C:\local\android-sdk\platform-tools\adb.exe logcat -c; .\android\run_test.ps1 -ScriptName test_launch_to_automap.json5 -Game d2 2>&1 ...`: passed
  with `PASS (file-based, 37/36 steps, 14068ms)`
