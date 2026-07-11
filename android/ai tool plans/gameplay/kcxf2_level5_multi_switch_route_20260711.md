# KCXF2 level 5 multi-switch route

## Goal
Fix KCXF2RM level 5 so metadata and guidebot follow the playable progression:
blue key, blue door, all required shootable switches, red key, red door, and the
remaining end-of-level route.

## Plan
- [ ] Inspect the generated level 5 route and extract the relevant key doors,
  hidden doors, triggers, links, and blocking sides from the loaded level.
- [ ] Reproduce guidebot's post-blue-key route state and identify why a hidden
  door is preferred over the required multi-switch dependency.
- [ ] Correct the shared metadata/guidebot planner without adding mission-specific
  behavior.
- [ ] Add focused native coverage for a barrier requiring multiple independent
  trigger firings and an emulator regression for KCXF2RM level 5.
- [ ] Rebuild D1/D2 and Android, regenerate metadata, and verify the corrected
  level 5 route in both metadata and live guidebot behavior.

## Findings
- Investigation in progress.
