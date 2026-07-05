# Base Mission Boss And Guidebot Metadata Plan 2026-07-04

## Goals
- Show `boss` route steps for base-game boss levels instead of labeling them as reactor steps.
- Ensure Descent 2 base mission metadata is generated into `game_data/mission_files`.
- Add level-detail notes when a Descent 2 level does not have an accessible caged guidebot.

## Investigation
- [done] Inspect current level metadata scanner fields for reactor, boss, and guidebot-related object/cage detection.
- [done] Find the JSON generation path for built-in mission metadata and the level detail UI model.
- [done] Add focused tests or fixtures for boss step selection and guidebot availability notes.

## Implementation Checklist
- [done] Add metadata fields needed to distinguish boss-objective levels and guidebot availability.
- [done] Update JSON serialization and generated mission metadata.
- [done] Update the level detail UI to show guidebot availability notes.
- [done] Run scoped formatter, metadata tests, and at least one build/test path.

## Results
- Boss robots now take priority over reactor/control-center targets in route metadata and use the label `Boss robot`.
- D2 guidebot metadata now includes `guidebot_count`, `guidebot_accessible`, and `guidebot_note`; notes are included in the level-detail dialog.
- Added `android/game_scripts/test_level_metadata_launcher_base_d2.json5` to generate base Counterstrike metadata as an array-shaped mission JSON.
- Regenerated `game_data/mission_files/Counterstrike.json` and `game_data/mission_files/Descent.json`.
- D1 conversion metadata now shows boss routes on levels 7 and 27. Entries 28-30 are the appended converted secret levels, so level 30 remains a reactor route.
- Counterstrike shows boss routes on levels 4, 8, 12, 16, and 20. Level 24 still stops at `red key unreachable`, which is a separate advanced pathing issue to study next.
