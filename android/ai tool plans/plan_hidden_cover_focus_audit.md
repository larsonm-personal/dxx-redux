# Plan: Hidden-cover focus audit

## Goal

Explain why a visually centered bad merged-wall texture can still log a different front-face winner, then narrow the renderer audit to the surviving hidden single-cover path.

## Required validation

1. `android\run-code-quality.ps1 -Fix`
2. `cd android; $env:JAVA_HOME='c:\local\jdk-21'; .\gradlew.bat :app:assembleDebug :app:testDebugUnitTest`

## Current evidence

- [x] Level 1 taps 1 and 2 both center-ranked hidden cover `door45#0` while the selected snapshot face remained the partial merged front wall `rock346 + ceil025`
- [x] Level 2 tap 3 selects merged front face `seg=301 side=4 rock322 + ceil025`, with nearby hidden no-`tmap2` candidates logged as `rock322` and `force01#7`
- [x] Current focus ranking uses bbox containment and bbox distance only. It does not record whether the center point was inside the actual projected polygon of either the front face or the hidden cover
- [x] Tracked faces currently keep 3D vertices, full or projected bboxes, and split geometry, but not projected per-vertex screen coordinates in the snapshot logs
- [x] Cover events currently keep only cover bboxes, not per-vertex projected geometry
- [x] Snapshot center currently uses framebuffer midpoint `screen_w / 2, screen_h / 2`
- [x] The D2 reticle draw path in `d2/arch/ogl/ogl.c` centers on the current canvas `(grd_curcanv->cv_bitmap.bm_x + bm_w / 2, grd_curcanv->cv_bitmap.bm_y + bm_h / 2)`, so a canvas-vs-framebuffer-center mismatch is now an explicit audit item
- [x] Experiment `11` was still a no-op on the bad `door45#0` snapshots because the secondary-unit clear helper only ran on the metl154-only single-draw path, while the captured bad draws were ordinary transparent-wall single-texture draws that inherited a parent `tmap2=0x125` face context

## Initial research anchors

- Shared focus logging and ranking: `android/app/src/main/cpp/shared/merged_wall_debug.c`
- Snapshot center source: `android/app/src/main/cpp/shared/merged_wall_debug.c`
- Tracked face capture: `android/app/src/main/cpp/shared/merged_wall_debug.c`
- Cover-event capture: `android/app/src/main/cpp/shared/merged_wall_debug.c`
- Reticle center path: `d2/arch/ogl/ogl.c`
- D1 mirror reticle path: `d1/arch/ogl/ogl.c`

## Work items

- [x] Log both framebuffer center and current-canvas center on snapshot frames
- [x] Add projected-polygon hit evidence for tracked faces and cover events in focus logs
- [x] Add retrospective logging for later merged-wall draws that still hit the center after the top focus cover
- [x] Add an experiment mode that clears texture units 1 and 2 before Android metl154 single-cover draws
- [x] Broaden experiment `11` so it clears texture units 1 and 2 on the actual transparent-wall single-texture path used by the `door45#0` repro, not just the metl154-only single-draw shim
- [x] Broaden the 3rd merged-wall experiment setting to match merged-wall face context by parent `tmap2`, not transparent-wall `wid`, on D1 and D2 single-texture draws
- [x] Add `[mwall_texexp_skip]` reason logging when the 3rd merged-wall experiment setting bails before clearing secondary texture units
- [x] Fix the D1 and D2 native experiment-name helper so the 3rd merged-wall experiment setting no longer logs as `default`
- [ ] Re-run a user tap capture and decide whether tap 3 was a center mismatch, a polygon-hit mismatch, or a genuinely different visible face
- [ ] Audit the hidden single-cover render path for `cover_tmap2=0` behind merged front faces
- [ ] Record the first renderer-side branch points that differ between `door45#0` and the level 2 `rock322` or `force01#7` cases

## Tranche notes

- [x] Snapshot frame logs now emit both framebuffer center and current canvas bounds plus center
- [x] Focus-cover logs now emit screen-center and canvas-center bbox hits for both the tracked face and the hidden cover
- [x] Focus-cover logs now emit projected vertex counts and projected-polygon hit results for both the tracked face and the hidden cover
- [x] Snapshot focus logging now emits later merged-wall face and cover draws that still hit the screen center after the top focus cover draw
- [x] Experiment mode `11` now clears texture units 1 and 2 before Android metl154 single-cover draws, and is surfaced through JNI, automation, and the in-game overlay
- [x] Experiment mode `11` now also covers the transparent-wall single-texture merged-wall path, so the next phone retest should emit `[mwall_texexp]` on the `door45#0` repro instead of silently skipping it
- [x] Experiment mode `11` now keys the single-texture clear helper off merged-wall face context by parent `tmap2`, so `door45#0` no longer depends on transient `wid=6/7` state to reach `[mwall_texexp]`
- [x] Experiment mode `11` now emits `[mwall_texexp_skip]` when it is selected but the single-texture clear helper bails before touching texture units
- [x] D1 and D2 native `[metl154exp] apply` logs now label mode `11` as `clear_secondary_units_single` instead of `default`
- [ ] Current `test_door45_pose_repro.json5` rerun still lands in snapshot status `no_projected_faces`, so the new tags need a refreshed repro pose before device-side validation can prove them

## Validation

- [x] `android\run-code-quality.ps1 -Fix`
- [x] `cd android; $env:JAVA_HOME='c:\local\jdk-21'; .\gradlew.bat :app:assembleDebug :app:testDebugUnitTest`