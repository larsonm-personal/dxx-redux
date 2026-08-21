# Demo grouping and CD audio ownership

## Goal

Group all demo content behind one Mods/Levels row with a group checkbox and a
demo-only submenu, while keeping each demo individually selectable and
deletable. Keep GOG `.gog`/`.inst` CD audio artifacts owned by the CD audio
source rather than exposing them as unrelated Mods/Levels entries.

## Work plan

- [x] Trace the current Mods/Levels row model, ordering controls, demo storage,
  and CD audio source ownership/classification.
- [x] Add a demo group row whose checkbox enables or disables every demo and
  whose submenu preserves the current ordered-list row shape.
- [x] Classify and group GOG `.gog`/`.inst` pairs as CD audio content, retaining
  their source registry and physical ownership together.
- [x] Add focused tests for demo group toggling/order and GOG/INST adoption so
  no constituent becomes orphaned or independently misclassified.
- [x] Run scoped formatting, focused tests, an Android build, and a maintained
  high-level launcher check where practical.

## Status

Complete. Focused JVM tests, scoped code quality, the three-ABI debug build,
and `test_demo_group_file_set_content.jsonc` passed. The demo submenu was also
opened and inspected on the emulator.
