# Counterstrike level 20 grated-switch yellow-key routing

## Goal

Correct Counterstrike level 20 routing after the blue key so it recognizes the nearby shootable switch behind the transparent grate, lowers the forcefield, and routes to the yellow key instead of selecting an impossible switch.

## Plan

- [x] Inspect checked level 20 metadata, previous Guide-Bot regressions, and trigger/key topology.
- [x] Reproduce the post-blue live-state decision and identify whether the defect is canonical planning, shootability discovery, or live certification.
- [x] Compare the relevant classic Guide-Bot key and switch behavior.
- [x] Implement the smallest general shared-model correction without mission-specific data.
- [x] Add focused native and maintained level 20 regression coverage.
- [x] Regenerate or verify affected metadata and run scoped quality, tests, and host builds.

## Constraints

- Keep live Guide-Bot selection convergent from current world state.
- Reuse prepared topology, shot poses, and reachability; do not add recurring full planning or broad phone-side scans.
- Preserve ordinary opaque-wall blocking and legitimate completed-switch handling.

## Findings

- The permissive planner still discovers the intended route: blue key, shoot trigger 31 through the transparent grate, gold key, red key, boss, exit.
- A transparency-free comparison pass replaced that route with triggers 18, 12, and 35 because it checked only that the earlier blue key survived. It did not reject the alternate after it dropped the later gold key.
- The replacement also produced the incorrect `gold key not necessary` annotation, so the bad live guidance originated in precomputed canonical metadata rather than current-state certification.
- The strict comparison now has to preserve every key in the permissive route. This is one constant-size mask comparison during metadata analysis and adds no recurring Guide-Bot gameplay work.
- Route-cache generation 9 invalidates persisted generation 8 results once after upgrade, preventing the old canonical level 20 route from surviving on a phone.
- The generated and checked metadata now route trigger 31 from segment 253, gold key in segment 220, red key, boss, and exit. Trigger 18 and the incorrect unused-gold annotation are absent.
- Focused Counterstrike metadata, route snapshot, metadata scan, Guide-Bot certifier, native route-cache, and Android cache/coordinator unit tests pass. Scoped quality checks and complete D1/D2 Windows host builds also pass.
- The global route-corpus test still reports unrelated pre-existing mission metadata changes in the working tree. Its Counterstrike level 20 record matches the updated reviewed baseline.
