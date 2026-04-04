# Overlay Fixes, Label Colors, Launcher Spacing, and Selective Filtering

## Status: ALL PHASES DONE

## Summary
Five areas: texfilt cycling bug, texture label colors, launcher spacing, GPU time formatting,
and selective texture filtering / MSAA for different rendering contexts. All implemented
and tested -- build passes, lint clean, D2 integration test passes.

---

## Phase 1: TexFilt cycling bug + overlay reorder

### Bug root cause
In `ogl_start_frame()`, the unconditional sync `g_texfilt_level = GameCfg.TexFilt` runs
BEFORE the `g_texfilt_pending_apply` check. This destroys the JNI-set value every frame:

1. Kotlin sets `g_texfilt_level=1`, `g_texfilt_pending_apply=1`
2. Next frame: `g_texfilt_level = GameCfg.TexFilt` overwrites 1 back to 0
3. Pending-apply block copies 0 into `GameCfg.TexFilt` and flushes with TexFilt=0
4. 500ms later: stats poll reads `g_texfilt_level=0`, overlay shows "OFF"

### Fix
Move `g_texfilt_level = GameCfg.TexFilt` AFTER the pending-apply block:
```c
// BEFORE (broken):
g_texfilt_level = GameCfg.TexFilt;       // unconditional overwrite
if (g_texfilt_pending_apply) {
    GameCfg.TexFilt = g_texfilt_level;   // copies already-overwritten value
    ...
}

// AFTER (fixed):
if (g_texfilt_pending_apply) {
    g_texfilt_pending_apply = 0;
    GameCfg.TexFilt = g_texfilt_level;   // uses JNI-set value
    ...
}
g_texfilt_level = GameCfg.TexFilt;       // sync AFTER apply
```

### Overlay line reorder
Move TexFilt and Color lines down to just above AF/MSAA/Labels buttons:
```
 1. VIDEO  25fps
 2. frame  40ms avg / 42ms max
 3. [load bar]
 4. GPU: 2.10ms
 5. Cache: 450ms
 6. Tex: 32MB
 7. Hires: 100/405 (24%)
 8. Max: 256x256
 9. GL cap: 4096px
10. Render: 640x360 / 1280x720
11. Binds: 142 (85% cache)
12. Polys: 1200  shd:4  mask:0
13. Color: RGB565
14. [TexFilt button]
15. [AF button]
16. [MSAA button]
17. [Labels button]
```

### Files
- `d2/arch/ogl/ogl.c`: move `g_texfilt_level = GameCfg.TexFilt` after pending-apply block
- `d1/arch/ogl/ogl.c`: mirror
- `android/app/src/main/java/com/dxxredux/app/VideoInfoOverlay.kt`: reorder lines

### Test
- Cycle TexFilt in overlay: should stay at each setting, not revert
- Verify overlay line order matches spec above

---

## Phase 2: Texture label colors -- bright yellow in low-res mode

### Root cause
`BM_XRGB(63, 63, 0)` does a lossy round-trip through the Descent palette:
1. `gr_find_closest_color(126, 126, 0)` -- input range 0-126, palette range 0-63
2. Finds closest palette index (likely a dimmer yellow-brown)
3. At render, converts back to GL float via `gr_current_pal[idx] / 63.0`

The result depends on which palette entries exist. The base game palette may not have
a pure bright (63, 63, 0). On high-res mode the same code runs, but the labels appear
against different texture backgrounds -- the perceived contrast may differ.

### Fix
Since texture labels are entirely an `#ifdef ANDROID` feature, bypass the palette
round-trip and use direct GL color for the font. Add a `gr_set_fontcolor_rgb` function
(or set a global that `ogl_ubitmapm_cs` checks) that passes (r, g, b) directly instead
of a palette index.

Simpler alternative: use `gr_find_closest_color(63, 63, 0)` directly (without the *2
that BM_XRGB adds), which will find a more accurate palette match. But this still depends
on palette contents.

Simplest fix: set the ogl_ubitmapm_cs color array directly before the font draw by
setting `cv_font_fg_color` to a sentinel value (e.g., -2) and adding a branch in
`ogl_ubitmapm_cs` that uses hardcoded bright yellow (1.0, 1.0, 0.0) or green (0.0, 1.0, 0.0).

**Recommended approach**: Add an `#ifdef ANDROID` helper `gr_set_fontcolor_direct_rgb(r,g,b)`
that stores raw 0-255 RGB in three new globals. When the font color is this sentinel value,
`ogl_ubitmapm_cs` uses those globals (divided by 255.0) instead of the palette lookup.
This keeps changes minimal and android-only.

