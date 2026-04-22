# Shield Controller / TV Focus Fixes

## Goals
- Fix launcher D-pad navigation for Bluetooth controllers that report D-pad via HAT axes
- Make focused buttons visibly obvious on TV/controller with a central bright green border
- Give installer/import flows a default selected button on first open
- Add narrow diagnostics for in-game controller/menu input without changing the D1 music path yet

## Plan
1. [x] Centralize TV focus-border styling in one helper so launcher buttons use a single editable color
2. [x] Update launcher/controller input so `AXIS_HAT_X/Y` synthesizes D-pad key events for Compose focus navigation
3. [x] Add initial focus to import cards and installer dialogs, preferring the happy-path button (`Extract`, `Import`, `Done`)
4. [x] Add focused in-game input logging around the Kotlin/native handoff for Shield testing
5. [x] Run code quality plus a focused Android build/compile validation

## Notes
- Hold D1 music changes for a separate pass unless a concrete Android TV audio-path bug is identified
- Keep changes in android/ and launcher Kotlin where possible