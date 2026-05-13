# Touch Button Dynamic Weapon Labels - 2026-05-12

## Goal
Show the currently selected bomb, primary weapon, or secondary weapon on touch buttons bound to the corresponding fire or cycle actions, using a two-line label layout.

## Plan
1. [x] Create this plan file
2. [x] Inspect the current button label draw path and existing weapon-name sources
3. [x] Add dynamic two-line labels for bomb drop/cycle and primary/secondary fire/cycle buttons
4. [x] Add focused unit coverage for the label helper behavior
5. [x] Run focused Android unit tests and record results

## Notes
- Keep this change scoped to Android overlay rendering and tests.
- Reuse existing weapon-state data instead of introducing new native hooks.
- Buttons bound to `BTN_DROP_BOMB`, `BTN_TOGGLE_BOMB`, `BTN_FIRE_PRIMARY`, `BTN_CYCLE_PRIMARY`, `BTN_FIRE_SECONDARY`, and `BTN_CYCLE_SECONDARY` now resolve a dynamic label from the polled `WeaponState`.
- Dynamic labels are two lines: action on the first line and the current selection on the second line.
- D2 uses the real upgraded-weapon indices for names such as `Gauss`, `Guided Missile`, `Smart Mine`, and `Earthshaker` instead of collapsing to base wheel slots.
- Validation: `:app:testDebugUnitTest --tests com.dxxredux.app.GyroToggleConfigTest --tests com.dxxredux.app.RemainingKeyTouchActionsTest` passed.
- Validation: scoped ktlint passed for `TouchOverlayView.kt`.