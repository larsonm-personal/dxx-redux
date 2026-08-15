# Base Game Robot Browser Plan

## Objective

Add a robot browser beside Level metadata for installed base-game data, allowing every robot from
the selected D1 or D2 game data set to launch the existing engine-rendered robot preview.

## Work

- [x] Trace the installed-file details UI and identify reliable D1/D2 base-data targets
- [x] Generalize robot preview requests so a base robot does not require a replacement or level row
- [x] Add a searchable or scrollable base-game robot list using the JSON name table
- [x] Add a Robot preview action beside Level metadata for the applicable base-game file
- [x] Extend launcher automation and request tests for base-game previews
- [x] Run scoped quality checks, Android tests/build, emulator coverage, and native host verification

## Constraints

- Resolve robot and model tables in native code rather than duplicating HAM parsing in Kotlin
- Load stock base-game data only; do not require a mission level or replacement metadata
- Keep D1 and D2 preview processes isolated
- Show numeric robot IDs even when the name table entry is blank
