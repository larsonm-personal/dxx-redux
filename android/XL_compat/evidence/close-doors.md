# Authored XL close-door actions

The later [door lifecycle work](door-lifecycle.md) supersedes this tranche's
89-entry spare-tail restriction and allocation adapter. The pinned XL stack
has 90 entries with growth disabled, not dynamically expandable storage. The
earlier results below remain evidence for their stated transition scope.

XL authored close actions now have their own implementation. They can close an
automatic door during its waiting period, reusing the active entry and applying
the source's timer conversion. Redux retains its existing alternate, which
ignores a close action while waiting. Force means overriding the wait, not
ignoring an obstructing actor

## Pinned semantics

Reference: arbruijn/d2x-xl 2576a98b5de310e3ac7c9777df2677d1627b8bae

- main/wall.cpp SHA-256: 05eaa2cab8dd7f5661afd210e0edc8dea4fcd2bb2075c00d3012bec395ab0c42
- main/segment.cpp SHA-256: eece1c8ba624bd42e6e77b9f2a3dd9a54489338139ee102245672b2b7353e60d

The independent oracle extracts AddActiveDoor, FindActiveDoor, CWall::CloseDoor
and CSegment::CloseDoor. Its allocation adapter supplies controlled success or
failure; obstruction is an explicit input, with real two-sided geometry checked
separately. No production transition helper is linked into the oracle

3,072 exact comparisons cover eight states, missing/front/back/second-part active
entries, six earned times, four animation lengths, obstruction and allocation
exhaustion. The original full-array adapter used the engine's 90-entry storage
with Redux's spare tail entry, simulating failure at 89. It did not establish
the actual XL stack boundary; the later lifecycle work corrects that adapter.
MSVC and GCC -Wall -Werror pass. Only
misleading indentation inside the unchanged upstream extract is suppressed;
adapter warnings remain enabled

Opening and waiting entries reuse max(0, animation length - earned time). Open
doors allocate a new entry. Connected wall state, active front/back indices and
the source motion cue are retained. Existing wall/active-door persistence is
authoritative; development schema 10 remains unchanged

## Intentional corrections and boundaries

The source DoorIsBlocked comment describes both segments, but the loop walks
only the front segment's actors. The runtime retains the existing two-sided
geometry check. Twelve real actor controls cover players, robots, pickups,
markers, weapons and fireballs on both sides. The first four block closure;
weapons and fireballs do not. Blocked actions preserve earned waiting state

Invalid coordinates/clips are no-ops. A damaged timer whose conversion would
overflow is rejected before mutation. Exhaustion preserves wall/active state and
keeps the spare entry required by the ordinary animation compactor. Unsupported
effect objects, ignore-marker flags, expanded active-door capacity and optional
slow-motion settings are not newly qualified

## Runtime evidence

The native fixture opens an automatic door, waits 1.5 seconds (below its normal
five-second deadline), then invokes the authored close action. It verifies
connected state and slot reuse, actual closing animation and physical/visibility
rays. Waiting state survives a disk save through stock. Closing state and its
earned timer survive a second save/restore after reversing the live door

All 12 focused regressions pass in 61.30 seconds, covering inventory, teleports,
wall chains/scheduling, object events and delayed chains, boosts, keyed walls,
doorways, boss transport and trigger flags. All 12 private-policy checks pass;
67 core feature tests are required. The final native control, including atomic
overflow rejection, passes in 2.97 seconds

All 80 Android checks pass in 74,864 ms on emulator-5554, run
f589da993a9d4c28920c994492e634d1. They verify actual player obstruction on both
sides, the unchanged motion-cue count while obstructed, an actual mixer attempt
when closure starts, earned waiting restoration through disk/rewind/stock, and
physical passage through the restored waiting door. Protected file hashes match
before and after; emulator-5556 was untouched. APK SHA-256:
78319527dfc099e985e13e34b584c35ddb3663e377a008599bf74c7c87604fe7

Both catalogs pass: 406 top-level entries, 461 support scripts, 99 standalone
JSON and 293 standalone PowerShell tests. Scoped mixed-language quality and
pinned PowerShell formatting/lint pass

## Assets and original candidates

The close fixture changes the preceding doorway fixture's first connected door
pair to automatic and retains its authored open/close actions. Original mission
bytes remain unchanged

- Original Neptune HOG: 4016618d24238803135dabf2b33daee2a2c34a670c3f016d94ae4639c8887303
- Derived close.hog: 3c8a1f6241a9f06df3931aa088606ad16ec6e7d4df0a333669b74046e4e62e9b
- Derived close.mn2: 6194d572c9269c95fd2d5f2586f65f0d9e3a4516dd790fbe933a330eda148245

[close-door-candidates.json](close-door-candidates.json) freshly inspects seven
original levels with sixteen authored close-door actions. Three decode and four
retain explicit rejections. The decoded targets include four automatic doors.
Each original remains runtime_qualified=false with its independent requirements;
these controls do not qualify normal mission completion or all wall behavior
Identical audit regeneration preserves report bytes and modification time
