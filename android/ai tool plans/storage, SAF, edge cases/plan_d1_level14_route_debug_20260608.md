# D1 level 14 route debug

## Goal
- Fix the false `gold key unreachable` result for D1 level 14.
- Prefer a graph/pathing correction over assuming key reachability.

## Plan
- [x] Inspect scanner graph traversal and D1 wall/key adapter values around locked doors.
- [x] Reproduce the level 14 route failure with focused diagnostics.
- [x] Remove same-key detour fallback; it is still a special case, not the underlying route fix.
- [x] Pin the ordinary route sequence: start/no keys to blue key, blue key to blue door, blue door to yellow/gold key.
- [x] Patch the route input so that ordinary route is recognized without level-specific or key-specific special casing.
- [x] Refresh metadata fixture and run focused verification.

## Notes
- Walkthrough/playthrough evidence says D1 level 14 is ordinary progression: blue key, yellow key, red key, reactor, exit.
- In code/UI naming this scanner currently calls the yellow key `gold`.
- The failure was not a missing key object. D1 level 14 has blue, yellow/gold, and red key targets in the object list.
- The same-key detour fallback was rejected because it is still a special case. It has been removed.
- The focused trace pinned the bug to the route start, not to D1 level 14 geometry. Headless metadata loading used `load_level()` without the gameplay init path that fills `Player_init[]`, so the scanner started from stale/default segment 0.
- Reading the loaded player start object directly gives D1 level 14 start segment 187. From there, no-key start to the blue key segment 621 is an exact path with no locked door, and blue-key route from the blue key to the yellow/gold key segment 622 is also exact.
- Refreshed `secret_area_base_game_baseline.json` after the corrected start source. The compare pass reports zero non-ok D1/D2 travel statuses and matches the refreshed fixture.
