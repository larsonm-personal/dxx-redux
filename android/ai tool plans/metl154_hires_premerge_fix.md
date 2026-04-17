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