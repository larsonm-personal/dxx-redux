# Energy center count metadata

## Goal
- Add level-analysis support for an `energy center count` metadata value
- Study D1 and D2 base levels to decide whether fuel/energy center segments already form connected labels or need distance-based grouping

## Plan
- [x] Inspect existing secret-area scanner, headless dump, and baseline fixture flow
- [x] Dump or derive energy-center segment layout for D1 and D2 base levels
- [x] Choose a grouping rule if separate chunks represent one practical energy center
- [x] Implement the metadata count in shared Android analysis code
- [x] Update focused fixtures/tests and run validation

## Refactor plan
- [x] Move energy-center metadata analysis out of `secret_area_scan.c`
- [x] Add a shared `level_metadata_scan` module and state accessor
- [x] Keep secret-area state focused on generated secrets
- [x] Re-run focused builds, formatter, and baseline regression

## Matcen count plan
- [x] Add matcen counting to `level_metadata_scan`
- [x] Serialize matcen count through headless dumps and introspection
- [x] Study D1/D2 base levels for connected robotmaker segments
- [x] Re-run focused builds, formatter, and baseline regression

## Matcen count notes
- Matcens are independent per `SEGMENT_IS_ROBOTMAKER` segment. `fuelcen_activate()` creates a `matcen_num`/`RobotCenters` entry per robotmaker segment, and matcen triggers activate linked segments individually with `trigger_matcen()`
- Base D1: 79 robotmaker segments, 78 connected components. Level 5 has one connected pair, but it still counts as two matcens
- Base D2: 74 robotmaker segments, 74 connected components
- `matcen_count` uses robotmaker segment count; `matcen_raw_count` is retained as connected-component audit data

## Notes
- Start with the existing segment-special data available to the scanner
- Base D1: 133 fuel-center segments, 58 raw connected chunks, 58 grouped centers at 20 `F1_0`
- Base D2: 157 fuel-center segments, 68 raw connected chunks, 68 grouped centers at 20 `F1_0`
- Threshold sweep result:
  - 30 `F1_0`: merges D2 levels 11 and 15
  - 40 `F1_0`: merges D2 levels 10, 11, and 15, with no D1 merges
  - 50 `F1_0`: also merges D2 levels 12 and 16
  - 60 `F1_0`: starts merging D1 level 10
- Use 40 `F1_0` as the default starting distance; override with `DXX_ENERGY_CENTER_GROUP_DISTANCE` for tuning
