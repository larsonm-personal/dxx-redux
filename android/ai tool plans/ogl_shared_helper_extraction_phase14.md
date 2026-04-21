# OGL shared-helper extraction phase 14

## Scope

This tranche removes the local `ogl_is_merged_wall_logging_target_bitmap`
wrapper in `d1/arch/ogl/ogl.c` and `d2/arch/ogl/ogl.c` by replacing its uses
with the shared predicate directly.

In scope:

- inline the merged-wall logging-target predicate in both `ogl.c` files
- remove the dead local predicate wrapper from both files
- preserve existing control flow and repeated predicate evaluation sites as-is

Out of scope:

- any new caching or control-flow cleanup around logging-target checks
- draw-path refactors beyond the wrapper removal
- shared helper API changes

## Required validation

1. `./android/run-code-quality.ps1 -Fix`
2. `./android/gradlew.bat :app:assembleDebug :app:testDebugUnitTest`
3. `./run-windows-build.ps1 -Target both -Preset x86-release -BuildType RelWithDebInfo`
4. `./android/diff_vs_upstream.ps1 -Top 20`

## Work items

- [x] Replace the logging-target wrapper calls in D1 and D2
- [x] Remove the dead local wrapper from both `ogl.c` files
- [x] Keep D1 and D2 mirrored after the predicate inline changes
- [x] Run validation and record the result

## Notes

- Keep the predicate call count and evaluation order unchanged within each file
- Prefer direct substitution only, with no nearby cleanup in the same hunk

## Result

- `ogl_is_merged_wall_logging_target_bitmap` was removed from both `ogl.c` files
- all remaining local uses now call `android_merged_wall_is_logging_target_bitmap` directly
- validation passed with the existing known warnings unchanged:
	- `loadgl.h` macro redefinition warnings
	- `ogl_get_free_texture` not-all-control-paths-return warning
- `diff_vs_upstream.ps1 -Top 20` reported:
	- `d1/arch/ogl/ogl.c`: `+2217 -50 total 2267`
	- `d2/arch/ogl/ogl.c`: `+2244 -49 total 2293`