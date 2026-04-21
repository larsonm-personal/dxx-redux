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
- [ ] Remove or reduce the `g_metl154_*` / `METL154_*` alias bridge in
      both `ogl.c` files where the replacement is mechanical and safe
- [ ] Update this file with results and validation

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
- Remaining `metl154` surface in `d1/` and `d2/` is now concentrated in just
  two files: `d1/arch/ogl/ogl.c` and `d2/arch/ogl/ogl.c`
- Remaining `metl154` line matches in `d1/` + `d2/`: 587
- `./android/diff_vs_upstream.ps1 -Top 20` totals moved from `+21018/-791`
  to `+20654/-793`
- `d1/main/render.c` churn moved from `430` total lines to `259`
- `d2/main/render.c` churn moved from `429` total lines to `258`
- `d1/arch/ogl/ogl.c` churn moved from `3365` total lines to `3361`
- `d2/arch/ogl/ogl.c` churn moved from `3399` total lines to `3395`

## Validation so far

- `get_errors` on touched files: clean
- `./android/diff_vs_upstream.ps1 -Top 20`: passed
- `get_errors` on `d1/arch/ogl/ogl.c`: clean
- `get_errors` on `d2/arch/ogl/ogl.c`: blocked by missing local include-path
  configuration in the editor (`SDL_types.h`, `physfs.h`, `GL/glew.h`), not by
  a syntax error introduced in this tranche
- `./android/run-code-quality.ps1 -Fix`: not run yet
- `./android/gradlew.bat :app:assembleDebug :app:testDebugUnitTest`: not run yet
- smoke test: not run yet

## Status

- [x] In progress
- [ ] Validation complete
