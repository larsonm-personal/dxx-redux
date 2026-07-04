# Indicator Line Thickness Plan

## Goal
Make the guidebot and player indicator paths visually thicker than single-pixel renderer lines.

## Steps
- [x] Inspect the 3D line/rod rendering APIs used by the indicator overlay
- [x] Implement scoped thickness for indicator paths without changing unrelated engine lines
- [x] Run scoped formatting and Android debug build verification

## Notes
- Keep the change Android-local if possible
- Preserve the existing pathfinding, colors, and fade behavior
