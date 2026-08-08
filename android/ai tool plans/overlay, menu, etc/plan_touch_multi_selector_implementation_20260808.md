# Touch multi-selector implementation

## Goal

Deliver a playable scroll-strip presentation for existing touch multi-selectors,
including editor tuning, Guide-Bot actions, D1 weapon selection, D2 fixed-tier
weapon rows, and exact game-thread weapon selection.

## Plan

- [x] Add selector presentation/orientation/config fields and pure geometry and
  weapon-row helpers with unit tests
- [x] Add scroll-strip editor creation, preview, properties, placement clamping,
  and non-interactive extent rails
- [x] Add scroll-strip runtime gesture state, rendering, magnification, haptics,
  Guide-Bot filtering, and weapon row switching
- [x] Add exact parameterized weapon selection from Kotlin through JNI to the
  game thread for both D1 and D2
- [x] Add input-demo direct-command recording/replay for exact weapon selection
- [x] Update a bundled touch preset to provide an immediately playable demo
- [x] Run scoped formatting, unit tests, Android native build/tests, and an
  emulator smoke test; fix failures
- [x] Mark completed work and record any deferred tuning items

## Deferred tuning

- Tune card size, row-switch distance, pitch, fade, and the default 2.0 center
  zoom from device play feedback.
- Consider separate visual themes or icon support after the interaction has
  settled; the first demo intentionally uses labels and existing ammo status.
