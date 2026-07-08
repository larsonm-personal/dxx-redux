# Multiplayer loading palette investigation - 2026-07-08

## Problem
Multiplayer loading/intertitle screens can show the wrong palette, often brown-tinted colors, across multiple screens. In-level textures are now correct after the level-palette invalidation fix, so this pass should focus on 8-bit screen/intertitle palette lifetime and Android upload/conversion behavior.

## Plan
- [done] Trace D1/D2 multiplayer loading and intertitle drawing paths.
- [done] Compare those paths with single-player/menu palette setup.
- [done] Identify whether the bad palette is the SDL canvas palette, Android ARGB conversion LUT, or cached GL/paletted screen texture data.
- [done] Apply a small generic fix, avoiding level-specific or screen-specific hacks.
- [done] Validate with scoped formatting and Android build tasks.

## Notes
- Do not edit `android/outstanding_bugs.md`.
- Prefer a palette lifecycle fix over new one-off logging.
- D2 `StartNewLevel` stages `Current_level_palette` with `no_change_screen`, then draws the visible `TXT_LOADING` box before the final `gr_palette_load(gr_palette)`.
- That makes `nm_draw_background()` remap the cached menu/scores background through the staged level palette on Android OGL, which produces the brown-tinted loading/intertitle look. Multiplayer makes this especially visible because the following wait screens can persist.
- Fullscreen `newmenu` PCX caching also lacks a filename key, so switching between fullscreen menu/intertitle backgrounds can reuse the wrong cached bitmap/palette.
- Fix: D2 Android `show_boxed_message()` now saves the staged palette/fade/current-palette state, forces the menu palette only for the transient non-rendered loading card, draws/flips it, then restores the staged state and invalidates/remaps menu assets.
- Fix: D1 and D2 fullscreen `newmenu` PCX cache now tracks the filename and reloads if the requested background changes.

## Validation
- `.\android\run-code-quality.ps1 -Fix -Paths @('d1\main\newmenu.c','d2\main\newmenu.c','d2\main\gamerend.c','android\ai tool plans\crash, logging, diagnostics\multiplayer_loading_palette_20260708.md')` passed.
- `.\gradlew.bat :app:externalNativeBuildDebug --no-daemon` passed from `android\` with JDK 21.
