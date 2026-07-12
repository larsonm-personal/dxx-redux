# D1/D2 kconfig scaled-render extraction plan

## Goal

Move the duplicated Android-only kconfig offscreen scaling and scrolling mechanism out of the upstream-original D1/D2 `kconfig.c` files while leaving private control-menu drawing and input policy local.

## Baseline

- `d1/main/kconfig.c`: 365 additions, 66 deletions versus `upstream/main`
- `d2/main/kconfig.c`: 377 additions, 67 deletions versus `upstream/main`
- Both files contain the same scale-coordinate, scroll clamp, offscreen bitmap/canvas, background, crop/blit, publish, and cleanup sequence
- `android_menu_scale.c/.h` already owns the scale computation, draw-state mutation, bitmap-region blit, and published geometry

## Work

- [x] Add a shared canvas-draw callback type to `android_menu_scale.h`
- [x] Add one shared kconfig scaled-render entry point that owns compute through publish/clear
- [x] Add a generic nonnegative scroll increment helper
- [x] Retain one compact local callback adapter per game for private `kc_menu` access
- [x] Replace the duplicated helper bodies and orchestration in both `kconfig.c` files
- [x] Preserve source rectangle, scale limits, background order, saved canvas restoration, scroll crop, and direct-render telemetry
- [x] Run scoped code quality
- [x] Build all Android ABIs
- [x] Build Windows D1 and D2
- [ ] Run focused controls/menu-scale integration coverage if the emulator is available
- [x] Record exact inherited-file reduction and update campaign documents

## Risk controls

- Do not move `kconfig_draw_contents`, item drawing, menu event handling, or control binding policy
- Invoke the callback only after the same scaled draw-state transition and with the same derived subcanvas
- Always restore the original canvas and scaled draw state before blitting or returning
- Keep the new entry point confined to the Android-owned shared module

## Completed result

- `d1/main/kconfig.c`: 365 additions to 291 additions versus `upstream/main`
- `d2/main/kconfig.c`: 377 additions to 303 additions versus `upstream/main`
- Exact inherited-file reduction: 148 additions
- Each game retains one four-line private draw adapter and its source-rectangle call; the shared module owns the full offscreen render transaction and scroll clamp
- Scoped code quality passed
- Android `externalNativeBuildDebug` passed for arm64-v8a, armeabi-v7a, and x86_64
- Windows D1 and D2 compiled and linked; D2 required renaming the same externally locked generated metadata executable before the final relink
- Focused emulator coverage is deferred because a pre-existing guidebot integration runner is actively using the sole emulator; the C14 runner did not stage an automation file and made no gameplay assertions
- `git diff --check` passed
