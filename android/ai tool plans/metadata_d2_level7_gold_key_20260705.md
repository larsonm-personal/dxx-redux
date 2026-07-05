# D2 Level 7 Gold Key Metadata Plan

## Goal
Find why Descent 2 level 7 metadata reports the gold key as unreachable even though the key is normally reachable.

## Tasks
- [x] Read project instructions and inspect the generated metadata for level 7.
- [x] Trace the gold-key reachability decision through the level metadata scanner.
- [x] Identify whether the problem is source data, object classification, wall traversal, or route staging.
- [x] Patch the scanner or generator if the cause is clear and narrow.
- [x] Run focused tests or regeneration checks.
- [x] Update this plan with findings and validation.

## Notes
- Treat this as a metadata/pathing scanner bug unless the level data itself says otherwise.
- Counterstrike level 7 is `coralbank quarry`; `travel_status` was already `ok`, but the newer documented `route_steps` chain stopped after two hidden doors with `route_problem: gold key unreachable`.
- The regression was in staged route key acquisition. It selected the shortest optimistic path to the key, then discarded the target if that path crossed a door locked by the same key. That missed longer valid routes around same-key doors.
- Fixed the route search so key acquisition can forbid doors locked by the key currently being acquired while still allowing other dependency blocks.
- Added a native scanner regression where a key has a longer open route but the shortest optimistic path crosses its own locked door.
- Validation: rebuilt and ran `test_level_metadata_scan` for both `buildd1` and `buildd2`; both passed.
