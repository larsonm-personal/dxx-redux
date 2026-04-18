# Metl154 tranche 4: shader and GL-state audit

## Scope

This tranche implements Part C.3 from
`metl154_postfix_cleanup_and_debug_harness.md`:

- audit the merged-wall shader variants that survived the cleanup tranches
- audit Android-only GL state changes that were added during the metl154 investigation
- remove or merge any shader/state path that is no longer needed after the cached-premerge fix and shared extraction

Out of scope for this tranche:

- launcher debug option UI from Part C.1
- in-game debug panel work from Part C.2
- crosshair debug-trigger capture from Part D
- new regression harness file format from Part E

## Required validation

1. `android\run-code-quality.ps1 -Fix`
2. `cd android; $env:JAVA_HOME='c:\local\jdk-21'; .\gradlew.bat :app:assembleDebug :app:testDebugUnitTest`
3. `android\run_test.ps1 -ScriptName test_merged_wall_snapshot_regression.json5 -Game d2 -Install`
4. `android\run_test.ps1 -ScriptName test_merged_wall_two_pass_probe.json5 -Game d2 -Install`

## Work items

- [x] Audit every surviving merged-wall shader route and record whether it should stay, merge, or be deleted
- [x] Audit investigation-era GL state changes (`glPolygonOffset`, cull/depth toggles, wrap restores) and classify each as keep, move, or delete
- [x] Remove or simplify any shader/state path proven unnecessary after tranche 3
- [x] Re-run the merged-wall regression and confirm no route regression on Counterstrike L1
- [x] Update this file with findings and final validation

## Research notes

- The shared extraction in tranche 3 left the route-level snapshot and regression in place, so this tranche can validate behavioral equivalence with the existing `merged_wall_snapshot` automation.
- The user-visible default-path invariant is now face-level: `faces[0] = seg=83 side=3 face=1 center_hit=true route=old_texmerge merge_impl=auto_old_texmerge decision_reason=tri13_face1_stock_64x64`.
- The repaired two-pass invariant is also face-level: `faces[0] = seg=83 side=3 face=1 submit_nv=5 center_hit=true cull_sensitive=false route=force_two_pass merge_impl=gpu_two_pass`, with `faces[1] = face=0 submit_nv=3`.
- The snapshot selector must not rely on later draw order alone when two faces tie on center distance. In the probe path, that let face 0 outrank the targeted face 1 until the tie-break started preferring larger centered coverage first.
- The audit target is specifically Android-only merged-wall rendering state, not unrelated desktop GL paths.
- The GLES3 shim audit showed only the fixed-function compatibility path plus the external-program override hooks used by the retained merged-wall shaders. The metl154-era shader sprawl was no longer in the shim.
- The still-live route split after tranche 3 is small and intentional: cached premerge stays for the fixed wall path, the GPU two-pass path stays as the generic fallback, and the old texmerge path stays only behind the surfaced legacy experiment toggle.
- The dead investigation-only experiment branches were still present in D1 and D2 OGL even though the Android-facing control surface already only exposed `default` and `force_legacy_texmerge`.
- The old center-sample evidence was not face-local. It became actively misleading once the tracker dropped clipped polygons with more than 4 points.

## Findings

### Shader route audit

- Keep the narrow Android-only `old_texmerge` escape hatch as the validated fix path for the metl154 `SIDE_IS_TRI_13` face-1 wall when both source textures are stock 64x64 runtime GL textures.
- Keep `merge_cached` with `gpu_cached_single` for the non-target split face and for other cached plain-texture merged walls.
- Keep the GPU two-pass merged-wall fallback path for non-cached cases.
- Keep the legacy texmerge route only behind `MERGED_WALL_EXPERIMENT_FORCE_LEGACY_TEXMERGE`.
- Delete the dead investigation-only experiment branches that no longer had any live Android control path.

### GL-state audit

- The earlier narrowed fallback conclusion was invalid because it depended on a center sample that was not tied to the actual submitted face.
- The real harness bug was in merged-wall tracking: `MERGED_WALL_LOG_PT_COUNT` was 4, so the snapshot silently dropped the clipped `submit_nv=5` face while the renderer logs were already showing it.
- After raising tracker capacity to 16 and reinstalling the APK, `[mwall_track]`, `[metl154submit]`, and `merged_wall_snapshot` all agreed on the relevant two-pass face: `seg=83 side=3 face=1 submit_nv=5 center_hit=true route=force_two_pass merge_impl=gpu_two_pass`.
- The repaired snapshot also showed the secondary face as `face=0 submit_nv=3`, `selected_count=2`, `tracked_count=3`, `center_hit_count=1`, and no relevant cover events for the target frame.
- The corrected centered face reports `cull_sensitive=false`, so the current evidence no longer supports temporary cull disable as the leading cause.
- Polygon offset was restored in both D1 and D2, and the current validation run logs `metl154state ... force_poly_offset=1 force_depth_off=0` on the relevant force-two-pass submissions.
- The debug-mode-only temporary depth disable path was kept because this evidence pass did not challenge it.
- Texture-wrap restore logic was audited and left unchanged because this tranche did not find proof that it was dead or wrong.

