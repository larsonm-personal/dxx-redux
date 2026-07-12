# Guidebot Unexplored Wheel Slice

Date: 2026-07-11

## Goal

Expose `Unexplored` as a normal Guide-Bot wheel slice instead of the center action for both new and migrated touch layouts.

## Plan

- [x] Update the locked Guide-Bot preset.
- [x] Migrate existing center-bound `Unexplored` actions into a slice and clear the center.
- [x] Update migration and preset tests.
- [x] Run scoped formatting, unit tests, and Android build.
- [x] Record the separate generic center-action issue without broadening this change.

## Result

- New Guide-Bot wheels place `Unexplored` immediately before `Next` in the slice list.
- The Guide-Bot preset center is empty.
- Bundled Advanced and Claw layouts use the same configuration directly.
- Layout version 10 moves an existing center-bound `Unexplored` action into a slice and clears that center.
- Custom center actions are preserved.

## Verification

- `GuidebotLockedWheelTest` passes.
- `GyroToggleConfigTest` passes with the version-10 migration baseline.
- Scoped code-quality checks pass.
- Android `:app:assembleDebug` passes.

## Follow-Up

Generic radial-menu center-action dispatch remains a separate issue. This change removes Guide-Bot `Unexplored` from that path but does not alter center hit testing or action dispatch for other radial menus.
