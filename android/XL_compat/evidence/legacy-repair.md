# Legacy 17/36 repair centers

The audited 17/36 combined segment type 2 now selects XL's separate repair path.
Redux retains its original 25-unit rate. The 15/33 repair encoding, optional
ship/producer settings and multiplayer remain gated. No original mission is
qualified by these derived controls.

## Source and encoding

Pinned reference: arbruijn/d2x-xl 2576a98b5de310e3ac7c9777df2677d1627b8bae.
The repair oracle now extracts CSegment::Upgrade, both conversion tables and
segment constants, in addition to the existing Repair and actual player caller.
It runs Upgrade before every legacy sample and verifies type 2 remains repair,
type 0 stays dry, properties are cleared and both damage fields reset.

Additional source SHA-256 values:

- main/segment.cpp: eece1c8ba624bd42e6e77b9f2a3dd9a54489338139ee102245672b2b7353e60d
- include/segment.h: 4f43198ba394971128c81373940b5a58283cbbbc2851ca6a6e4187e78382a667

The derived fixture changes only room 0's function byte in the original
Observatory level. Its real open portal connects room 0 side 1 to dry room 6
side 3. Other original geometry, resources and no-damage rooms remain intact.
Original files are hash-checked before and after the native integration.

- Original obsrv-xl.hog: e89acaad09cefa0c09fc4b487c56b6077477cfe20169f364fe54f627040b01e7
- Derived oldrep.hog: aa9dc28e1497d702325a6cdbfe967d50e70d3996d4ba8ff688241361a8d72de2
- Derived oldrep.mn2: acc8e6e01e0d26049ebcda29407be8fa76bfa78c312347df4cbe660a016aec96

## Native evidence

All 1,440 source repair samples and 12 actual player frames match the extracted
source under MSVC and GCC with -Wall -Werror. They cover frame durations,
remaining capacity, cue deadlines, dry rooms and negative/zero/partial/full/
overfull shields. Energy remains unchanged. The shared native integration also
crosses the real portal both ways, checks the repair automap color, saves through
a stock mission, restores a pending cue, rejects eight corrupt trailers atomically
and verifies Redux's original rate. Schema 10 is unchanged.

The existing 23/40 matrix still passes all 5,760 samples and 42 player frames.
New admission/browser contracts accept 17/36 repair and retain the 15/33 gate.
Those contracts caught an initially stale blocked viewer status, which was fixed
before acceptance.

The pinned survey contains zero legacy repair candidates; the repeatable
[empty candidate report](legacy-repair-candidates.json) records that limited
finding. It does not establish that no such missions exist. Observatory is a
derived encoding fixture, not a newly qualified repair mission.

## Android and final validation

The registered repair runner's -Legacy17 mode owns the new 64-step JSON script.
All 64 checks pass on emulator-5554 in 87,953 ms, run
8aa20a2180544265a3e5a87819010947. They exercise healing/cap, actual mixer cues,
physical crossings, automap, disk/rewind and stock-to-XL restoration. Final APK
SHA-256: 28dd0fc3178215e8e7f96fb6e004a6d0c3387f745ef9767b25c7816741d2d39a.
An earlier run also passed before the viewer-status correction; the final run
uses the corrected build. Protected pilot/config/save file hashes match after
restoration; emulator-5556 was untouched.

All nine focused native regressions pass in 37.10 seconds, including the
existing modern repairs, original Observatory, doorways and keyed/closing doors.
All 12 private-policy tests pass, with 68 core tests required by the full private
runner; this does not claim all 68 were run here. Both catalogs pass: 406 top-level
entries, 462 support scripts, 99 standalone JSON tests and 293 standalone
PowerShell tests. Scoped mixed-language quality and explicit pinned PowerShell
analysis pass. Identical candidate regeneration preserves report bytes and mtime.

The existing modern Android mode also passes all 70 steps on the same APK
in 76,214 ms, run 5eaf63e7114442c0afb2ab15ab10b5a2. Both modes restore protected files.
