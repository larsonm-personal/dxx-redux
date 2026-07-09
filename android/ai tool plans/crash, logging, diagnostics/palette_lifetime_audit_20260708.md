# Palette lifetime audit - 2026-07-08

## Goal
Audit other places where palette-indexed screens, menus, movies, backgrounds, fonts, and textures can be drawn or uploaded with the wrong palette on Android OGL.

## Plan
- [done] Inventory palette load, staged palette, fullscreen bitmap, movie, menu, and OGL texture upload paths.
- [done] Classify each path as safe, probably safe, suspect, or known risky.
- [done] Identify small generic fixes or tests, without adding screen-specific hacks.
- [done] Record findings for implementation follow-up.

## Constraints
- Do not edit `android/outstanding_bugs.md`.
- Prefer palette lifecycle/cache-key fixes over one-off per-screen changes.
- Treat D1 and D2 duplication explicitly.

## Palette Model
- `gr_palette` is the logical palette used by remapping and most cached bitmap texture uploads.
- `gr_current_pal` is the active hardware/OGL palette used by transient blits and OGL color tinting.
- D2 `load_palette(..., no_change_screen=1)` intentionally updates `gr_palette` without updating `gr_current_pal`.
- OGL cached bitmap uploads normally convert indexed pixels through `ogl_pal`, which defaults to `gr_palette`.
- OGL transient blits (`ogl_ubitblt_i`) temporarily convert indexed pixels through `gr_current_pal`.
- Therefore a staged palette is safe only when nothing draws or uploads indexed art that depends on the active palette until `gr_palette_load(gr_palette)` catches up.

## Findings

### Known Risky Or Fixed Recently
- D2 loading boxed message during `LoadLevel()` is the exact failure mode that caused the brown multiplayer loading screen: `load_palette(Current_level_palette, 1, 1)` stages the level palette, then a menu-style boxed message draws before final activation. The current D2 Android guard in `d2/main/gamerend.c` is the right shape: save both logical and active palette state, draw the loading card under the menu palette, then restore.
- D1 `LoadLevel()` also draws a boxed message before final `gr_palette_load(gr_palette)`, but D1 uses `gr_use_palette_table("palette.256")` and has no D2 `last_palette_loaded`/pig palette split. It should still be watched, but it is less likely to produce the D2-specific menu palette regression.

### Suspect
- `nm_draw_background()` in `d1/main/newmenu.c` and `d2/main/newmenu.c` is the broadest remaining risk. It globally caches `nm_background`, remaps it into whatever `gr_palette` is active on first use, then reuses the cached bitmap and OGL texture across unrelated screens. D2 has an Android startup guard for `Game_wind == NULL`, but that is not a general palette identity check.
- `nm_draw_background()` callers include high scores, netgame info, controls, listboxes, guide-bot menus, and boxed messages. Most callers rely on "menu mode already has menu palette", but `set_screen_mode(SCREEN_MENU)` does not enforce a palette.
- Android scaled menus draw into a temporary indexed bitmap, then blit with `ogl_ubitblt_i`, which uses `gr_current_pal`. This is correct only if the offscreen drawing palette and active palette match.
- `gr_remap_bitmap_good()` mutates bitmap indices but does not invalidate an existing OGL texture. Most callers remap fresh bitmaps, but this is a structural stale-GPU-copy hazard if any uploaded bitmap is remapped later.
- D2 movies mutate `gr_palette` through `MovieSetPalette()` and draw frames through transient blits. The frame path tries to wait one frame after a palette change, then calls `gr_palette_load(gr_palette)`. This is probably acceptable for frames, but nested movie UI such as pause text/subtitles inherits movie palette state and should not be mixed with menu-background caching.
- Font rendering is mask/tint based, but the tint color index is usually computed from `gr_palette` while the OGL tint uses `gr_current_pal`. Any staged split can make text colors wrong even if the font atlas itself is fine.
- PVP stock-visual enforcement now allows visual replacements in single player and coop, and blocks them in non-coop multiplayer. The remaining palette risk in PVP is the BlackAndWhitePyros player-ship recolor path: it samples `gr_current_pal` and writes replacement indices with `gr_find_closest_color()`, so it assumes the logical and active palettes both describe the level palette.

### Probably Safe
- `nm_draw_background1()` now keys the fullscreen menu background by filename and restores its saved PCX palette on cached draws. It also forces the OGL texture to re-upload before draw. That is the right pattern for asset-owned palettes.
- Title screens and briefing screens load the PCX palette, activate it, draw a short-lived fullscreen bitmap, and free the bitmap on close/screen change. They mutate global palette state, but are not obviously stale-cache-prone.
- Automap backgrounds read a PCX palette, remap the background into the current game palette, activate that same game palette, and free the background on close. This is safe during ordinary automap flow.
- Save/load thumbnails were already moved to RGB storage and transient OGL blits using `gr_current_pal`, avoiding stale cached bitmap upload.
- The Android SDL software bridge rebuilds its ARGB lookup from the SDL canvas palette at each blit. It can display a wrong active palette, but it is not itself caching stale palette-expanded textures.

### Watchlist
- D1 credits do not explicitly switch to `credits.256`; D2 does. D1 remaps the star backdrop into whatever logical palette is current. This may be historical/intended, but it is not a strong invariant.
- Kill matrix reads the star background into `gr_palette`, activates it, and frees it after the window. This is probably safe, but it intentionally leaves palette restoration to later level/menu transitions.
- Endlevel terrain/satellite IFF art and D2 Hoard orb art are remapped into the current logical palette. They are safe if loaded after the correct level palette is staged, but inherit any upstream palette mistake.

## Recommended Follow-Up
- Add a palette identity to the framed `nm_background` cache in D1 and D2. Store a hash of the target palette used for remap; if `gr_palette` changes, free/remap/re-upload rather than reusing the old bitmap/texture.
- Consider making `gr_remap_bitmap_good()` OGL-aware by invalidating an existing bitmap texture after it changes indexed data. Audit parent/sub-bitmap cases first so this does not discard unrelated hi-res replacement textures by accident.
- Keep the D2 boxed-message palette scope, but if another staged-palette UI case appears, move that logic into a small Android-only helper rather than adding another local save/restore block.
- Extend generic palette diagnostics, not level-specific logging: screen mode, `Game_mode`, `gr_palette` hash, `gr_current_pal` hash, `ogl_pal` source/hash, D2 `last_palette_loaded`, and current palette name where available.
- Add a small automation/introspection regression for D2 multiplayer loading: with a visual texture pack enabled, coop should use mod textures and PVP should report stock-visual enforcement; loading/intertitle screens should show matching logical/current palette hashes before indexed offscreen blits.

## Implementation Checklist
- [done] Add palette identity to framed `nm_background` cache in D1 and D2.
- [done] Invalidate bitmap OGL texture after indexed remap in D1 and D2.
- [done] Run scoped code quality on touched files.
- [done] Run Android native build check.
