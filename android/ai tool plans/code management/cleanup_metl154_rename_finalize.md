# Cleanup tranche: finalize metl154 rename leftovers

## Scope

This tranche finishes the rename cleanup that tranche 1 intentionally left
partial:

- remove remaining `metl154_*` names where the code is already generic
- rename or delete `render.c` and `texmerge.c` investigation leftovers where
  the code is already generic or no longer justified
- shrink the alias bridge in `d1/arch/ogl/ogl.c` and `d2/arch/ogl/ogl.c`
  without changing runtime behavior
- keep extraction to `android/app/src/main/cpp/shared/` out of scope for
  this tranche, except for small shared rename cleanups needed to match the
  new names

Out of scope:
- moving large OGL helper bodies to `android/app/src/main/cpp/shared/`
- deleting broad OGL diagnostics wholesale
- changing merged-wall route selection or cached-premerge behavior

## Required validation

1. `./android/diff_vs_upstream.ps1 -Top 20`
2. `./android/run-code-quality.ps1 -Fix`
3. `./android/gradlew.bat :app:assembleDebug :app:testDebugUnitTest`
4. one smoke test if device availability permits

## Work items

- [x] Remove the metl154-only render-list diagnostics from `d1/main/render.c`
  and `d2/main/render.c`, and rename the surviving old-merge log tag to
  generic merged-wall wording
- [x] Rename remaining generic helpers in `d1/main/texmerge.c` and
  `d2/main/texmerge.c`
- [x] Rename shared name-gated helpers to neutral merged-wall wording where
  the code is already generic
- [x] Remove or reduce the `g_metl154_*` / `METL154_*` alias bridge in
  both `ogl.c` files where the replacement is mechanical and safe
- [x] Update this file with results and validation

## Notes

- Keep D1 and D2 changes mirrored unless a signature difference forces a
  small divergence
- Prefer small, anchored patches in both `ogl.c` files
- Do not change log semantics in this tranche unless the rename is part of a
  generic naming cleanup

## Results so far

- Removed the hardcoded metl154 render-list summary and portal-tracking logs
  from `d1/main/render.c` and `d2/main/render.c`
- Renamed the surviving old-merge experiment log tag in both render files
  from `[metl154exp]` to `[mwall_exp]`
- Moved the texmerge logging-target bitmap check behind a shared helper:
  `android_merged_wall_is_logging_target_bitmap(grs_bitmap *bm)`
- Removed the metl154-named texmerge helper from both games and renamed the
  texmerge log tag from `[metl154texmerge]` to `[mwall_texmerge]`
- Shrunk a first safe slice of the `ogl.c` alias bridge by removing the
  `METL154_DEBUG_*` and `METL154_EXPERIMENT_*` compatibility defines and by
  renaming `ogl_metl154_experiment_name()` to
  `ogl_merged_wall_experiment_name()` in both games
- Renamed the generic OGL logging-target bitmap helper in both games from
  `ogl_is_metl154_bitmap()` to
  `ogl_is_merged_wall_logging_target_bitmap()` and routed it through the
  shared `android_merged_wall_is_logging_target_bitmap()` helper
- Renamed the generic `skip_*`, `is_*_plain`, `*_force_*`, `*_state`, and
  screen-area locals in the main OGL draw path to merged-wall wording in
  both games without changing route selection or log behavior
- Renamed the generic OGL submit-context struct, state variables, route/upload
  helpers, and input-summary helpers in both games to merged-wall wording
- Fixed the follow-on Android build break in `d2/arch/ogl/ogl.c` by restoring
  the intended merged-wall local-state prologue after the rename pass
- Fixed the Android launcher Kotlin build error by replacing delegated-property
  smart-cast sites with `discId?.let { resolvedDiscId -> ... }` in
  `SetupActivity.kt`
- Removed now-unused Android introspection geometry helpers after the cleanup
  work so the native build runs without the new helper warnings introduced
  during this tranche
- Remaining `metl154` surface in `d1/` and `d2/` is now concentrated in just
  two files: `d1/arch/ogl/ogl.c` and `d2/arch/ogl/ogl.c`
- Remaining `metl154` line matches in `d1/` + `d2/`: 403
- Remaining `metl154` line matches in `d1/arch/ogl/ogl.c` /
  `d2/arch/ogl/ogl.c`: `202` / `201`
- `./android/diff_vs_upstream.ps1 -Top 20` totals moved from `+21018/-791`
  to `+20620/-801`
