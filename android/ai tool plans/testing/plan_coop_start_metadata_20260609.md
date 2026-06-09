# Coop start metadata

## Goal
Add a single per-level-set `coop_starts` metadata stat for single-player mission sets, aggregating the range of coop/player start counts seen across the set.

## Plan
- [x] Inspect native level metadata row/header generation and UI parsing.
- [x] Identify the engine data that represents player/coop starts after level load.
- [x] Add per-level counting and per-set range formatting.
- [x] Update launcher model/UI and regression JSON output if needed.
- [x] Validate with scoped tests or metadata generation.
- [x] Run scoped code quality checks.

## Notes
- Native metadata counts the first normal player start plus every `OBJ_COOP` start in each successfully loaded level.
- The root metadata result now formats the set-level range as `coop_starts` for blank or `normal` mission types.
- Launcher parsing, dialog header display, and automation JSON output now preserve the root `coop_starts` value.
- Added a JVM parser test for the `coop_starts` root/header field.
- Level metadata targets now carry mission descriptor `type` so anarchy sets do not get the single-player coop-start header.
- Validation passed with the focused JVM test, Android native debug build, and scoped code quality checks.