### Files
- `d2/2d/font.c` or `d2/arch/ogl/ogl.c`: add direct-RGB font color support (`#ifdef ANDROID`)
- `d2/include/gr.h`: declare `gr_set_fontcolor_direct_rgb` (`#ifdef ANDROID`)
- `d2/main/gamerend.c`: use `gr_set_fontcolor_direct_rgb(255, 255, 0)` for yellow,
  `(0, 255, 0)` for green
- `d1/` mirrors for all above

### Test
- No hires pack: all texture labels should be bright yellow
- With hires pack: hires labels bright green, base labels bright yellow
- Run on emulator, visually confirm

---

## Phase 3: Launcher graphics page -- reduce radio button spacing

### Current state
Radio button rows have `padding(vertical = 2.dp)` and Material3's default RadioButton
has built-in touch-target padding (~48dp minimum height). The section headers use 11.sp,
options 10.sp, inter-section spacers 6.dp + divider + 6.dp. Despite small text, rows are
tall because of RadioButton's Material touch-target.

### Fix
Override the RadioButton's minimum touch-target size by wrapping it in a
`Modifier.size(...)` or using `LocalMinimumInteractiveComponentSize`:

```kotlin
CompositionLocalProvider(
    LocalMinimumInteractiveComponentSize provides 0.dp
) { ... radio button content ... }
```

This removes the Material 48dp minimum. Then reduce `padding(vertical = 2.dp)` to
`padding(vertical = 0.dp)` to get natural line spacing.

Also reduce inter-section gaps from 6dp + divider + 6dp to 4dp + divider + 2dp.

### Files
- `android/app/src/main/java/com/dxxredux/app/GraphicsSettingsPage.kt`

### Test
- Open Graphics settings page, all 5 sections should be visible with minimal scrolling

---

## Phase 4: GPU time -- two significant figures

### Current format
`"GPU: ${gpuTimeUs / 1000}.${(gpuTimeUs % 1000) / 100}ms"` -- one decimal (e.g., "0.4ms")

### Requirements
Two significant figures, not two decimal places. Examples:
- 11234us -> "11ms" (two sig figs, no decimal needed)
- 1123us  -> "1.1ms"
- 456us   -> "0.46ms"
- 45us    -> "0.045ms"

### Fix
Use conditional formatting based on magnitude:
```kotlin
val gpuText = if (gpuTimerAvailable != 0) {
    when {
        gpuTimeUs >= 10000 -> "GPU: ${gpuTimeUs / 1000}ms"
        gpuTimeUs >= 1000  -> "GPU: ${gpuTimeUs / 1000}.${(gpuTimeUs % 1000) / 100}ms"
        gpuTimeUs >= 100   -> "GPU: 0.${gpuTimeUs / 10}ms"
        gpuTimeUs >= 10    -> "GPU: 0.0${gpuTimeUs / 10}ms"
        else               -> "GPU: 0.00${gpuTimeUs}ms"
    }
} else "GPU: n/a"
```

### Files
- `android/app/src/main/java/com/dxxredux/app/VideoInfoOverlay.kt`

### Test
- Run on S25, verify GPU timer shows two sig figs (e.g., "0.45ms" for 450us, "11ms" for 11000us)

---

## Phase 5: Selective texture filtering and MSAA

### Goal
Three separate filtering/MSAA scopes with independent control:
1. **3D world**: texture filtering + MSAA (existing controls, keep as-is)
2. **Menus/text/briefings/movies**: no filtering, no MSAA (default OFF, new toggle)
3. **HUD**: separate toggle for filtering (default ON)

### Architecture analysis

**Menus/briefings/movies**: These already render WITHOUT MSAA because `ogl_start_frame()`
(which binds the MSAA FBO) is only called for 3D game frames. Menus go through
`event_process()` -> `newmenu_draw()` -> `gr_flip()` without ever calling `ogl_start_frame()`.
So MSAA is already not applied to menus.

For texture filtering on menus: menu backgrounds and element textures go through
`ogl_bindbmtex()` -> `ogl_loadbmtexture_f(bm, GameCfg.TexFilt)`. The filtering is baked
at texture-load time. Options:
  a. Override at bind time: after `glBindTexture`, call `glTexParameteri(GL_NEAREST)` when
     in menu context -- cheap (~0.1us/call), no texture reload needed
  b. Load menu textures with TexFilt=0 always -- requires knowing which bitmaps are "menu"

Option (a) is simpler. Add a global `g_ogl_render_context` (0=menu, 1=3D, 2=HUD) and
check it in `ogl_bindbmtex()`. When context=MENU and menu-filtering is disabled,
override to GL_NEAREST after binding.

