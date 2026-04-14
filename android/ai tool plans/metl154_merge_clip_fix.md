## Metl154 Merge Clip Fix

### Goal
- harden the Android plain metl154 merge path against near-plane and view-pyramid clipping artifacts
- keep the change narrow to D1 and D2 OGL merge code
- validate with Android code quality, debug assemble, unit tests, and an emulator smoke test

### Plan
1. Add a narrow clip helper in D2 `g3_draw_tmap_2` for Android plain metl154 draws.
2. Mirror the same helper and call site in D1.
3. Run Android validation and fix any regressions from the patch.
4. Update the metl154 phase note with the implementation result.

### Status
- [x] D2 helper added
- [x] D1 helper mirrored
- [x] Android validation run
- [x] Phase note updated

### Result
- Android plain `metl154` merge draws now take a narrow CPU clip path before
	entering the existing `g3_draw_tmap_2` body when the source polygon carries
	view-pyramid clip codes
- The change is mirrored in both `d2/arch/ogl/ogl.c` and `d1/arch/ogl/ogl.c`
	and intentionally leaves non-`metl154` merge draws alone for this first fix
	experiment
- The clipped temporary vertices reuse interpolated `u/v` from `clip_polygon`
	and collapse clipped lighting to mono via `p3_l`, which matches the existing
	data available in `g3s_point` and keeps the patch small
- Validation passed with `android\run-code-quality.ps1 -Fix` and
	`android\gradlew.bat :app:assembleDebug :app:testDebugUnitTest`
- The emulator smoke test could not run in this session because
	`adb devices` returned no attached devices, so `android\run_test.ps1` was
	left blocked waiting for a device and then stopped