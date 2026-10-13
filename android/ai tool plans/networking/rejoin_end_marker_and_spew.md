# Rejoin end marker and migrated spew recovery

- [x] Compare the S25 and S21 exported build 25010 Network logs
- [x] Retain an early object end marker until the expected objects arrive
- [x] Add a deterministic migration regression for end-marker overtaking
- [x] Reproduce graceful host departure and inspect spew accounting across migration
- [x] Fix confirmed departure fault, retaining Network-category diagnostics
- [x] Format, build both engines, and run relevant integration tests

## Evidence

S25 log `debuglog_20261012_181053.txt` sends the last 13 objects with hash
30734da5 before end-marker hash 9b936855. S21 log
`debuglog_20261012_181055.txt` receives the end marker at 18:12:47.926 and
the object packet at 18:12:47.930. All 181 objects arrive, but the premature
marker resets the parser's count from 168 to zero. Subsequent SYNC packets
are safely deferred forever for that attempt. The wire/network location of
the ordering change is not established by these native socket traces.

Spew accounting needs a graceful-departure regression: prior migration tests
force-stopped the host, which bypasses its normal inventory drop sequence.

The user's exit path is the touch overlay's Exit action, represented by
`enter_launcher` in automation. An additional Abort Game defect was found while
testing: it clears Current_level_num before multi_leave_game sends the drop and
ledger. Both messages arrive with level/visit zero and are rejected by the
current-mine fence. Move that cleanup until after departure messages are sent.

Recovery remains player-owned across levels and save/resume. No current-level
filter is added to inventory reclamation. The current disk capacity is 16
remembered player records. The current mine identity is needed when recording
new drops, not when reclaiming already-recorded gear from other levels.

The first valid Abort Game run retained five ledger rows and cashed in all
remaining missile spew. One missile was picked up by the surviving player before
rejoin, so the regression compares combined inventory on both peers rather than
requiring the departing player's entire original inventory to remain unclaimed.

## Outcome and validation

The touch Exit path also passed with five missiles reclaimed and one already
collected by the surviving player. No recovery-tagged objects remained on either
peer, and combined missile inventory was conserved. This does not reproduce or
resolve the user's stranded six missiles. Keep that issue open for another
device reproduction with the added Network diagnostics, which include ledger
receipt, identity matching, object bindings, migration, pickups, and reclamation.

- D1-in-D2 level 8: `test_lan.ps1 -SkipBuild -HostMigration -Game d2 -MissionFile descent -InitialLevel 8` passed
- Native D1 level 8: `test_lan.ps1 -SkipBuild -HostMigration -Game d1 -InitialLevel 8` passed
- Both runs exercised early SYNC and an early object end marker, graceful first
  departure, both migration directions, object/allocator agreement, inventory
  conservation, and bidirectional gameplay packets
- x86_64 debug APK and ARM64 CMake targets for both engines built successfully
- Scoped code-quality checks, both automation catalog validators, and
  `git diff --check` passed
