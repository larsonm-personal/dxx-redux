# Plan: Double-Tap Fire Fix, mouseSensitivity Removal, Diagnostic UI Clarification

## Summary
1. Remove `mouseSensitivity` completely (no backward compat needed)
2. Add double-tap fire primary to right stick, fire secondary to left stick in default layouts
3. Create automation test to verify double-tap fire primary works
4. Fix the double-tap fire primary bug at the C engine level
5. Fix gyro diagnostic display visibility in Add Control dialog

## Phase 1: Remove mouseSensitivity
**Status: DONE**
- Removed field from AnalogStickControl data class in TouchControl.kt

## Phase 2: Add double-tap bindings to default layouts
**Status: DONE**
- advanced.json: look stick = "Fire Primary", move stick = "Fire Secondary"
- claw.json: same pattern
- simple.json: move stick = "Fire Primary"

## Phase 3: Fix diagnostic display visibility
**Status: DONE**
- Added verticalScroll(rememberScrollState()) to AddControlDialog Column

## Phase 4: Fix double-tap fire primary at C level
**Status: DONE**
- Root cause: `FireLaser()` in game.c uses `Controls.fire_primary_state` (level-triggered).
  Brief presses that arrive and release within the same game frame leave state == 0, losing the shot.
- Fix: Also check `Controls.fire_primary_count` (edge-triggered, incremented on every button-down
  event by kconfig). If count > 0 when FireLaser runs, fire even if state is 0. Consume count to
  prevent re-firing.
- Applied to both d2/main/game.c and d1/main/game.c under `#ifdef ANDROID`
- Note: uses `ANDROID` define (project convention), NOT `__ANDROID__`

## Phase 5: Test automation
**Status: DONE**
- Created test_fire_primary.json5:
  - Launches D2 level 1 on Trainee difficulty
  - Fires 4 brief (50ms) presses with 800ms gaps (well above ~500ms fire_wait)
  - Asserts energy < 100 (each shot costs ~0.25 energy at Trainee)
- Test passes with the C-level fix

## Verification
- Build: PASSED (assembleDebug)
- Lint: PASSED (all checks)
- Test: PASSED (test_fire_primary.json5)
