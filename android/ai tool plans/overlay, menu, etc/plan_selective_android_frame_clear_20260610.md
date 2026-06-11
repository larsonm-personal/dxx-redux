## Goal
Add an Android-only backing clear for frames that can leave stale border pixels, while skipping it for the common full in-engine frame path.

## Plan
- [x] Add an Android OGL helper that clears the current window backing to black before window compositing.
- [x] Call the helper from the event draw path only when the visible game window will not provide a complete backing frame.
- [x] Apply the same scoped hook to D1 and D2.
- [x] Run scoped code quality on the changed files.
