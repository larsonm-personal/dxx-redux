# Level metadata scroll UI tweaks

## Goal
- Keep the level metadata table header visible while the level rows scroll vertically.
- Add horizontal scroll indicator icons to the metadata table scroll area, matching the existing vertical affordance.

## Plan
- [x] Inspect the current level metadata dialog table and scroll indicator implementation.
- [x] Refactor the table so the header sits outside the vertical row scroller but inside the horizontal scroller.
- [x] Add left/right horizontal scroll indicators for the same scroll area.
- [x] Run scoped formatting/build validation.
