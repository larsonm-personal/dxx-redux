# Gamepad navigation across game menus

## Scope

Implement the menu-navigation audit findings for Android D1 and D2 while preserving desktop input and the existing launcher/gameplay bindings

- [x] Route menu A/B/D-pad and menu shortcut buttons before gameplay/meta bindings, retaining native A hold behavior for pilot deletion and weapon reordering
- [x] Keep button releases paired with their original destination across menu transitions
- [x] Support stick navigation in Android overlays and Guide Bot menus without moving the game or a covered native menu
- [x] Add controller navigation to legacy controls pages and A/B dismissal to scores, credits, pause, and netgame information
- [x] Expose controller actions and visible hints for demo deletion/conversion, high-score reset, and multiplayer preset deletion/defaults
- [x] Add integration coverage through actual Android KeyEvent/MotionEvent routing, including remapped buttons, overlays, both native menu implementations, and special screens/actions
- [x] Run scoped code quality, Android build/unit tests, Windows builds, and the D1/D2 emulator integration test
- [x] Review the full diff and update the bug checklist after verification

## Implementation direction

Menu navigation owns controller input while a menu is frontmost. Gameplay bindings resume in gameplay and automap, and dedicated Menu bindings remain available where they do not override menu A/B or context actions. Preserve native joystick A delivery because native menus use press/release and long-hold semantics

Use small shared Android helpers for native menu event translation and Kotlin navigation state, with narrowly guarded hooks in both engines. Add controller shortcuts to the existing native action handlers so confirmations and persistence stay in the engine

## Validation evidence

- Scoped mixed-language code quality passed
- Kotlin production compilation passed without new source warnings
- Both native engines compiled for Android arm64-v8a, armeabi-v7a, and x86_64; debug APK packaging passed
- Four focused controller unit tests passed with normal unit-test source compilation and no exclusions
- Both Windows MSVC builds and their upstream compatibility CTests passed
- D1/D2 emulator regression passed through test_gamepad_menu_navigation_unified.jsonc: D1 completed 184 steps and D2 completed 196 steps
- The regression verifies remapped buttons, native menus/listboxes, legacy binding capture/cancel and scrolling, Android overlay navigation/child dismissal, scores/credits/pause/netgame information, demo and preset confirmations, pilot long-hold deletion prompts, weapon reordering, gameplay/automap axes, and Guide Bot stick navigation/B/Y dismissal
- Runtime testing also exposed and fixed a missing font assignment on the Guide Bot menu's temporary Android scaling canvas
- Local evidence: temp/gamepad-menu-integration.log (D1 pass), temp/gamepad-menu-d2-final.log (D2 pass), temp/gamepad-unit-final.log, temp/gamepad-guidebot-fix-build.log, temp/gamepad-windows-build.log, temp/gamepad-windows-d1-tests.log, temp/gamepad-windows-d2-tests.log, and temp/gamepad-quality-final.log

## Controller behavior

Menus reserve A/B, D-pad, sticks, and their context buttons before gameplay bindings. Raw A press/release remains available for long-hold pilot deletion and weapon reordering

- A selects and dismisses informational screens; B cancels or closes
- L1/R1 scroll menus and legacy controls pages
- X deletes demos/presets or clears a legacy binding
- Y converts demos, resets scores/preset defaults, or restores legacy control defaults
- Y also closes an already-open Guide Bot menu
- Existing destructive-action confirmations remain in place
