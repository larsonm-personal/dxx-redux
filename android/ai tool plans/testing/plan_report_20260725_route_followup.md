# Report 20260725 route follow-up

## Goal

Investigate and disposition `test_secret_area_baseline` and `test_obsidian_level1_objective_markers` after the recent carrier-continuation route-planner change.

## Plan

- [x] Select two related failures and record the investigation scope
- [x] Review the secret-area structural changes against the source fix and regeneration workflow
- [x] Reproduce and inspect the Obsidian live route state without relying on a longer timeout
- [x] Fix stale expectations or code only when the intended semantics are established
- [x] Validate through normal runner ordering and record results

## Findings

- The secret-area baseline had one structural change: Counterstrike level 10 now contains the complete reviewed route after the carrier-continuation fix. The added gold and red carriers, triggers 18 and 25, reactor, and exit match the focused route-status regression
- The Obsidian failure was not a timeout or random stale device state. Regenerated level 10 metadata inserted newly solvable shoot switch trigger 8 before unresolved trigger 0
- The combined test requested Guide-Bot guidance after granting all keys, then incorrectly expected the second pending objective
- The test now asserts trigger 8 and its objective location, completes it through the existing `fire_trigger` adapter, and retains every original unresolved-trigger assertion

## Validation

- Secret-area baseline regenerated through its canonical producer and matched on a clean rerun
- Full Obsidian combined script ran from launcher step 1 through all 193 steps and passed. The repaired sequence observed trigger 8, fired it, then observed unresolved trigger 0 with the original segment, wall, and guidance assertions
- Automation catalog validation passed with 35 standalone JSON tests and 18 support scripts
- Scoped code-quality validation passed
- Normal suite runner rebuilt the APK, recovered from a launcher-preflight emulator restart, reprovisioned the device, and passed `test_secret_area_baseline`
- Suite report: `temp/test_reports/route_followup_20260725/report_20260725_171846.md`
