# Base Campaign Route Regression Audit

Date: 2026-07-11

## Scope

- Treat Descent and Descent II: Counterstrike as authoritative, professionally authored route corpora.
- Investigate new trigger dependency loops and other `ok` to non-`ok` regressions.
- Keep guidebot firing-position reachability aligned with metadata reachability.
- Check the complete checked-in mission corpus for collateral regressions.

## Findings

- Descent: every level reports `ok`.
- Counterstrike: every level except level 10 reports `ok`.
- Counterstrike level 10 remains `partial: gold key unreachable`. Its gold key carrier is sealed behind static closed walls with no trigger or reactor linkage, so this remains an explicit authored/static-analysis exception.
- The reported Counterstrike trigger loops were false dead ends. Nested dependency failures identified and excluded the outer trigger instead of the actual inner trigger that failed.
- A failed route attempt could retain partially fired triggers and movement state, poisoning subsequent alternatives.
- Failed keys were retried without a scoped exclusion, while successful key acquisition did not reopen the trigger alternatives made reachable by that key.
- A usable keyed door could be interpreted as requiring its attached opener trigger even after the player owned the key.
- Metadata firing-position sampling handled narrow face and edge lanes that guidebot did not sample.
- Corpus comparison against `HEAD`: 64 levels improve to `ok`; four older `ok` results become non-`ok`. Three of those old routes cross static closed geometry to reach keys (`d1mercen` level `merc06`, `Extra_Missions` `RETRIB10`, and `RETRIB11`). `Extra_Missions` `SUPER_S` reaches its reactor but has no modeled exit path. These are retained as reviewable exceptions instead of weakening closed-wall handling.

## Remediation

- [x] Use transparent-wall visibility rays for shootable targets through grates.
- [x] Track and propagate the actual nested failed trigger and key.
- [x] Roll failed trigger attempts back transactionally before trying alternatives.
- [x] Scope failed-key and failed-trigger exclusions to one target search.
- [x] Clear exclusions after acquiring a key changes reachability.
- [x] Prevent key acquisition from crossing a door that requires the key being acquired.
- [x] Prefer normal traversal of an owned, unlocked keyed door over an attached opener trigger.
- [x] Retry boss/reactor routing from a clean snapshot while acquiring reachable prerequisite keys.
- [x] Expand metadata and guidebot visibility sampling to face, vertex, and edge-biased positions.
- [x] Add a base-campaign route-status regression test.
- [x] Regenerate the complete mission metadata corpus.
- [x] Complete final native builds and tests.

## Verification

- D1 and D2 desktop builds pass.
- D1 and D2 `test_level_metadata_scan` pass.
- `test_base_mission_route_status.ps1` passes.
- `test_mission_metadata_travel_times.ps1` passes for 1,274 level records.
- Full metadata regeneration completed for 110 archives: 109 passed, one configured oversized archive skipped, zero failures.

## Expected Baseline

- `Descent.json`: all routes `ok`.
- `Counterstrike.json`: all routes `ok` except level 10, which remains the reviewed `gold key unreachable` exception.
