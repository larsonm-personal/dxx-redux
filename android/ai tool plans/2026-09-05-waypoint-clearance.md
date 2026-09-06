# Waypoint clearance

1. Add shared full-radius waypoint approach validation and local entrance routing
2. Apply it to ordinary GuideBot path steering and simulation final approaches
3. Verify the key alcove and compare all four primary missions

## Findings and implementation

- Descent conversion level 8's generated key-contact point intersects the alcove ceiling; the segment center has clearance
- Added shared full-effective-radius sweeps and deterministic waypoint validation in the D2 GuideBot code, used by D1-in-D2 and the sim as well
- Invalid point repair searches up to 16 positions toward the segment center, requiring an occupiable point and a clear interior approach
- Door and blastable-wall contact targets remain governed by their normal interaction handling
- The sim validates its final target and preserves the existing terminal path point when the replacement leg is blocked, using direct final steering only when the sweep is clear
- Ordinary proximity waypoints are repaired when their approach is actually blocked at geometry, with a clear leg from the actor to the replacement
- The quarter-unit collision proximity test only determines when to attempt repair; it does not reduce the collision radius or allow passage through a wall
- Blanket waypoint-skip gating and eager adjustment of future waypoints both caused regressions in testing and were removed
- This is a bounded local repair, not an exhaustive solver for every possible nonconvex segment or multi-segment detour

## Validation

- Both Descent conversion level 8 and original D1-in-D2 level 8 complete in the new integration test
- The 88-level primary-mission v8 run preserves every baseline completion status: Counterstrike 28/30, FirstStrike 22/30, Castaway 2/10, Obsidian 11/18
- D2 build, scoped quality checks, and all 47 native CTests passed
- The focused integration test covers levels 8 and 12 in both First Strike versions, all completing identically across repeated runs
- The repeated 88-level bundle had one intermittent startup access violation in FirstStrike level 4 before its first route goal; its second run completed, and four subsequent serial reruns all matched and completed
- Do not describe the repeated bundle as entirely clean: that startup failure remains unresolved, although the single full v8 run was clean and preserved every baseline status
- Final evidence: android/temp/waypoint_primary_verified_20260905, android/temp/waypoint_d1_l4_verify_20260905, and android/temp/waypoint_test_final_extended.log
- Checked-in mission JSONs are not rewritten by these test runs
