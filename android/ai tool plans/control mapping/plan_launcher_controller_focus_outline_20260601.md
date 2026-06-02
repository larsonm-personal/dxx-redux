# Plan: Launcher Controller Focus Outline

## Status

Implemented and verified.

## Goals

- Make focused launcher controls show the shared green outline consistently,
  including sliders, filter chips, switches, radio rows, and custom clickable
  rows.
- Allow D-pad navigation to resume after a touch interaction on touch+controller
  devices.
- Fix the reported pages: Advanced Settings debug log sliders, Game
  Preferences, launcher Select Game chips, and MIDI Preview slider/Close
  traversal.

## Steps

- [x] Inspect shared focus utilities and affected pages.
- [x] Add shared focus-outline modifiers for Material controls and sliders.
- [x] Apply the shared modifiers to Advanced Settings, Game Preferences,
      launcher base game picker, and MIDI Preview.
- [x] Add tests for focus utility policy where feasible.
- [x] Run focused compile/tests and code quality.
- [x] Make required-file detail dialogs default to Close instead of Forget.
- [x] Re-run focused compile/tests and code quality after follow-up.
