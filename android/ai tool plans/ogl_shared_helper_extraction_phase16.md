# OGL shared-helper extraction phase 16

## Scope

This tranche extracts two duplicated cached-texmerge utility helpers from
`d1/arch/ogl/ogl.c` and `d2/arch/ogl/ogl.c` into
`android/app/src/main/cpp/shared/merged_wall_debug.{h,c}`:

- `ogl_android_texmerge_visible_dim`
- `ogl_android_texmerge_init_bitmap`

In scope:

- add shared utility APIs for visible dimension and bitmap init
- replace local calls in D1 and D2 with shared helper calls
- remove dead local utility helper definitions from both `ogl.c` files

Out of scope:

- texmerge cache slot replacement policy
- UV construction helpers and draw path logic
- any changes to bitmap flag handling semantics

## Required validation

1. `./android/run-code-quality.ps1 -Fix`
2. `./android/gradlew.bat :app:assembleDebug :app:testDebugUnitTest`
3. `./run-windows-build.ps1 -Target both -Preset x86-release -BuildType RelWithDebInfo`
4. `./android/diff_vs_upstream.ps1 -Top 20`

## Work items

- [x] Add shared visible-dim and bitmap-init APIs/implementations
- [x] Replace D1 and D2 local call sites with shared helper calls
- [x] Remove dead local helper definitions in both `ogl.c` files
- [x] Run validation and record the result

## Notes

- Keep the same rounded visible-dimension math and fallback behavior
- Keep bitmap field initialization order/values unchanged

## Result

- moved duplicated utilities into shared helpers:
	- `android_merged_wall_cached_texmerge_visible_dim(...)`
	- `android_merged_wall_cached_texmerge_init_bitmap(...)`
- removed local `ogl_android_texmerge_visible_dim` and
	`ogl_android_texmerge_init_bitmap` from both `ogl.c` files
- D1 and D2 texmerge cache paths now call shared utility helpers directly
- validation passed:
	- `run-code-quality.ps1 -Fix`
	- Android `:app:assembleDebug :app:testDebugUnitTest`
	- `run-windows-build.ps1 -Target both -Preset x86-release -BuildType RelWithDebInfo`
	- `diff_vs_upstream.ps1 -Top 20`
- latest phase-16 churn lines:
	- `d1/arch/ogl/ogl.c`: `+2144 -50 total 2194`
	- `d2/arch/ogl/ogl.c`: `+2171 -49 total 2220`
- warnings remained non-blocking:
	- existing `loadgl.h` macro redefinition warnings
	- existing `ogl_get_free_texture` not-all-control-paths-return warning
	- Android `game_introspect.cpp` unused-function warnings