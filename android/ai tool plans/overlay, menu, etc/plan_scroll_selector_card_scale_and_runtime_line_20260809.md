# Scroll selector card scale and runtime line

## Goal

Add an editable text/button scale for scroll-strip option cards, with the old
visual size represented as about 0.7 and a larger 1.0 default, and keep the
extent line exclusive to the touch editor.

## Plan

- [x] Define and persist the scroll-strip card scale
- [x] Apply the scale to card text, padding, and spacing
- [x] Add the card scale slider to the touch editor
- [x] Remove the extent line and center marker from live gameplay only
- [x] Add tests and run formatting, Android tests, build, and emulator smoke

## Result

Scroll-strip option cards now have a persisted `stripCardScale` setting. A
value of 0.7 reproduces the previous text and card size, while the new default
of 1.0 is proportionally larger. Text size, card padding, item packing, base
row separation, and paired weapon-row separation all respond to the setting.

The live overlay no longer draws the selector extent line or its center marker.
The touch editor continues to draw the offset line and end caps.

Scoped formatting, focused scroll-strip tests, the full Android unit suite, and
`assembleDebug` pass. The APK was installed on `emulator-5554`; the Guide
properties displayed `Text/button scale: 1.00`, and saving the Advanced preset
wrote `stripCardScale: 1` without a crash or ANR.
