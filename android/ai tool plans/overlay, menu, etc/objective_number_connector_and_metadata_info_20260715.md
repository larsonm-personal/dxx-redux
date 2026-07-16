# Objective Number, Connector, And Metadata Info

## Goals

- Prefix upper-left automap objective text with the same display number used by its world-space marker.
- Correct Obsidian level 1 objective 3's invalid full-map shoot-switch connector at the shared planner or overlay rule responsible.
- Move each metadata level row's info action to the second column, immediately after the level number.

## Plan

- [x] Trace automap display numbering, connector endpoints, and metadata table column order.
- [x] Capture Obsidian level 1 objective 3's live activation and aim positions and validate its visibility claim.
- [x] Share objective display-number calculation between upper-left text and world markers.
- [x] Correct invalid or misleading shoot-switch connector endpoints without mission-specific exceptions.
- [x] Move the level info action and Route header to the second table column.
- [x] Extend introspection and launcher/game integration coverage.
- [x] Run scoped quality, native tests, D1/D2 builds, Android build, metadata regression, and focused emulator tests.

## Boundaries

- Do not add Obsidian-specific segment, wall, or trigger exceptions.
- Do not alter GuideBot movement, path creation, simulation, firing, flares, or RNG.
- Preserve objective route order and metadata detail navigation behavior.

## Result

- Upper-left objective text and world-space markers now derive stable numbers from the canonical route, including filtered Remaining and Next views.
- Obsidian level 1 objective 3's activation and aim points are about 559 world units apart. Long guidance pairs over 200 units now render only the objective endpoint instead of a misleading full-map connector; route planning is unchanged.
- The metadata level info action and Route header are now the second column after Level, with detail notes aligned below Name.
- Added automap introspection for the first numbered objective text and suppressed long-guidance count. The focused Obsidian emulator test verifies numbering, connector suppression, nearby connector retention, and all 18 metadata routes.
- Scoped code quality, D1/D2 route snapshot and metadata tests, both Windows builds, Android unit tests, Android debug assembly, and the 35-step focused emulator test pass.
