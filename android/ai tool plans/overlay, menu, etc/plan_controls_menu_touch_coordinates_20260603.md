# Controls Menu Touch Coordinates Plan

Status: implemented and verified

Created: 2026-06-03

## Request

In-game Options -> Controls can scroll to bottom rows:

- GAME SYSTEM KEYS
- NETGAME SYSTEM KEYS
- DEMO SYSTEM KEYS

On Android touch, the lowest tappable row appears to be GAME SYSTEM KEYS. NETGAME SYSTEM KEYS and DEMO SYSTEM KEYS appear visually present but taps do not select them.

## Initial Hypothesis

- The Controls screen is a standard `newmenu` created in `d1/main/menu.c` and `d2/main/menu.c`.
- The issue is likely in Android scaled menu pointer mapping or scrollable `newmenu_mouse` hit testing, not in the menu item definitions.
- The recent direct scaled render path changed where menu pixels are drawn, so touch coordinates may need to be mapped through the direct-render source rectangle before row hit testing.

## Findings

- The Controls menu has 22 items, and the bottom rows are indexes 19, 20, and 21.
- Emulator staging can scroll to the bottom and select both NETGAME SYSTEM KEYS and DEMO SYSTEM KEYS by ADB tap, so the base `newmenu` row bounds are not obviously wrong.
- The Android scaled-menu touch path remapped each event independently. If a touch started inside the enlarged menu but moved or released near an edge, the sequence could switch from scaled-menu coordinates back to raw screen coordinates before the menu release hit-test.
- SDL mouse button handlers use the stored `Mouse.x/y` from motion events rather than the x/y carried on the button event. Android was positioning the cursor on DOWN and MOVE, but not on UP, so a final release location could be missed if no MOVE was delivered there.

## Implementation Notes

- Lock the scaled-menu source/destination rectangles for one touch sequence after ACTION_DOWN starts inside the scaled menu.
- Remap MOVE and UP through that locked rectangle so one tap/drag remains in one coordinate space.
- Push a final zero-delta SDL mouse motion at ACTION_UP before pushing SDL_MOUSEBUTTONUP, so menu release hit-testing uses the final finger coordinate.

## Verification

- Ran scoped code quality with `android/run-code-quality.ps1 -Fix` on the native input file, this plan, and the new staging script.
- Built the debug APK with `./android/gradlew.bat -p android :app:assembleDebug`.
- Started `Nexus5X_Light_1` and installed the rebuilt APK through `android/helpers/run_test.ps1`.
- Ran `test_controls_bottom_stage_d2.json5`; it passed and left the Controls menu scrolled to the bottom.
- Verified real ADB taps on the rebuilt APK:
  - Tap at `800,875` opened the netgame system keys page.
  - Tap at `800,900` opened the demo playback controls page.

## Work Plan

1. Done: trace Android pointer coordinate conversion into `newmenu_mouse`.
2. Done: compare draw-time menu row positions with hit-test bounds for scrolled menus.
3. Done: patch the smallest shared D1/D2 or Android helper fix.
4. Done: add or extend an automation script that navigates to the bottom controls rows and verifies bottom menu geometry.
5. Done: run scoped code quality, Android build, and focused emulator test.
