# Vertigo 16 routing

Diagnose the tight-bend key approach in Vertigo 16, implement a general movement correction, and verify deterministic completion and the eight-mission regression set. Preserve existing work and leave TEW 9 and Counterstrike secret -5 deferred. Do not edit outstanding_bugs.md or put implementation in headers.

## Findings and retained change

- Vertigo 16, Fold Zandura, stalled before the blue key at the 16-to-22 opening. The opening is approximately five units high; the simulated ship has radius 310325 (about 4.735 units)
- Waypoint adjustment, corner detours and disabling outward path smoothing did not fix the passage. Long collision sweeps can miss contacts encountered by short physics steps. Detailed geometry also revealed tiny off-center fitting positions, so this is a navigation constraint rather than a proof that no player could ever cross
- The planner's existing clearance estimate described destination segment centers and discarded isolated bad centers. It therefore accepted the narrow shortcut and omitted the alternate entrance's switch prerequisite
- Shared topology construction now measures a portal's width relative to its edges and constrains narrow portals only when the full ship overlaps solid geometry at the portal center. It uses the existing FVI 15/20 edge-radius tolerance, preserving near-diameter passages rather than imposing an ideal sphere diameter
- The planner's transit masks and the live Guide-Bot certifier both honor the portal constraint. Vertigo 16 now shoots trigger 5 before collecting the blue key, then completes the original gold-key, trigger 18, trigger 8, reactor and exit sequence
- An initial full-diameter threshold regressed Plutonia 28. Matching the engine's existing edge tolerance restores it. No mission IDs, segment IDs, header implementation or experimental steering changes remain

## Validation

- `android/temp/core_eight_vertigo16_tolerance`: all 203 levels, no previously confirmed completion lost. New physical completions: Vertigo 14, Vertigo 16 and Plutonia 16. Plutonia 22 changes from timeout to an explicit planning failure and remains unresolved
- Vertigo 16 completes in 5581 frames; Plutonia 28 remains confirmed in 4160 frames
- Added a repeated integration test for Vertigo 16's switch-first route and Plutonia 28's preserved passage, plus native coverage distinguishing a bad-center estimate from an explicit narrow-portal constraint
- Final D1 and D2 host builds pass (`temp/vertigo16_final_build.log`). Existing unrelated D1 weapon ordering return-path warnings remain; no warnings were introduced in the changed code
- All 49 native tests pass (`temp/vertigo16_native_tests.log`), including the added live-frontier constraint check
- Scoped formatting and lint pass (`temp/vertigo16_quality.log`)
- `test_vertigo_level16_narrow_portal.ps1 -NoBuild` passes both repeated cases with byte-equivalent JSON values and matching regenerated metadata (`temp/vertigo16_integration.log`)
- Refreshed all eight targeted missions with the Windows host metadata runner (`temp/vertigo16_metadata.log`) and copied the final generator's normalized simulation JSONs
- Final corpus `android/temp/core_eight_vertigo16_verified`: **183/203 ok**, up from 179/203, with **zero lost passes**. Vertigo is now 20/24 and Plutonia 24/32. The shared change also incidentally resolves Counterstrike -5's metadata/simulation mismatch during the ordinary full-corpus refresh; no separate work was undertaken on that deferred level
- TEW 9 remains deferred. Plutonia 22 remains unresolved, now reporting a planning failure instead of timing out. Remaining Vertigo failures are 4, 6, secret -2 and secret -3
- All diagnostic exclusions, geometry-search experiments and steering modifications were removed. `android/outstanding_bugs.md` was not changed
