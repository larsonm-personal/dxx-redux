# Route regression trends and backlog audit

- [x] Compare route status and problem fields between HEAD and the current worktree metadata
- [x] Exclude Castaway from transition trends and distinguish newly added records from true transitions
- [x] Summarize improvements, regressions, and materially changed partial or failed results
- [x] Inventory the current partial and failed route corpus, including duplicate archive records
- [x] Rank the remaining work by official mission impact, recurrence, and diagnostic severity
- [x] Bucket remaining routes by likely planner or metadata failure mechanism
- [x] Record the evidence and conclusions in this plan

## Findings

- The tracked JSON diff contains 1,715 levels in 132 files; an earlier recursive glob incorrectly omitted the 115 changed JSON files directly under `game_data/mission_files`
- Across the full diff, transitions are 1,522 `ok -> ok`, 85 `failed -> failed`, 66 `partial -> partial`, 33 `ok -> partial`, 6 `ok -> failed`, 2 `partial -> failed`, and 1 `partial -> ok`
- Excluding Castaway as originally requested, transitions are 1,518 `ok -> ok`, 85 `failed -> failed`, 65 `partial -> partial`, 29 `ok -> partial`, 6 `ok -> failed`, and 2 `partial -> failed`; there are no improvements
- Castaway separately has 4 `ok -> ok`, 4 `ok -> partial`, 1 `partial -> partial`, and 1 `partial -> ok`
- Castaway level 1 is one of the downgrades: `ok -> partial` with `route target unreachable`; levels 5 and 8 become partial for unresolved switches, level 9 becomes partial for target reachability, and level 2 improves from partial to ok
- All 1,522 retained `ok` routes gained a nonzero completion certificate, and 1,298 gained a nonzero required-key mask
- The new untracked D1 level pack has 17 one-level missions: 14 `ok` and 3 `partial`; it is new coverage rather than a transition
- The current corpus contains 1,732 route records: 1,537 `ok`, 102 `partial`, and 93 `failed`
- A conservative mission-name and level identity collapse leaves 173 logical bad routes: 88 partial and 85 failed, with 22 extra archive or case-variant records
- Key prerequisite trial failures are the largest class at 86 logical routes, followed by route-target reachability at 52 and unresolved switch activation at 24
- Highest mission-specific priorities are Counterstrike secret level -5; Vertigo secret levels -2 and -3; the repeated Lost Levels family; Disintegration; Ascent; Plutonian Shores; Nefarious Assault; and the same official secret-level geometry repeated in Trinity
- Training New Pilots contributes nine start-only target failures but is lower priority because it is a nonstandard training set with no Guide-Bots and no recorded exits
- Missing exits, one missing level file, and multiplayer-anarchy-only records should be triaged as content or corpus cases before planner work

## Corrective re-audit

- [x] Re-enumerate changed metadata using the whole `game_data/mission_files` directory rather than a recursive glob that omitted root-level files
- [x] Recompute all status transitions, reporting Castaway separately from the requested non-Castaway trends
- [x] Recheck diagnostic changes, completion certificates, and the new-file population
- [x] Replace the preliminary findings above with corrected totals and priorities