## Implementation notes

- Removed dead merged-wall experiment constants from the shared Android header and collapsed the surfaced experiment control to `default` and `force_legacy_texmerge` only.
- Simplified JNI debug-flag naming and experiment-mode naming to the surviving merged-wall surface.
- Removed dead `clip_all`, `overlay_only`, `alpha_raw`, forced-RGBA, no-mip, and forced-stock experiment branches from both D1 and D2 OGL texture and draw paths.
- Removed the obsolete per-mode texture invalidation path on experiment apply. The experiment apply log now records `texture_reload=0` instead of pretending to reload deleted experiment paths.
- Added a maintained Android automation hook to force the GPU two-pass fallback path so it can be regression-tested directly.
- Extended merged-wall snapshot introspection with face geometry, split, cull-sensitivity, and `submit_nv` fields so the regression can assert on submitted face data instead of framebuffer samples.
- Raised merged-wall tracked-point capacity from 4 to 16 and added `[mwall_track]` logging so clipped 5-point submissions are preserved in the snapshot.
- Restored the historical Android-only runtime fallback in D1 and D2 `render_face()` so the target metl154 face routes to `old_texmerge` with `merge_impl=auto_old_texmerge` and `decision_reason=tri13_face1_stock_64x64`.
- Tightened `test_merged_wall_two_pass_probe.json5` to assert the corrected face-level invariant instead of center-sample output.
- Hardened `test_merged_wall_snapshot_regression.json5` so it explicitly clears `merged_wall_force_two_pass` before asserting the default path.
- Changed merged-wall snapshot face selection to prefer larger centered coverage before later draw order when center-distance ties occur, which keeps the targeted `submit_nv=5` face ranked first in the two-pass probe.
- Restored polygon-offset save/restore in the D1 and D2 two-pass fallback path and extended `metl154state` logging to include polygon-offset state plus `force_poly_offset`.
- Left the fallback two-pass depth suppression path as the only remaining debug-only state override.

## Validation result

- `android\run-code-quality.ps1 -Fix`: passed
- `cd android; $env:JAVA_HOME='c:\local\jdk-21'; .\gradlew.bat :app:assembleDebug :app:testDebugUnitTest`: passed
- `adb logcat -c; .\android\run_test.ps1 -ScriptName test_merged_wall_two_pass_probe.json5 -Game d2 -Install`: passed with `temp\probe_result_capture.json = {"result":"PASS"...}` and `temp\probe_introspect_capture.json` showing `debug_flags.merged_wall_force_two_pass=1`, `faces[0].face=1`, `faces[0].submit_nv=5`, `faces[0].route=force_two_pass`, `faces[1].face=0`, and `merged_wall_last_draw_state.force_polygon_offset=true`.
- `adb logcat -c; .\android\run_test.ps1 -ScriptName test_merged_wall_snapshot_regression.json5 -Game d2`: passed immediately after the probe without reinstalling, with `temp\snapshot_result_capture.json = {"result":"PASS"...}` and `temp\snapshot_introspect_capture.json` showing `debug_flags.merged_wall_force_two_pass=0`, `faces[0].face=1`, `faces[0].route=old_texmerge`, `faces[0].merge_impl=auto_old_texmerge`, and `faces[0].decision_reason=tri13_face1_stock_64x64`.

## Follow-up plan: evidence-driven fallback GL-state pass

- [x] Re-audit the surviving two-pass fallback block in D1 and D2 and identify the exact state toggles that still differ from the cached path
- [x] Add temporary evidence logging around the fallback path so emulator runs can prove whether polygon offset, cull disable, and depth disable are exercised
- [x] Run a targeted Android regression or scripted capture that hits the fallback path and review the logs
- [x] Remove or narrow any fallback GL-state change that the evidence shows is unnecessary
- [x] Re-run validation and update this file with the result of the evidence pass

## Follow-up plan: regression harness repair after visual regression

- [x] Invalidate the old center-pixel evidence and document why it was insufficient
- [x] Extend merged-wall snapshot introspection to include geometry and cull-sensitivity diagnostics for selected faces
- [x] Re-run the force-two-pass path on the current bad renderer state and capture face-local evidence plus debug logs
- [x] Restore the suspect fallback GL-state change and re-run the same capture to identify a stable good-vs-bad invariant
- [x] Update the Android regression script to assert on the corrected geometry/state invariant instead of center-pixel evidence
- [x] Re-run validation and update this file with the repaired regression coverage

## Status

- [x] Audit complete
- [x] Cleanup complete
- [x] Validation complete
- [x] Harness repair complete