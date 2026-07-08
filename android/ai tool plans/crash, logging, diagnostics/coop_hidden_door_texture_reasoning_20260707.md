# Coop hidden door texture reasoning

## Goal
- Explain why the tap probe reports `no_crosshair_face` even though a visible face exists
- Re-evaluate the D2 level 7 coop texture issue as a hidden-door / wall-animation problem
- Prefer a direct code fix or reasoned rollback over adding another speculative logging bit

## Plan
- [done] Trace what the tap probe actually tracks versus all rendered level geometry
- [done] Inspect hidden-door and wall-animation texture paths for single-player versus coop differences
- [done] Treat the hidden-door load reset patch as a failed visual hypothesis
- [done] Correct the failed camera/start-position explanation after same-position single-player testing
- [done] Identify the Android texture replacement gate as the remaining same-position coop difference
- [done] Revert the unrelated invalid-primary render fallback tweak from this test path
- [done] Patch Android coop to use the same texture replacement path as single player
- [done] Validate with scoped code quality and Windows build

## Findings
- `no_crosshair_face` is not proof that no level face is under the tap. The tap probe tracks merged-wall / secondary-texture faces, while hidden doors use primary textures (`WCF_TMAP1 | WCF_HIDDEN`).
- D2 level 7 hidden entrances such as `140:4` and `170:0` load with valid authored primary texture `608`. Clip 21 also starts at frame texture `608`, so the prior load-time hidden-door preservation patch could not change this level visually.
- The logged `910` texture references are real level-authored references. Texture `910` is out of range because D2 HAM has `NumTextures=910` and valid indices `0..909`.
- The same-position single-player test rules out the prior explanation that coop merely exposed different nearby geometry.
- Coop still differs from single player in the texture loader. `ogl_allow_png()` allows replacement textures in single player, but in multiplayer it requires `Netgame.AllowCustomModelsTextures`.
- The coop logs showed `custom_tex=0`, which means Android coop was explicitly taking the non-replacement texture path even though single player takes the replacement path.
- This can make the same face, same side data, and same UVs bind different GPU texture data in coop versus single player.

## Patch
- Reverted the failed `WCF_HIDDEN` gamesave change.
- Reverted the invalid-primary render fallback tweak because it did not explain the same-position single-player / coop difference.
- In both D2 and D1 OpenGL paths, allow Android coop games to load replacement textures regardless of the multiplayer custom-texture option.
- Leave non-coop multiplayer and non-Android behavior on the existing `AllowCustomModelsTextures` gate.

## Validation
- `.\android\run-code-quality.ps1 -Fix -Paths @('d1/arch/ogl/ogl.c','d2/arch/ogl/ogl.c','android/ai tool plans/crash, logging, diagnostics/coop_hidden_door_texture_reasoning_20260707.md')` passed.
- `.\run-windows-build.ps1` passed for D1 and D2.
- `.\android\gradlew.bat -p android :app:externalNativeBuildDebug --console=plain` passed for debug Android native builds.
- `git diff --check` passed.
