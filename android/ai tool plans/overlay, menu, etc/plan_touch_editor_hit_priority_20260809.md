# Touch editor hit priority

## Goal

Make discrete touch controls win selection when they overlap broad analog stick
or drag-region controls in the touch overlay editor.

## Plan

- [x] Trace hit collection and overlap resolution in the touch editor
- [x] Define and implement explicit control-category selection priority
- [x] Add focused regression coverage for overlapping controls
- [x] Run scoped formatting, unit tests, Android build, and emulator smoke check
- [x] Record the completed behavior and any remaining limitations

## Result

Discrete editor controls now exclude overlapping analog-stick bodies, floating
zone edges, axis-region bodies, and axis-region edges from the current hit
stack. Overlapping discrete controls still use the existing tap-to-cycle
behavior. No remaining limitation is known for the requested priority rule.
