Touch overlay music testing fixes

Goal:
- Fix issues found during testing of the overlay music menu.
- Improve source-switch diagnostics enough that a no-report crash can be narrowed from Android debug logs.

Plan:
- [x] Read local instructions and inspect the current overlay panel.
- [x] Enlarge the volume slider touch target, especially at zero volume.
- [x] Make pause/resume visibly toggle between Pause and Play.
- [x] Move the source drop-down to the top row next to the Music label, make it larger, and draw it opaque.
- [x] Add source-switch logging and fix obvious crash-prone source-switch paths.
- [x] Run scoped formatting plus focused Kotlin/native verification.

Notes:
- Testing notes reported a crash after changing source without a crash report, so this tranche should add native/Kotlin breadcrumbs around source switching even if no local repro is available.
