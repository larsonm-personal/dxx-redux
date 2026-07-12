# Plan: D1/D2 OGL DXA Mask Helper Reuse, 2026-07-11

## Goal
- Remove D2's regrown local DXA mask loader and make both games use the existing Android-owned implementation
- Repair the shared helper's stale nine-argument `ogl_loadtexture` declaration by using a compile-time-checked eight-argument callback

## Baseline
- `d1/arch/ogl/ogl.c`: `+2046/-65` against `upstream/main`
- `d2/arch/ogl/ogl.c`: `+2176/-64`
- D2 local DXA mask helper and comment: exactly 37 added lines
- D1 already has two shared-helper call sites; D2 has two local-helper call sites
- Both live games define the branch's `ogl_loadtexture` with eight arguments
- The shared source directly declares and calls a nine-argument variant, which is C ABI undefined behavior even though current Android builds link

## Steps
- [x] Add an eight-argument callback type to `ogl_texture_android.h`
- [x] Remove the direct shared `ogl_loadtexture` declaration and invoke the supplied callback
- [x] Pass each game's local `ogl_loadtexture` at the two existing D1 and two D2 call sites
- [x] Delete only D2's local DXA mask loader and route its calls to the shared helper
- [x] Run scoped static and quality checks
- [x] Build and link both Android games for arm64-v8a, armeabi-v7a, and x86_64
- [x] Run Windows D1/D2 builds or record managed-environment limitations
- [x] Run focused D2 custom-texture/mask coverage
- [x] Record exact before/after metrics and update the campaign catalog

## Guardrails
- Preserve the `<basename>_mask.png` path, threshold, inversion, alpha texture setup, 8-bit upload, `is_png`, cleanup, and timing call sites
- Preserve D1 `textures/d1` and D2 set-specific resolved bitmap names
- Do not modify ETC2/KTX2 upload transactions or texture lookup order
- Do not add mask introspection to currently dirty unrelated files
- Do not change CMake; the shared source is already linked into both Android targets
- Keep desktop code unaffected under existing Android guards

## Expected result
- D1 inherited-file line count unchanged
- D2 OGL changes from `+2176/-64` to `+2139/-64`
- Combined OGL additions fall from 4,222 to 4,185
- Exact inherited-file reduction: 37 additions

## Outcome
- Added `android_ogl_loadtexture_fn` with the live eight-argument signature and passed each game's local function at all four call sites
- Removed the unsafe nine-argument shared `extern` and direct call
- Removed D2's exact 37-line local helper and retained one shared definition
- D1 remained `+2046/-65`; D2 moved from `+2176/-64` to `+2139/-64`
- `git diff --check` passed with only line-ending normalization warnings
- Both Android games compiled and linked for arm64-v8a, armeabi-v7a, and x86_64 with no callback-type warnings
- The Windows wrapper remained blocked before source compilation by managed access to `C:\local\vcpkg\buildtrees` and fresh-configure compiler/tool discovery
- The signed all-ABI test APK verified under v1, v2, and v3 with the existing debug certificate
- D2 `test_mod_loading.json5` passed 33 of 33 steps with the 128-pixel pack, 100 percent replacement coverage, and 167 concrete `Loaded mask:` entries from the shared implementation
- The test now enables texture logging before launch and restores the launcher preference afterward
