# Guidebot Wheel Unlock and Next Goal Plan

## Goal
Keep the guidebot control wheel state in sync after unlock and add a way to resume the default goal progression.

## Steps
- [x] Locate the guidebot wheel state, unlock detection, and objective selection logic
- [x] Fix any obvious stale locked-state path without over-investigating sporadic repro
- [x] Add a "next goal" wheel action that clears the forced objective and resumes default blue-yellow-red-exit progression
- [x] Run scoped formatting and Android debug build verification

## Notes
- D2-only guidebot behavior should stay D2-scoped
- Avoid broad engine refactors for the intermittent locked-label report