**HUD**: HUD renders inside `game_render_frame_mono()` after `render_frame()` returns.
The boundary is clear: `render_gauges()` and `game_draw_hud_stuff()` are the HUD functions.
Set `g_ogl_render_context = CTX_HUD` before these calls, and `CTX_3D` before/after
`render_frame()`.

For MSAA on HUD: the MSAA FBO is still bound during HUD rendering. To disable MSAA for
HUD, resolve the FBO early (before render_gauges) and continue rendering to the default
framebuffer. This is a small change in `game_render_frame_mono()`.

### Config model
Two new boolean settings stored in **descent.cfg via GameCfg** (not SharedPreferences),
exposed in the launcher Graphics settings page and communicated to the game via JNI:
- `menu_filtering`: false (default) -- menus/briefings get GL_NEAREST
- `hud_filtering`: true (default) -- HUD gets filtering (uses world TexFilt value)

Persistence: add `MenuTexFilt` and `HudTexFilt` fields to `GameCfg` (in config.h/config.c),
read/write them in `ReadConfigFile()`/`WriteConfigFile()`. The launcher reads descent.cfg
to populate the UI, same as TexFilt. JNI `nativeSetGraphicsOption` handles live toggle.

MSAA is already scoped to 3D-only so no new MSAA control is needed.

Note: movies already have `GameCfg.MovieTexFilt` as a separate control. These new
settings control menus/briefings and HUD only, not movies.

### Rendering context transitions
```
Menu frame:
  g_ogl_render_context = CTX_MENU   (set once in event loop or gr_flip path)
  -> ogl_bindbmtex checks context, overrides to NEAREST if menu_filtering off

Game frame:
  ogl_start_frame()
    g_ogl_render_context = CTX_3D
    (bind MSAA FBO)
  render_frame()                    -- 3D world, uses TexFilt normally
  g_ogl_render_context = CTX_HUD   -- set before render_gauges
  render_gauges()                   -- HUD, overrides to NEAREST if hud_filtering off
  game_draw_hud_stuff()             -- HUD text
  show_extra_views()                -- sub-viewports (re-enters 3D context internally)
  g_ogl_render_context = CTX_3D    -- restore after HUD
  gr_flip()                         -- resolve MSAA, swap
```

### Changes
- `d2/arch/ogl/ogl.c`:
  - Add `int g_ogl_render_context` (0=MENU, 1=3D, 2=HUD)
  - In `ogl_bindbmtex()`: after binding, if context is MENU or HUD and filtering
    disabled for that context, call `glTexParameteri(GL_NEAREST)` on both min/mag
  - Set `g_ogl_render_context = CTX_3D` in `ogl_start_frame()`
  - Set `g_ogl_render_context = CTX_MENU` at top of `gr_flip()` (for next menu frame)
- `d2/main/gamerend.c`:
  - Set `g_ogl_render_context = CTX_HUD` before `render_gauges()`
  - Restore `g_ogl_render_context = CTX_3D` after HUD rendering
- `d1/` mirrors for all above
- `android/app/src/main/cpp/jni_main.c`: handle new JNI settings
- `android/app/src/main/java/com/dxxredux/app/VideoInfoOverlay.kt` or
  `GraphicsSettingsPage.kt`: add toggle controls

### Note on render context default
The default context is CTX_MENU (0), meaning any rendering that happens before
`ogl_start_frame()` (menus, briefings, title screen) automatically gets the menu
filtering behavior. This is correct since `ogl_start_frame()` is only called for 3D
game frames.

### Test
- With menu_filtering OFF: menu backgrounds should be pixel-sharp (nearest neighbor)
- With hud_filtering ON: cockpit/HUD gauges should be smoothly filtered
- Toggle each in-game and verify texture appearance changes
- 3D world filtering should be unaffected by either setting

---

## Implementation Order

1. **Phase 1** -- TexFilt cycling bug fix + overlay reorder (small, high priority)
2. **Phase 4** -- GPU time two decimals (trivial, 1-line change)
3. **Phase 2** -- Label colors (small, android-only change)
4. **Phase 3** -- Launcher spacing (small, kotlin-only)
5. **Phase 5** -- Selective filtering/MSAA (medium, C + JNI + UI)

## Open Questions
- Should `menu_filtering` also affect the automap? (probably yes -- automap is 2D overlay)
- Should `hud_filtering` affect text labels like score/ammo or only the cockpit gauge
  bitmap? (probably both -- they're intermixed in the same render pass)
- Should TexFilt cycle button in overlay affect only 3D, or 3D+HUD? (3D+HUD makes sense
  since they share the TexFilt value when hud_filtering is ON)
