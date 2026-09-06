# Live key pickup targets

1. Track the source key position used to compute a pickup waypoint
2. Refresh the approach when the key moves, retaining physical door frontiers and normal collision/pickup rules
3. Verify Obsidian 8 deterministically and compare the four core missions

## Validation

- Obsidian 8 completes identically across repeated runs: 4093 frames, exit at 68 rounded seconds
- Added test_guidebot_live_key_pickup.ps1 and updated only Obsidian level 8's checked-in simulation result
- All 88 core levels checked: FirstStrike 30/30, Counterstrike 28/30, Castaway 2/10, Obsidian 12/18; no previously successful level regressed
- Windows D2 build, 47 native CTests, and scoped code quality pass
- Pickup refresh preserves carrier-drop approach policy, uses the ordinary path follower, and does not enlarge pickup/contact radii or reset stall credit
- Evidence: android/temp/live_key_core_20260906 and android/temp/live_key_final_20260906
