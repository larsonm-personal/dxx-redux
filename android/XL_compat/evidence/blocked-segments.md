# Modern XL blocked segments

Modern property bit 4 now blocks physical entry by players, robots and powerups.
It combines with water, lava and no-damage properties independently of segment
functions. Redux retains its ordinary path. Legacy blocked function 14, other
properties, custom resources and multiplayer remain independently gated

## Source decisions

Pinned reference: arbruijn/d2x-xl 2576a98b5de310e3ac7c9777df2677d1627b8bae

The executable oracle extracts the active CSegment::IsPassable definition.
reference_blocked.cmake verifies its segment.cpp, segment.h and wall.h hashes.
The destination property blocks only players, robots and powerups; leaving is
allowed. Weapons, debris, other actors and actorless queries keep ordinary wall
behavior. A blocked region is not a globally opaque wall

The source path builder and boss destination search deliberately pass NULL.
Those queries remain unchanged. MoveObjectToLegalSpot passes the robot, so its
recovery search must skip blocked destinations. Physical FVI and player
containment enforce entry rules; transparent-wall traversal cannot bypass them.
The source's explicit no-clip bypass remains available

Default-function blocked segments receive the source grey automap color before
door, secret and revealed overrides. The retained original function determines
eligibility, avoiding a false grey cue on translated generator/boost functions.
Multiplayer powerup redistribution also checks blocked regions in the reference;
that consumer remains outside the admitted singleplayer scope

Audited consumer SHA-256 values:

- physics/sphere_collision.cpp: 499873929ce6606f00b504e59624595e6bee57f9f4e225f53c85ebd0c1a80b60
- ai/aimove.cpp: 2aef988961c51464669d280d9073e2eceb98a8da45041a43d503b667005b5941
- ai/aipath.cpp: 2dbd0485df951ce4a4c70de1ce8e6fc87706394a831218ba3fd60420685dc8b1
- objects/boss.cpp: 553b98cd769c6105532b48ccd9d4b31b25f33d8d7bdda4c29a09434d80aa9042
- render/automap.cpp: 19b7128b48dadea220a085018e0fd3a2ff31d59fa04d60fce78d4bb1f5877be8
- main/dropobject.cpp: f1d186ac0fead42edb47d3f2b94daa449405cdc9aefa109862df0ca65b788907

## Native and lifecycle evidence

2,304 exact passability samples cover all 16 admitted property combinations,
nine actor/query types, eight ordinary wall-flag combinations and wall presence.
The extracted source comparison passes MSVC and Linux GCC with -Wall -Werror

Actual FVI and object physics check players, robots, pickups, weapons and debris
in both directions. Actorless rays pass; FQ_TRANSWALL cannot bypass an actor's
restriction. A real closed wall still stops projectiles. No-clip crosses, and
robot recovery with only a blocked neighbor refuses to relocate through it

Mixed blocked/lava robot production and blocked/water item production work.
Disk restoration through a stock mission restores retained source properties
and generated actor identity. Stock queries leave the player unchanged. The
property is immutable source data and needs no new save schema; development
schema 9 remains current. Automap/projectile counters are diagnostic only

All 14 focused regressions pass in 125.57 seconds: inventory, wall teleports,
object events, boosts, spawn points, blocked segments, lava, water actors,
generator assignment, boss transport/groups, equipment, modern properties/water.
All 12 private-asset policy tests pass; the core requires 63 feature tests.
Both catalogs pass with 402 top-level entries, 457 support scripts, 99 standalone
JSON tests and 289 standalone PowerShell tests

## Derived fixture and original candidates

blocked.hog derives from the existing water-actor fixture using hash-verified
Neptune geometry, scaled fourfold for normal actor clearance. Segments 40-55
contain the 16 property combinations, segment 60 is blocked, 61 is dry, and
generators 22/23 combine blocked with lava/water. Original bytes are unchanged

- Original neptunxl.hog: 4016618d24238803135dabf2b33daee2a2c34a670c3f016d94ae4639c8887303
- Derived blocked.hog: 42456e953153e828386ae8ee3cef80701dd49f527d5ecfd46392908fbbaafdd7
- Derived blocked.mn2: 0f1a73772a8f2c2f1b4eaf287fe0e566a899cedeb568b935eeb67b3c0d616754

[blocked-candidates.json](blocked-candidates.json) reinspects 29 surveyed levels
containing 6,037 blocked segments. Twenty-two decode and seven retain explicit
native rejections. Every record retains runtime_qualified=false. Decoding and
property support do not establish original-mission gameplay qualification

Repeating the audit preserves identical report bytes and modification time

One original uses an extended D2X HOG. The audit's bounded, read-only member
reader follows CLevelHeader::Read to verify its surveyed payload hash. The
engine still rejects that container; no level is silently extracted to evade
admission. Audited io/hogfile.cpp SHA-256:
87919a369a1e9501ae3520a85c382f8114dfd54939639026a3ea3bd88eae1339

## Android acceptance

The registered test_xl_blocked_runtime runner uses the pinned derived fixture
and backs up/restores pilot, saves and configuration. All 66 steps pass on
emulator-5554 in 117,951 ms, run ab0df91551b1412d991aba0ceda1018f. APK SHA-256:
81b42e2c3587f5662cb6e8e72d621db8dbb65ad7a12b0334e4e697fa99a4112e

Controlled poses followed by real thrust verify entry rejection and exit.
Normal laser firing records an actual projectile crossing. Automap invokes the
blocked cue, disk and rewind restore properties, mixed producers create actors,
and a stock transition clears XL behavior before disk restoration. Protected
file hashes match before and after; emulator-5556 was untouched. There was no
autonomous level navigation and no original-mission qualification
