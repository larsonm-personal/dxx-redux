# Report 20260725 route failure investigation

## Goal

Investigate the route-analysis regression failures and the related Obsidian runtime assertion without treating isolated passes or longer timeouts as fixes.

## Plan

- [x] Read repository instructions and identify the related failure cluster
- [x] Trace the changed route output to recent source and baseline changes
- [x] Determine whether the Obsidian runtime failure is a stale expectation, leaked state, or engine defect
- [x] Make the smallest justified fix, if one is established
- [x] Run focused diagnostics plus an order-sensitive or broader validation that exercises setup and cleanup
- [x] Record findings and validation results here

## Findings

- `test_base_mission_route_status` and `test_mission_route_corpus` were deterministic stale expectations after the reviewed carrier-continuation fix in `344392b9` and metadata regeneration in `57b5b0b5`
- Counterstrike level 10 intentionally changed from a two-step partial route to an eight-step complete route
- The status test now requires every Counterstrike route to be `ok` and directly checks the complete level 10 carrier, switch, reactor, and exit sequence
- The corpus baseline was regenerated and its six changed records match the six metadata files changed by the reviewed regeneration
- The related Obsidian runtime failure is not supported as a random flake: the same scenario passed in the July 23 and July 24 reports, then changed from `unresolved_trigger` to `shoot_switch` after the July 25 route-planner change. It needs a separate semantic review rather than a timeout increase

## Validation

- Scoped code-quality pass succeeded
- Direct status and corpus test runs succeeded
- Normal suite-runner validation with `-Filter 'test_*mission_route*'` rebuilt the debug APK, performed suite preflight, and passed both selected tests in their normal sorted order
- Suite report: `temp/test_reports/route_investigation_20260725/report_20260725_163001.md`
