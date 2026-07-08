# Coop rendering difference audit

## Goal
- Audit D1/D2 code for coop-specific rendering differences
- Treat world geometry, texture selection, texture upload, lighting, and OpenGL state differences as suspicious
- Separate intentional multiplayer UI/object overlays from differences that could change static level pixels

## Plan
- [done] Inventory render-adjacent `GM_MULTI` / `GM_MULTI_COOP` branches
- [done] Inspect texture, OpenGL, texmerge, lighting, and geometry render paths
- [done] Classify remaining differences as suspicious, intentional, or simulation-state rather than render-state
- [done] Patch only if the audit finds another unreasonable coop world-render difference
- [done] Record validation or no-change rationale

## Findings
- The core mine render path (`render.c`, `3d/interp.c`) does not have coop-specific geometry or wall-texture branches.
- Android texture replacement was already patched in `ogl_allow_png()` so coop now follows the same replacement texture path as single player.
- The matching polygon-model replacement path in `polyobj.c` was still gated by `AllowCustomModelsTextures` for all multiplayer. This is the same category of problem as the texture replacement gate, except for objects/models instead of walls.
- `texmerge.c`, `piggy.c`, and `ogl_invalidate_game_palette_textures()` have Android multiplayer branches, but the audited branches are logging/cache-forensics only and do not choose different pixels.
- Dynamic lighting still has multiplayer policy differences: `AllowColoredLighting` can disable colored dynamic lights in multiplayer, and D2 caps multiplayer headlight reach. These can change lighting pixels, but they are exposed netgame options / inherited multiplayer behavior rather than hidden coop-only asset routing.
- Other render-adjacent differences are HUD/automap/netgame presentation (`gamerend.c`, `gauges.c`, `automap.c`), player ship color policy (`BlackAndWhitePyros` / `FairColors`), or full-screen flash reduction. They do not explain static level texture corruption.

## Patch
- In both D1 and D2 `draw_polygon_model()`, Android coop now allows loaded replacement polygon models even when the multiplayer custom models/textures option is off.
- Non-coop multiplayer and non-Android behavior still use the existing `AllowCustomModelsTextures` gate.
- Cleaned the player-ship texture range check in both `polyobj.c` copies to avoid the Android compiler's one-past-array warning while preserving the same color lookup.

## Validation
- `.\android\run-code-quality.ps1 -Fix -Paths @('d1/main/polyobj.c','d2/main/polyobj.c','d1/arch/ogl/ogl.c','d2/arch/ogl/ogl.c','android/ai tool plans/crash, logging, diagnostics/coop_rendering_difference_audit_20260707.md')` passed.
- `.\run-windows-build.ps1` passed for D1 and D2.
- `.\android\gradlew.bat -p android :app:externalNativeBuildDebug --console=plain` passed for debug Android native builds with no C/C++ warnings from the touched files after the range-check cleanup.
- `git diff --check` passed; Git reports line-ending normalization warnings for the touched upstream `polyobj.c` files.
