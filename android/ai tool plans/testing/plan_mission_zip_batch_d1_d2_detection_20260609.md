# Mission ZIP Batch D1/D2 Detection - 2026-06-09

## Goal
Make the mission ZIP batch wrapper run each ZIP against the correct game instead of assuming D2 for every mission.

## Plan
- [x] Inspect the failed batch output and current reusable template.
- [x] Infer each ZIP's game from mission/level filename extensions before running it.
- [x] Parameterize the support template for game id and launch button text.
- [x] Include the inferred game in per-ZIP records and summaries.
- [x] Run scoped validation for script parsing and formatting.

## Notes
- The current template always analyzes with `"game": "d2"` and taps `Launch Descent 2`.
- D1 mission ZIPs can import successfully but will not appear in the D2 mission list, producing `SELECT_NON_BASE_MISSION: no non-base mission in listbox`.
- The first failed batch sample was mostly D1: `bratmaze.zip`, `cererian_1.3.zip`, `chromium.zip`, and `Colossus.ZIP`.
- `Countd2.zip` is D2 and may still represent a separate import/mission-list issue.
- Focused validation passed for `bratmaze.zip` as D1 and `descent_maximum_fixed.zip` as D2.
- `Colossus.ZIP` still crashes `analyze_level_metadata_all` when correctly routed as D1, so it is a separate analyzer issue.
