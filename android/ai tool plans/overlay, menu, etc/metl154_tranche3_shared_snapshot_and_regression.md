# Metl154 tranche 3: shared snapshot extraction and regression harness

This tranche covers the next three requested follow-ups after tranche 2:

- move the surviving generic merged-wall debug helpers out of D1 and D2 into shared Android code
- remove the remaining tranche-1 compatibility aliases and finish the generic naming cleanup on the retained snapshot path
- build a regression harness around the generic snapshot path so a route regression on the original Counterstrike L1 wall fails automatically

## Scope

In scope:

- extract shared merged-wall debug state and retained snapshot bookkeeping from `d1/arch/ogl/ogl.c` and `d2/arch/ogl/ogl.c`
- extract the render-side Android draw-face context and per-face merged-wall logging helpers from `d1/main/render.c` and `d2/main/render.c`
- remove the compatibility aliases from `android/app/src/main/cpp/shared/debug_tex_overlay.h`
- replace retained internal `metl154_*` names on the shared snapshot path with generic merged-wall names
- replace the hardcoded `metl154_geometry` introspection block with generic merged-wall snapshot introspection
- extend automation so scripts can request a merged-wall snapshot and place the player on a deterministic view for assertions
- add one Android regression script that loads D2 Counterstrike L1, places the player on a deterministic merged-wall view, requests a snapshot, and asserts the cached route

Out of scope for this tranche:

- deleting every remaining `metl154`-specific experiment helper that still reflects genuinely texture-specific behavior
- a new custom regression level or broader graphics harness file format
- launcher UI changes beyond existing merged-wall controls

## Research notes

- The retained snapshot path is still duplicated in `d1/arch/ogl/ogl.c` and `d2/arch/ogl/ogl.c` under `metl154_*` names, but the logic itself is now generic after tranche 2.
- `render.c` still owns duplicated Android helpers for draw-face context setup/clear and per-face merged-wall logging.
- The merged-wall globals are still defined in each `ogl.c`, while `render.c`, `jni_main.c`, `game_automate.cpp`, and `game_introspect.cpp` all reference them through `debug_tex_overlay.h`.
- Current automation can set `merged_wall_mode` and `merged_wall_experiment`, but not `merged_wall_snapshot` or a deterministic in-game camera pose.
- Existing engine helpers are sufficient for a deterministic test pose: `compute_segment_center`, `compute_center_point_on_side`, `get_side_normal`, `vm_vec_normalized_dir_quick`, and `vm_vector_2_matrix`.
- The fixed regression target remains D2 Counterstrike L1, with the original bad family centered on `seg=83 side=3` and the adjacent `82/4/0 <-> 83/4/0` portal area.
- During implementation, the old `get_side_normal` dependency in shared automation was replaced with local face-normal computation built from the side vertex split so the shared C++ code does not depend on D1/D2 `gameseg.c` linkage details.

## Plan

- [x] Add shared merged-wall debug source/header files under `android/app/src/main/cpp/shared/`
- [x] Move merged-wall globals and retained tracked-face / cover-event / snapshot state into the shared source
- [x] Move Android draw-face context setup / clear and per-face merged-wall logging into the shared source
- [x] Update D1 and D2 OGL code to call shared helpers instead of owning duplicated retained snapshot logic
- [x] Update D1 and D2 render code to call shared helpers instead of owning duplicated Android draw-face helpers
- [x] Remove the compatibility aliases from `debug_tex_overlay.h` and update call sites to use `MERGED_WALL_*` and `g_merged_wall_*`
- [x] Expose the last merged-wall snapshot through `game_introspect.cpp`
- [x] Remove the hardcoded `metl154_geometry` introspection block
- [x] Extend automation with merged-wall snapshot triggering and a deterministic view-placement step
- [x] Add a regression script under `android/game_scripts/`
- [x] Run code quality, Android build/tests, and at least the new regression script

## Status

- [x] Shared extraction complete
- [x] Regression harness complete
- [x] Validation complete

## Implementation notes

- Shared merged-wall bookkeeping now lives in `android/app/src/main/cpp/shared/merged_wall_debug.c` and `merged_wall_debug.h`, with D1 and D2 calling the shared helpers instead of owning separate retained snapshot code.
- Shared introspection now serializes `merged_wall_snapshot` from the retained snapshot result and no longer emits the hardcoded `metl154_geometry` block.
- Shared automation gained `face_view`, `merged_wall_snapshot`, and `clear_robots` support so the regression can request a snapshot after removing combat noise from the mine.
- The final regression keeps the stable wall-level invariant in the assertion set: the selected merged wall stays on `seg=83`, `side=3`, `route=merge_cached`, and `merge_impl=gpu_cached_single`.
- Exact triangle index on that side was not stable enough across view-placement runs, so the final regression does not assert `faces[0].face`.

## Validation target

- `android\run-code-quality.ps1 -Fix`
- `android\gradlew.bat :app:assembleDebug :app:testDebugUnitTest`
- Android emulator regression run through `android\run_test.ps1`

## Validation result

- `android\run-code-quality.ps1 -Fix`: passed
- `android\gradlew.bat :app:assembleDebug :app:testDebugUnitTest`: passed
- `android\run_test.ps1 -ScriptName test_merged_wall_snapshot_regression.json5 -Game d2 -Install`: passed with `PASS (file-based, 26/25 steps, 7680ms)`
