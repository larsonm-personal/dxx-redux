# Automap Next Objectives Position

## Goal

Place the automap next-objectives text directly below the existing level label in both Descent 1 and Descent 2.

## Plan

- [x] Identify the independent level-label and objective-list screen anchors.
- [x] Pass the level-label-aligned origin into the shared objective renderer.
- [x] Add introspection coverage for the objective-list origin.
- [x] Run focused native tests, D1/D2 builds, Android build, and automap emulator coverage.

## Boundaries

- Do not change objective selection, wording, count, color, or world-space markers.
- Preserve existing font and line spacing.

## Result

- D1 and D2 pass their level-label baseline and left inset into the shared renderer.
- The first objective uses that baseline plus one `LINE_SPACING`; subsequent objectives retain the existing spacing.
- Introspection verifies the objective block is below the level label, and the focused Obsidian emulator test observes three lines in that position.
- D1/D2 route snapshot and metadata scan tests, D1/D2 Windows builds, and the Android multi-ABI debug build pass.
