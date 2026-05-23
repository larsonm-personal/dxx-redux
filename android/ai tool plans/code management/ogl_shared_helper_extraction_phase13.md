# OGL shared-helper extraction phase 13

## Scope

This tranche removes the local `ogl_log_metl154_palette_source` wrapper in
`d1/arch/ogl/ogl.c` and `d2/arch/ogl/ogl.c` by inlining the shared palette-log
helper at the texture-load call sites.

In scope:

- inline the palette-source logger calls in the PNG and stock texture load paths
- remove the dead local palette-source wrapper from both `ogl.c` files
- keep D1 and D2 mirrored where the underlying loader paths are shared

Out of scope:

- the higher-fanout logging-target wrapper
- any submit-context or draw-path logic
- any shared helper API changes

## Required validation

1. `./android/run-code-quality.ps1 -Fix`
2. `./android/gradlew.bat :app:assembleDebug :app:testDebugUnitTest`
3. `./run-windows-build.ps1 -Target both -Preset x86-release -BuildType RelWithDebInfo`
4. `./android/diff_vs_upstream.ps1 -Top 20`

## Work items

- [x] Inline the palette-source logger calls in D1 and D2
- [x] Remove the dead local palette-source wrapper from both `ogl.c` files
- [x] Keep the D1 and D2 Android merge paths mirrored after the inline changes
- [x] Run validation and record the result

## Notes

- Prefer small anchored patches in both `ogl.c` files
- Keep the existing bit masks and source labels unchanged (`png-pal`, `stock-pal`, `stock-native`)
- D1 has one more stock-path palette log call than D2; preserve the existing site layout in each file

## Result

- `ogl_log_metl154_palette_source` was removed from both `ogl.c` files
- all remaining local call sites now invoke `android_merged_wall_log_palette_source` directly
- validation passed with the existing known warnings unchanged:
	- `loadgl.h` macro redefinition warnings
	- `ogl_get_free_texture` not-all-control-paths-return warning
- `diff_vs_upstream.ps1 -Top 20` reported:
	- `d1/arch/ogl/ogl.c`: `+2222 -50 total 2272`
	- `d2/arch/ogl/ogl.c`: `+2249 -49 total 2298`