# Scroll selector row offset

## Goal

Allow a scroll selector's option row to sit above/left of, centered on, or
below/right of its trigger button, with above/left as the default.

## Plan

- [x] Define the three-position setting and shared geometry
- [x] Persist the setting in touch layouts and human-readable configs
- [x] Apply identical row placement in the editor preview and live overlay
- [x] Add the + / 0 / - control to the scroll-selector editor
- [x] Add focused tests and run formatting, Android tests, build, and emulator smoke

## Result

Scroll selectors now store a three-position option-row offset. `+` places a
horizontal row above its trigger or a vertical row to its left, `0` preserves
the centered layout, and `-` places the row below or to the right. The default
is `+`.

The live selector and touch editor use the same cross-axis offset calculation.
Paired weapon-row transitions are applied relative to that base offset. The
setting is serialized in both internal and human-readable touch layouts.

Scoped formatting, focused scroll-strip tests, the full Android unit suite,
and `assembleDebug` pass. The APK was installed on `emulator-5554`; the Advanced
preset exposed the + / 0 / - editor controls, switching to `-` saved
`BELOW_RIGHT`, and switching back to `+` saved `ABOVE_LEFT` without a crash or
ANR.
