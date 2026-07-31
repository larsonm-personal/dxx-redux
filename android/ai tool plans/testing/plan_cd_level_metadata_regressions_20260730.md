# CD level metadata regressions

## Goal

Determine whether missions extracted from CD images participate in the
level-metadata analysis pipeline. If they do not, add a small declarative source
mechanism, generate normalized checked-in regression files, and verify the
result without committing it.

## Plan

- [x] Audit metadata source discovery and existing regression output ownership
- [x] Add declarative CD mission discovery with focused tests
- [x] Generate metadata regressions for eligible extracted CD missions
- [x] Run validation and code-quality checks
- [x] Review the final diff and record results

## Result

The existing pipeline covered built-in Counterstrike and mission archives but
did not discover CD-extracted missions. A checked-in source manifest now points
to the Vertigo `d2x.hog` and `d2x.mn2` files, and both the host regeneration
helper and canonical emulator wrapper process the configured CD sources.

Generated regression data includes all 23 Vertigo levels. Twenty-two routes
analyze as `ok`; secret level 2 is retained with `route_status: failed` and
`route_problem: route target unreachable`.

Validation completed:

- `android/tests/test_cd_level_metadata_sources.ps1`
- Two successful CD-only host metadata runs with byte-identical output
- `android/tests/test_mission_metadata_travel_times.ps1`
- Updated and revalidated `android/tests/fixtures/mission_route_baseline.json`
- `android/tests/test_regenerate_all_regression_data.ps1`
- Mission-metadata normalization check
- Focused `android/run-code-quality.ps1 -Fix`
- `git diff --check`
