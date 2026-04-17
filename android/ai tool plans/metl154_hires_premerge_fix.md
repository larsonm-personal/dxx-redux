# Metl154 Hires Premerge Fix

## Goal

- keep hires textures enabled
- stop relying on Android's live two-texture path for plain transparent `tmap2` walls
- render the merged result through the normal single-texture path so behavior matches legacy texmerge more closely

## Plan

1. Add an Android-only cached premerge path for plain transparent `tmap2` walls in D2 OGL code.
2. Mirror the same path in D1.
3. Keep mask-backed `tmap2` overlays on the existing mask shader path for this tranche.
4. Run code quality and Android build validation.
5. Update this file with results and remaining risks.

## Status

- [x] D2 Android premerge cache added
- [x] D1 mirror added
- [x] Validation run
- [x] Results recorded

## Result

- Added an Android-only cached premerge path in D1 and D2 for plain transparent `tmap2` walls.
- The cache renders the merged bottom-plus-overlay result into a texture once, then feeds that texture through the normal single-texture `g3_draw_tmap()` path.
- Mask-backed `tmap2` overlays still use the existing mask shader path in this tranche.
- Added Android debug logging for cache create and reuse on `metl154`, plus a route log showing `merge_impl=gpu_cached_single` when the cached path is selected.

## Validation

- `android\run-code-quality.ps1 -Fix` passed.
- `android\gradlew.bat :app:assembleDebug :app:testDebugUnitTest` passed.
- Desktop/root `cmake` validation was blocked by the local environment, not by this change:
	- repository root has no top-level `CMakeLists.txt`
	- direct `d2` configure failed because local desktop SDL dependencies were missing (`SDL_mixer`)

## Remaining Risk

- This tranche only redirects plain transparent `tmap2` walls to the cached premerge path.
- Super-transparent mask cases still rely on the existing shader path and may need the same treatment later if Android-only mismatches show up there.
- Manual on-device retest is still required to confirm the returned METL154 artifact is gone from the previously bad camera angle.

## Follow-up Goal

- remove the remaining METL154 face-1 old-texmerge fallback now that the generic cached-premerge path has removed the original artifact
- keep a single Android behavior for plain transparent `tmap2` walls instead of mixing cached-premerge and legacy texmerge on different faces

## Follow-up Plan

1. Confirm from the newest Android log that the only downgraded wall is still using `merge_impl=auto_old_texmerge`.
2. Remove the METL154 face-1 fallback in D2 and D1 render code so Android uses the generic cached-premerge path everywhere that path applies.
3. Run file error checks and Android validation again.
4. Update this file with the cleanup result.

## Follow-up Status

- [x] Remaining face-1 fallback removed
- [x] Validation rerun
- [x] Cleanup result recorded

## Follow-up Result

- The newest Android log showed the remaining downgraded wall was still using `merge_impl=auto_old_texmerge` for `seg=83 side=3 face=1` while the rest of the tracked `metl154` walls were already on `merge_impl=gpu_cached_single` with 512x512 hires inputs.
- Removed that face-1 Android fallback from D1 and D2 render code so plain transparent `tmap2` walls now use the generic cached-premerge path instead of mixing cached-premerge and legacy texmerge.
- File error checks passed for the edited render files.
- `android\run-code-quality.ps1 -Fix` passed again.
- `android\gradlew.bat :app:assembleDebug :app:testDebugUnitTest` passed again.

## Follow-up Remaining Risk

- The confirmation log still comes from the pre-cleanup build, so an on-device retest is still needed to prove the downgraded face now routes through the cached-premerge path too.
- The mask-backed super-transparent path is still unchanged in this tranche.

## Verification Update

- `android\temp_game_logs\debuglog_20260417_102556.txt` shows `seg=83 side=3 face=1` on `route=merge_cached merge_impl=gpu_cached_single`, and the old `auto_old_texmerge` fallback strings are absent.
- Added a joined-texture debug overlay helper in D1 and D2 OGL code so cached-premerge faces can show two stacked labels using the original `tmap1` and `tmap2` names.
- Updated the Android `DbgAltTexMerge` startup comments to describe the current behavior instead of the removed per-face fallback.
- `android\run-code-quality.ps1 -Fix` passed again.
- `android\gradlew.bat :app:assembleDebug :app:testDebugUnitTest` passed again after the label fix.

## Anchor Follow-up Goal

- keep the joined-texture overlay labels on one stable screen anchor instead of letting them hop between the two triangle centroids of the same joined wall

## Anchor Follow-up Plan

1. Compare the cached-premerge joined-label insertion path with the stable single-texture label path.
2. Collapse duplicate joined labels by seg/side/name so both triangles of one joined wall contribute to one anchor per label line.
3. Mirror the fix in D2 and D1.
4. Re-run Android validation and record the result.

## Anchor Follow-up Status

- [x] Joined-label anchor fix added
- [x] Validation rerun
- [x] Result recorded

## Anchor Follow-up Result

- Compared the cached-premerge joined-label path against the regular single-texture label path and confirmed the main mismatch was that the merged path was sometimes anchoring labels from post-clip geometry inside `g3_draw_tmap_2_internal` instead of the original face geometry passed into `g3_draw_tmap_2`.
- Updated D1 and D2 OGL code so merged joined labels and `tmap2` overlay labels now compute their screen anchor from the original face point list, even when the merged draw path clips the polygon before submitting it.
- Kept the same centroid-in-3d-space projection math as the regular label path, but now apply it to the correct face vertices.
- File error checks passed for the edited header and native files.
- `android\run-code-quality.ps1 -Fix` passed again.
- `android\gradlew.bat :app:assembleDebug :app:testDebugUnitTest` passed again after the anchor fix.

## Anchor Follow-up Remaining Risk

- This fix makes merged labels use the same face-space anchor source as the regular labels. If any remaining movement shows up, the next place to inspect is whether a specific label is being emitted from more than one render path in the same frame.
- The Android build still reports pre-existing warnings in `gamerend.c`, `ogl.c`, and `SetupActivity.kt` that were not introduced by this change.