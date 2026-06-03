# Rounded Corner HUD Text Inset Tri-State Plan

## Goal
- Change `CornerTextInset` from an on/off setting to a three-state setting.
- Use `0=off`, `1=half`, `2=full`, with `1` as the default.
- Keep JSON export/import and live graphics setting application on the same config key.

## Steps
- [x] Update launcher graphics UI to save off/half/full values.
- [x] Update native graphics option handling to clamp and apply the tri-state values.
- [x] Update D1/D2 config comments/default semantics.
- [x] Extend focused config tests.
- [x] Run formatting and targeted verification.

## Notes
- Reuses the existing `CornerTextInset` key so existing value `1` becomes the new default half movement.
