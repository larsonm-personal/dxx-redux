# Modern XL lava volumes and contact physics

Modern 23/40 lava property bit 2 uses an XL-owned contact path. Redux remains the
alternate in the shared binary. Independent unsupported properties, signed
segment damage, resources, legacy lava encodings and multiplayer remain gated.
Derived controls qualify behavior; they do not qualify original missions

## Source and profile

Pinned reference: arbruijn/d2x-xl 2576a98b5de310e3ac7c9777df2677d1627b8bae

The executable oracle extracts CSegment::Physics, CSide::Physics,
CObject::ApplyWallPhysics, CheckSegmentPhysics, CheckWallPhysics, ScrapeOnWall,
ApplyDamageToPlayer, Bump and ApplyForce, plus CFixVector::Random. Motion uses
extracted CPhysSimData::Setup, CObject::ProcessDrag and UpdateOffset with the
previously pinned independent XL vector math. Source hashes are checked by
reference_lava.cmake and the preceding motion oracle definitions

The profile enables normal fluid physics and normal drag/speed, with optional
addon scrape sounds, slow motion and missile acceleration disabled. RNG comparison
uses identical bounded input transcripts; it does not assert that Redux and every
XL platform have identical PRNG implementations. Normal source arithmetic and the
release float force path are compared without linking production fluid helpers

- Water wins motion precedence: water uses 75 percent, lava alone 66 percent
- Lava wins volume damage precedence: texture 404 damage is halved for contact,
  then halved again for free-volume damage. Trainee halves the resulting damage
- Solid sides and active illusion surfaces can supply texture contact physics.
  Open portals, nonillusion portal walls and disabled illusions do not supply it
- Wall texture physics takes precedence over volume physics. Protected contact
  suppresses damage and idle turbulence. Protected free lava still has the
  source flash/turbulence while ApplyDamageToPlayer suppresses health loss
- The actual CheckWallPhysics caller is player-only. Nonplayer actors retain
  fluid movement without acquiring unreachable robot-destruction behavior
- Scrape impulses occur only after XL's randomized interval expires, including
  its backward-clock behavior. The interval consumes RNG even without an impulse
- Lava chooses the lavafall loop; thrusting volume water and water surfaces use
  their distinct loops. Registry-aware cleanup repairs stale loop markers after
  restore and removes old loops on dry transitions

The pinned non-volumetric ColoredSegmentColor path returns no colored boundary
for lava-only fog type 2. Mixed water/lava retains the water-colored boundary.
The port does not invent an orange surface from an unused color-table entry.
Optional volumetric fog and broader visual parity remain separate work

## Persistence and stock isolation

The XL scrape clock is separate from Redux's global scrape clock. Development
trailer schema 9 directly replaces schema 8 and stores elapsed time, rebased onto
the restored game clock. All loaded XL sources require this runtime trailer,
including levels with only texture contacts. There is no pre-release migration

Clock validation occurs before any cross-feature state is published. Backward
clock values are retained when representable; malformed types and arithmetic
overflow are rejected. Saturating elapsed subtraction avoids overflow after an
extremely old valid timestamp. Mission changes clear the XL clock; stock fluid
hooks select Redux without changing its object or physics state

## Native evidence

10,240 contact samples compare exact type, shield damage, palette red, rotation,
velocity impulse, elapsed clock and RNG consumption against the extracted source.
The matrix includes all supported property combinations, free and overlapping
poses, solid/portal/illusion/disabled/closed surfaces, five frame durations,
trainee/normal difficulty, thrust, invulnerability, zero/fractional/normal mass,
nonphysics objects, automap force suppression and elapsed interval boundaries.
A separate native control preserves active boost velocity when the scrape impulse
interval expires

The motion oracle covers 384 unaccelerated samples plus 288 nonzero drag/thrust
samples across lava, water, mixed, protected and dry rooms. Real engine physics
checks both portal directions and the following frame's medium. Ordinary
nonplayer frames do not inflict lava damage or consume player turbulence RNG

Actual object frames inflict nonlethal/lethal lava damage, preserve protection,
and enter normal death/respawn. Disk restoration through a stock mission restores
source properties and the pending scrape interval. Eight malformed trailer
controls reject without publishing the clock; cross-feature corruption is included.
Stock dispatch leaves its player object unchanged

Windows MSVC source comparisons pass. The contact and motion cases also pass Linux
GCC with -Wall -Werror. Twenty-four distinct focused regressions pass across two
runs (149.23 and 43.29 seconds, with two justified repeated tests after the motion
oracle extension). Both automation catalogs pass: 401 top-level entries, 456
support scripts, 99 standalone JSON tests and 288 standalone PowerShell tests.
All 12 private-asset policy tests pass; 62 feature tests are now required

## Derived assets and original candidates

lava.hog derives from the water-actor fixture using hash-verified original Neptune
geometry. Segments 60-67 cover lava, dry, mixed, protected and water controls;
robot center 22 and item center 23 are in lava. Geometry and actor positions are
scaled fourfold for normal actor clearance; original bytes remain unchanged

- Original neptunxl.hog: 4016618d24238803135dabf2b33daee2a2c34a670c3f016d94ae4639c8887303
- Derived lava.hog: 032a983541d4eb85e06069a34b047fbd73e443f752121372092469da94c7a9d3
- Derived lava.mn2: ed1d6d53e12983c69a8196419a5d8ab1ca6cd54784148fd4e054d26cdce731ad

[lava-candidates.json](lava-candidates.json) freshly reinspects all 13 surveyed
modern lava levels, containing 279 lava segments. Eleven decode; the other two
retain explicit native rejections. Every record retains runtime_qualified=false.
Hash matching, native decoding and feature counts do not establish gameplay
support. Repeating the audit preserves identical report bytes and modification time

## Android acceptance

The registered test_xl_lava_runtime runner stages only the pinned derived assets,
backs up the target's pilot/config/saves and checks restored file hashes. Its owned
script checks protected and unprotected lava, mixed water/lava, real thrust,
linked and playing mixer loops, disk/rewind, mixed producers and a stock transition.
All 58 steps pass on emulator-5554 in 78,491 ms, run
9e740805b68843e99e4eee79abf156e8. APK SHA-256:
1b26b316d6baf60b577478ca41f9c536f301acd0fc2d6fe8c570fa4e4414a8f2.
Protected before/after file hashes are identical; emulator-5556 was untouched

The first run passed the fluid/lifecycle checks but its producer assertion timed
out because the script left robots suspended. The final script enables robots
for generation and then suspends them for stable inspection. The failed run also
restored protected files. Launcher recovery was infrastructure work, not gameplay
navigation or a feature pass

## Packed-vector portability follow-up

The random direction helper now fills an aligned local fix array before constructing
the packed vms_vector. This removes pointers to potentially unaligned packed members
without changing axis selection, arithmetic, normalization or RNG order/count.
The Android x86_64 build and an ARM64 compilation of fluids.cpp pass without the
packed-member warnings. ARM64 execution has not been tested

The four existing native lava/water regressions pass in 27.03 seconds, including
10,240 extracted-source contact comparisons and 672 motion comparisons. The Android
lava runner passes all 58 steps in 101,090 ms, run
fad4f2a594a64f5d8ab96e64df7cba58. APK SHA-256:
b9a1c98b148ccd61148b42e8770ff94b6dff7cbcc63d53adaabd21bd24319736.
Protected before/after hashes match; emulator-5556 was untouched. These are derived
controls and do not promote any original mission to runtime-qualified
