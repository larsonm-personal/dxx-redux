# Score Added Counter Stack Plan

## Goal
- Move optional robot and hostage HUD counter lines up when the temporary +score line is not actually drawn.

## Steps
- [x] Inspect D1/D2 upper-right HUD draw ordering and +score visibility conditions.
- [x] Add a shared local predicate for whether the +score HUD line is visible.
- [x] Use that predicate for both +score drawing and counter line spacing in D1/D2.
- [x] Run formatting and targeted verification.

## Notes
- Keep the existing top-right timer spacing unchanged.
- Mirror changes in D1 and D2.
