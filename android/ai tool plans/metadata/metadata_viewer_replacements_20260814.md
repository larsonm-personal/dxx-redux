# Metadata Viewer Replacements

## Goal

Show level-specific replacements compared with base game values in the metadata viewer, initially for player ship size.

## Plan

- [x] Extend native D2 analysis output with base and mod player ship radii when they differ.
- [x] Parse replacement rows into the shared Kotlin metadata model and persisted results.
- [x] Add a Replacements section with Base game and Mod columns to level details.
- [x] Add parser and viewer-model regression coverage, including omission when values match.
- [x] Run scoped formatting, native tests, focused Android tests, and D1/D2/Android builds.

## Boundaries

- Do not show an empty Replacements section or unchanged rows.
- Keep D1 and unmodified D2 metadata output free of replacement rows.
- Preserve exact fixed-point values in metadata so small differences remain visible.

## Result

- Metadata rows now carry exact base-game and mod values for changed player ship size.
- The main metadata viewer shows a deduplicated Replacements table with Base game and Mod columns only when changes exist.
- Obsidian level 4 reports base radius 310325 and mod radius 310313; stock Counterstrike level 4 reports no rows.
- Cache generation 3 invalidates metadata results created before replacement details were serialized.
- Scoped quality checks, all 73 native tests, focused Android tests, both Windows builds, and the debug APK build passed.
