# Controls Editor Scroll Plan

Status: implemented and verified

Created: 2026-06-03

## Request

Make the enlarged controls editor pages scrollable now that the weapon edit pages are readable.

## Work Plan

1. Done: trace Android kconfig draw and input coordinate handling after the direct-render scaling work.
2. Done: make kconfig use a real destination viewport with a scrolled content offset when the scaled editor exceeds the visible render surface.
3. Done: add touch-drag, wheel, PageUp, and PageDown scrolling that adjust the same scroll state used by drawing.
4. Done: keep touch hit testing aligned with the scrolled source position by publishing the visible source slice through `android_menu_scale_publish()`.
5. Done: extend automation to prove scrolling state changes on the D2 Customize Weapon Keys page.
6. Done: run scoped code quality, Android build, and focused installed emulator verification.

## Implementation Notes

- The kconfig scaler now keeps the enlarged 85 percent width target while clipping the visible viewport to 85 percent of render height.
- On the 960x540 Android render surface, the controls editor renders as full content `816x487` with a visible viewport `816x459`.
- The renderer draws the full enlarged editor into a high-resolution bitmap, then copies the visible slice 1:1 to the screen. This avoids the unsafe negative subcanvas path and avoids scaling text down through a low-resolution intermediate.
- Mouse-wheel and PageUp/PageDown adjust `android_scroll_y`.
- Finger drag while holding the left mouse/touch button adjusts `android_scroll_y` after an 8-pixel movement threshold, and drag releases do not start editing the highlighted binding.

## Verification

- Passed: `android\run-code-quality.ps1 -Fix -Paths ...`
- Passed: `android\gradlew.bat -p android :app:assembleDebug`
- Passed: `android\helpers\run_test.ps1 -ScriptName test_kconfig_keyboard_stage_d2.json5 -Game d2 -Install -TimeoutSeconds 260`
- Automation result: `PASS`, 42/41 steps, 14240 ms.
- Focused assertions covered all four controls editor pages, then verified Customize Weapon Keys had `dst=816x459`, `render_h=487`, PageDown moved `menu_scale.src.y` from `70` to `94`, and PageUp returned it to `70`.
- A raw `adb input swipe` probe was inconclusive because it did not appear to reach the rotated game surface, so touch-drag behavior is implemented in engine code but not covered by the current automation script.
