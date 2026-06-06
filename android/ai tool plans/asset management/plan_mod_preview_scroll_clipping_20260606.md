# Mod Preview Scroll Clipping Plan

## Goal
- [ ] Fix mission ZIP constituent previews that show clipped last lines and do not become scrollable when content barely exceeds the visible area.

## Steps
- [ ] Inspect the mod details preview composables and identify the scroll/height constraint causing the clipping.
- [ ] Apply a small layout fix that preserves the existing detail dialog design.
- [ ] Add or update focused coverage if the affected layout has practical test hooks.
- [ ] Run scoped code quality and a focused Android test/build check.

