# Rejoin SYNC completion guard

- [x] Read build 25000 crash and exported Network log for the former host rejoining D1-in-D2 level 8
- [x] Keep the join open until the object end marker verifies the complete transfer and rebuilds the allocator
- [x] Prevent periodic SYNC resends during an active object transfer
- [x] Extend host-migration integration coverage with an intentionally early SYNC, object parity, allocator consistency, and continued gameplay traffic
- [x] Format, build both engines, run integration coverage, and record results

## Evidence

The client received 168 objects with `num_objects=1`. At 17:06:00.591 it
accepted wrapped SYNC for attempt 596516649 and entered PLAYING. The last
three objects and the end marker (expected count 171) arrived at 17:06:00.593
and were rejected solely because the join was no longer active. The allocator
then returned an occupied slot, causing `obj_create` to assert at object.c:1582.
This capture demonstrates premature admission, without requiring packet loss.

The migrated host had Network logging disabled. The client capture cannot
identify which host send path produced the early SYNC. Keep this distinction
explicit; the receiver must enforce completion regardless of the send path.
Diagnostics remain in the exported Network category.

## Validation

- Scoped code quality checks passed
- Debug APK built for x86-64, including both engines
- CMake ARM64 RelWithDebInfo build passed for both engines; existing warnings only
- Automation catalog and master test catalog checks passed
- `test_lan.ps1 -SkipBuild -HostMigration -Game d2 -MissionFile descent -InitialLevel 8`: passed, including both host swaps, rejoining, and Guide-Bot authority
- `test_lan.ps1 -SkipBuild -HostMigration -Game d1 -InitialLevel 8`: passed, including both host swaps and rejoining
- Local run output: `temp/rejoin-lan-d1ind2.log`, `temp/rejoin-lan-d1.log`, `temp/rejoin-debug-build.log`, `temp/rejoin-arm64-build.log`

The integration injects a valid wrapped SYNC after the first object packet,
asserts that the receiver deferred it, and checks object ownership parity,
allocator counts, and bidirectional gameplay traffic after rejoining. It then
reverses the host roles and performs an ordinary rejoin as well.

Both runs confirmed at least one deferred early SYNC, matching allocator/live
object counts, matching synchronized object ownership, and sustained PDATA in
both directions after each rejoin. Runs used two emulators; the user's phone
reproduction has not yet been repeated with this change.