- `d1/main/render.c` churn moved from `430` total lines to `259`
- `d2/main/render.c` churn moved from `429` total lines to `258`
- `d1/arch/ogl/ogl.c` churn moved from `3365` total lines to `3346`
- `d2/arch/ogl/ogl.c` churn moved from `3399` total lines to `3379`
- Follow-on OGL shared-helper extraction phases completed after this rename
  tranche:
  - phase 15: extracted duplicated cached-texmerge logging helper to
    `merged_wall_debug.{h,c}`
  - phase 16: extracted duplicated cached-texmerge visible-dim and
    bitmap-init helpers to `merged_wall_debug.{h,c}`
  - phase 17: extracted duplicated cached-texmerge UV builder helper to
    `merged_wall_debug.{h,c}`
  - phase 18: extracted duplicated cached-texmerge cache-entry reset helper
    and centralized the cache-entry struct in `merged_wall_debug.{h,c}`
  - phase 19: extracted duplicated cached-texmerge cache-clear loop into
    shared helper `android_merged_wall_cached_texmerge_clear(...)`
  - phase 20: extracted duplicated cached-texmerge size selection and bounds
    logic into shared helper `android_merged_wall_cached_texmerge_choose_size(...)`
  - phase 21: extracted duplicated cached-texmerge cache-slot selection logic
    into shared helper `android_merged_wall_cached_texmerge_choose_slot(...)`
  - phase 22: extracted duplicated cached-texmerge reuse/commit/filter helper
    blocks into shared helpers:
    `android_merged_wall_cached_texmerge_try_reuse(...)`,
    `android_merged_wall_cached_texmerge_commit_entry(...)`,
    `android_merged_wall_cached_texmerge_set_render_filters(...)`, and
    `android_merged_wall_cached_texmerge_finalize_filters(...)`
  - phase 23: extracted duplicated cached-texmerge FBO render pass into
    shared helper `android_merged_wall_cached_texmerge_render_to_texture(...)`
  - phase 24: extracted duplicated cached-texmerge output-texture setup into
    shared helper `android_merged_wall_cached_texmerge_setup_output_texture(...)`
  - phase 25: extracted duplicated cached-texmerge finalize-orchestration
    into shared helper
    `android_merged_wall_cached_texmerge_finalize_entry(...)`
  - phase 26: extracted duplicated cached-texmerge slot reservation and
    eviction into shared helper
    `android_merged_wall_cached_texmerge_reserve_entry(...)`
  - phase 27: extracted duplicated Android MSAA FBO create and destroy
    helpers into shared helpers in `shared/ogl_msaa_android.{h,c}`
  - phase 28: extracted duplicated Android texture-label anchor and
    joined-label helper logic into `android_texture_debug.{h,c}`
  - phase 29: replaced the remaining duplicated Android screen-space overlay
    label block in both `ogl.c` files with the shared texture debug helper
  - phase 30: moved duplicated Android texture-debug and render-context
    global definitions out of both `ogl.c` files into `android_texture_debug.c`
  - phase 31: extracted duplicated Android texture-list stats and anisotropy
    reapply helpers into `shared/ogl_texture_android.{h,c}` and fixed the
    shared cached-texmerge runtime-state boundary exposed by Android validation
- Current `diff_vs_upstream` OGL churn after follow-on phases:
  - `d1/arch/ogl/ogl.c`: `+1672 -50 total 1722`
  - `d2/arch/ogl/ogl.c`: `+1696 -49 total 1745`

## Validation so far

- `get_errors` on touched files: clean
- `./android/diff_vs_upstream.ps1 -Top 20`: passed
- `get_errors` on `d1/arch/ogl/ogl.c`: clean
- `get_errors` on `d2/arch/ogl/ogl.c`: blocked by missing local include-path
  configuration in the editor (`SDL_types.h`, `physfs.h`, `GL/glew.h`), not by
  a syntax error introduced in this tranche
- `./android/run-code-quality.ps1 -Fix`: passed
- `./android/gradlew.bat :app:assembleDebug :app:testDebugUnitTest`: passed
  after fixing the D2 OGL local-state regression and the delegated-property
  Kotlin lookup sites
- Follow-on phase validations passed for both phase 15 and phase 16:
  - `./android/run-code-quality.ps1 -Fix`
  - `./android/gradlew.bat :app:assembleDebug :app:testDebugUnitTest`
  - `./run-windows-build.ps1 -Target both -Preset x86-release -BuildType RelWithDebInfo`
  - `./android/diff_vs_upstream.ps1 -Top 20`
- smoke test: not run yet

## Status

- [x] Completed
- [x] Validation complete
