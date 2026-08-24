# GuideBot route stability regression

## Goal

Stop GuideBot from repeatedly reversing and resetting its route after the
impassable-grate routing correction, while retaining alternate-route planning
around non-flyable shoot-through walls.

## Plan

- [done] Trace prepared-route certification, live replanning, and route
  decision reuse across successive GuideBot updates.
- [done] Implement the smallest lifecycle correction that preserves a valid
  live replan without restoring unreachable prepared-route fallbacks.
- [done] Add a regression covering stable repeated routing and the existing
  impassable-grate case.
- [done] Run scoped formatting, native tests, and D1/D2 Windows builds.

## Results

- The impassable-grate correction causes affected end-of-level routes to use
  the full current-state planner. Those plans intentionally have an unchecked
  reusable certificate.
- Route adoption previously required the old certificate even when a completed
  current-state replan published identical guidance. Routine audits therefore
  replaced the active movement path and repeatedly turned GuideBot around.
- Equivalent current guidance now retains the active path. An uncertified path
  is still stopped while replacement guidance is unavailable or calculating,
  and changed guidance still replaces it.
- Added a route-adoption regression and an Android GuideBot diagnostic logging
  each retain, replace, or stop decision.
- Scoped code quality passed. All 43 D2 native tests passed. Android
  `assembleDebug` passed for arm64-v8a, armeabi-v7a, and x86_64. D1 and D2
  Windows builds passed.
