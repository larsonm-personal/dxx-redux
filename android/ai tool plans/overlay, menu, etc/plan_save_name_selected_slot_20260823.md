# Save name only on selected slot

## Goal

In a brand-new game, keep empty manual-save slots visually empty until the player selects one, then prefill that selected slot's edit field with the current level name.

## Plan

1. [Complete] Trace the paired D1 and D2 save-menu initialization and selection/edit behavior, including the earlier default-level-name work.
2. [Complete] Change the smallest shared-equivalent code paths so only the chosen empty slot receives the default name.
3. [Complete] Extend the unified save/load integration test for the initial and selected-slot menu states.
4. [Complete] Run scoped formatting/lint, unified D1/D2 emulator regression, Android APK build, Windows D1/D2 build, and available CTest coverage.

## Notes

- Preserve existing save descriptions and non-Android behavior unless the underlying bug is platform-independent.
- Preserve unrelated working-tree changes in `d1/main/newmenu.c`, `d2/main/newmenu.c`, and Android files.
