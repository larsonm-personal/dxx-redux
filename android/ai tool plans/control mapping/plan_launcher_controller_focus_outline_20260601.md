# Plan: Launcher Controller Focus Outline

## Status

Implemented and verified, including weapon autoselect D-pad reorder viewport
tracking.

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
- [x] Add green outlines to weapon autoselect controls and list selections.
- [x] Add green outlines to Advanced Settings log-file name rows.
- [x] Fix MIDI preview upward navigation when the progress slider is disabled.
- [x] Rework multiplayer outlines with a shared focus-visible control modifier.
- [x] Re-run focused compile/tests and code quality after this follow-up.
- [x] Fix multiplayer text-field D-pad escape paths on the main screen and Join
      by IP dialog.
- [x] Fix Host LAN Game dialog traversal from Cancel down into mission selection.
- [x] Rework Advanced Settings log-file row highlighting so the focused filename
      itself visibly receives the green outline.
- [x] Re-run focused compile/tests and code quality after this follow-up.
- [x] Add shared app-level text-field D-pad navigation helper.
- [x] Apply it to remaining launcher and multiplayer text-entry fields, including
      Host LAN Game level/max players.
- [x] Re-run focused compile/tests and code quality after this follow-up.
- [x] Keep grabbed weapon autoselect items visible while D-pad reordering.
- [x] Re-run focused compile/tests and code quality after this follow-up.
