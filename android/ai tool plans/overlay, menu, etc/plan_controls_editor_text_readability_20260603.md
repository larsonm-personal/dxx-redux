# Controls Editor Text Readability Plan

Status: implemented and verified

Created: 2026-06-03

## Request

Continue enlarged text and micro-text resolution work for the controls editor pages:

- Customize keyboard
- Customize joystick
- Customize mouse
- Customize weapon keys

The requested order is:

1. Maintain higher resolution for tiny text during intermediate draw states, so the pages are usable even before text resizing.
2. Enlarge drawn text, widen the pages to an 85 percent screen-width cap instead of an 85 percent height cap, and make the pages scrollable.

## Initial Findings

- These pages are implemented by `kconfig.c`, not `newmenu.c`.
- D1 and D2 each have their own `kconfig.c`, so most engine-facing changes need to be duplicated.
- There is already an Android-only `android_menu_scale_compute_kconfig()` helper in `android/app/src/main/cpp/shared/android_menu_scale.c`.
- The current scaled kconfig path appears to draw into an intermediate low-resolution canvas and then blit/scale it, which can preserve layout but still causes micro-text softness.
- Baseline emulator introspection for Customize Keyboard reported `src=670x400`, `dst=768x459`, `scale=1.1475`; that is capped by 85 percent of render height.

## Implementation Notes

- Changed the kconfig scaling helper to target 85 percent of render width for controls editor pages.
- Changed D1 and D2 Android kconfig scaled drawing to render directly into the final destination canvas instead of drawing into a temporary bitmap and blitting it.
- Added Android kconfig scroll state and PageUp/PageDown plus mouse-wheel hooks. The shared scale result can now represent a visible source slice if a future controls editor layout becomes taller than the render surface.
- Current controls editor pages fit vertically at the new width-based scale on the 960x540 Android render surface, so the overflow path is present but not exercised by the focused automation.
- Added `test_kconfig_keyboard_stage_d2.json5` to stage all four D2 controls editor pages and assert that each kconfig page uses direct render and the new width-based scale.
- Baseline Customize Keyboard introspection used `dst=768x459`, capped by 85 percent of render height.
- Final installed emulator introspection used `dst=816x487`, `scale=1.2179104`, and `direct_render=true`, capped by 85 percent of render width.

## Work Plan

1. Done: trace current Android kconfig draw path in D1 and D2.
2. Done: change kconfig Android scaling to prefer direct high-resolution drawing into the final scaled destination.
3. Done: update kconfig scale computation so the editor pages can use up to 85 percent of screen width.
4. Done: add kconfig scroll state, PageUp/PageDown, mouse-wheel hooks, and source-slice scaling support for overflow cases.
5. Done: add automation coverage that stages Customize Keyboard, Customize Joystick, Customize Mouse, and Customize Weapon Keys.
6. Done: run scoped code quality, Android build, and focused installed emulator verification.

## Verification

- Passed: `android\run-code-quality.ps1 -Fix -Paths ...`
- Passed: `android\gradlew.bat -p android :app:assembleDebug`
- Passed: `android\helpers\run_test.ps1 -ScriptName test_kconfig_keyboard_stage_d2.json5 -Game d2 -Install -TimeoutSeconds 260`
- Automation result: `PASS`, 38/37 steps, 12770 ms.
- Final page assertion included Customize Weapon Keys step 35 with `menu.type=kconfig`, `menu_scale.active=true`, `menu_scale.direct_render=true`, and `menu_scale.dst.w >= 810` with `got 816`.
