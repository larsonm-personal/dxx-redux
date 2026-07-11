# KCXF2 level 5 multi-switch route

## Goal
Fix KCXF2RM level 5 so metadata and guidebot follow the playable progression:
blue key, blue door, all required shootable switches, red key, red door, and the
remaining end-of-level route.

## Plan
- [x] Inspect the generated level 5 route and extract the relevant key doors,
  hidden doors, triggers, links, and blocking sides from the loaded level.
- [x] Reproduce guidebot's post-blue-key route state and identify why a hidden
  door is preferred over the required multi-switch dependency.
- [x] Correct the shared metadata/guidebot planner without adding mission-specific
  behavior.
- [x] Add focused native coverage and an emulator regression for KCXF2RM level 5.
- [x] Rebuild D1/D2 and Android, regenerate metadata, and verify the corrected
  level 5 route in both metadata and live guidebot behavior.
- [ ] Resolve the remaining corpus regressions listed below in a follow-up pass.

## Findings
- KCXF2RM level 5 trigger 6 alone controls the red-key grate. The two other
  nearby switches open different wall pairs; the level data does not encode a
  three-switch dependency for that grate.
- Proactive blue/gold/red collection sent the route through optional secrets.
  Keys are now acquired from actual route blockers, with ordered recovery only
  when the primary route cannot expose a usable dependency.
- Secret exits are no longer selected as normal end-of-level targets.
- Progress-aware path cost is retained during recursive key acquisition. Turning
  it off caused the KCXF2 level 5 hidden-door shortcut to return.
- The live guidebot regression passes blue key -> shoot trigger 6 -> red key.
- Trine 2 level 7 regressed because removing proactive key collection also
  removed its recovery path. Transactional key recovery restores its exact old
  six-step route and travel distance (6283.92762535642).
- Full host regeneration completed for 110 archives: 109 passed, one configured
  large archive skipped, and no scans failed.
- Corpus comparison reduced new `ok` -> non-`ok` transitions from 86 to 14.
  The remaining cases are concentrated in trigger dependency loops in Ascent,
  Counterstrike/TRINITY, and Phenomia, plus key failures in Ascent,
  Castaway Redux, Counterstrike/TRINITY, KCXF2RM level 6, and Plutonia.
