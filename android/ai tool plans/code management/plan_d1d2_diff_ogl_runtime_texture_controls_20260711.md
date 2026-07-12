# Plan: D1/D2 OGL Runtime Texture Controls, 2026-07-11

## Goal
- Centralize the byte-for-byte identical Android texture-filter runtime logic from both inherited OGL files
- Keep MSAA pending handling local so the texture tranche has no framebuffer callback and does not overlap the later MSAA lifecycle tranche

## Baseline
- D1 `arch/ogl/ogl.c`: `+2046/-65`
- D2 `arch/ogl/ogl.c`: `+2139/-64` after the completed DXA-mask reuse tranche
- D1 lines 624-816 and D2 lines 634-826 are 193 lines each and byte-for-byte identical
- Modeled exact inherited-file reduction: 150 additions per game, 300 total
- Target result: D1 `+1896/-65`, D2 `+1989/-64`

## Steps
- [x] Add one pointer-backed texture-filter runtime state to `ogl_texture_android.h`
- [x] Add shared effective-filter, bound-texture filtering, and pending texture-option functions
- [x] Reuse the existing shared texture-byte, anisotropy, and raw TexFilt helpers
- [x] Define one static state beside each game's texture list
- [x] Replace the selective bind block and texture byte counter bodies with shared calls
- [x] Remove local anisotropy/effective/single/all-filter implementations
- [x] Replace the texture part of pending runtime options while retaining the exact local MSAA block
- [x] Route remaining effective-filter call sites to the shared function
- [x] Run scoped quality, search, and diff checks
- [x] Build and link both games for all Android ABIs
- [x] Run focused runtime graphics-option, MSAA FBO, and pause-viewport coverage
- [x] Record exact metrics and update the campaign catalog

## Guardrails
- The bound texture is already bound before the shared selective-filter call
- Effective filter zero must remain a no-op
- `OGL_FLAG_NOCOLOR` textures always obey menu filtering in every render context
- Menu context zero and HUD context two may force nearest independently
- All-texture apply skips invalid and no-color textures, generates missing mipmaps only when filtering is on, and never removes mipmaps
- Bind cache invalidation occurs only when textures were updated, except anisotropy keeps its existing unconditional invalidation after a valid pass
- Preserve pending order and barriers: anisotropy, raw TexFilt, requested/applied synchronization, then local MSAA
- Preserve both `start_frame` and menu-only `gr_flip` call sites
- Do not change texture upload, lookup, DXA mask, ETC2/KTX2, MSAA creation, or framebuffer flow

## Validation targets
- `git diff --check` and a search proving no local duplicate helpers remain
- D1 and D2 Android link for arm64-v8a, armeabi-v7a, and x86_64
- Windows both-game wrapper if the managed vcpkg/toolchain environment permits it
- Existing graphics-preference coverage for TexFilt, anisotropy, and MSAA pending paths
- D1/D2 level or automap launch, D2 MSAA smoke, custom-texture/mod loading, and merged-wall snapshot as applicable

## Outcome
- Added one shared pointer-backed runtime state and centralized effective filtering, bound-texture filtering, and pending texture-option dispatch in `ogl_texture_android.{c,h}`
- Reused `android_ogl_get_texture_bytes`, `android_ogl_apply_anisotropy_all`, and `android_ogl_apply_texfilt_all`; the first two no longer remain unused shared helpers
- Left each inherited OGL file with the same compact state initializer and shared calls while keeping the complete MSAA pending block local
- Removed 151 inherited additions from each game, 302 total:
  - D1 moved from `+2046/-65` to `+1895/-65`
  - D2 moved from `+2139/-64` to `+1988/-64`
- Shared code grew by 183 lines, so total source size fell by 119 lines while the inherited D1/D2 merge surface fell by 302 lines

## Validation result
- Static review confirmed the D1/D2 adapters remain mirrored, the local duplicate helpers are gone, and the effective-filter call sites use the shared function
- `git diff --check` passed with only line-ending normalization warnings
- Both games compiled and linked for arm64-v8a, armeabi-v7a, and x86_64, for six successful game/ABI links
- The focused unified runtime script passed 43 of 43 steps in both D1 and D2
- Each focused run recorded the expected production-path graphics log counts:
  - `gr_flip` pending applies: 2
  - `start_frame` pending applies: 8
  - anisotropy applies: 2
  - graphics option requests: 7
  - AF-effective TexFilt reapplies: 1
- D2 MSAA FBO smoke coverage passed
- D2 pause-menu viewport coverage passed
- The all-ABI test APK verified v1, v2, and v3 signatures and passed 16 KiB native-library alignment verification
- The Windows wrapper could not reach source compilation because the managed environment denied access to the external vcpkg build tree and could not configure a fresh compiler/toolchain
- The scoped quality wrapper could not execute the external clang-format binary under the managed sandbox; manual scoped style review and the non-mutating static checks completed
