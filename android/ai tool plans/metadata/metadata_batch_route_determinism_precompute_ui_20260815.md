# Metadata batch failures, route determinism, and precompute progress UI

Date: 2026-08-15
Status: complete

## Requests

- Diagnose and fix five failed mission metadata archives from the full batch
- Repair the new partial route in `KCXF2RMv11.json`
- Determine and reduce avoidable route-coordinate drift in `Descent.json`
- Add an Advanced-tab view for live background route-precompute progress
- Base overall progress on every level across base D1, base D2, and mods
- Persist a scrollable/exportable per-level, per-mission, and priority-switch log

## Plan

- [x] Inspect batch artifacts and classify all five archive failures
- [x] Reproduce and fix import, worker lifecycle, crash, and partial-load causes
- [x] Compare KCXF2 and Descent routes against reviewed baselines and make route
  output deterministic without materially increasing analysis time
- [x] Audit the background coordinator's discovery, scheduling, and persisted state
- [x] Implement a persistent progress/event store with total-level accounting
- [x] Add an Advanced-tab progress, history, and export UI
- [x] Add focused unit and integration regressions
- [x] Rerun affected archives, route corpus checks, Android build/tests, and quality

## Constraints

- Do not hard-code mission-specific behavior
- Preserve unrelated working-tree changes and generated regression evidence
- Keep background analysis bounded and lower priority than interactive work
- Make persisted diagnostics useful across app restarts
