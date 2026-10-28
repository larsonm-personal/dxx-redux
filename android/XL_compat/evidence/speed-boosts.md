# Speed boosts and restricted boost areas

Singleplayer type-15 wall and admitted nonplayer object actions now have a
separate XL movement runtime. Redux remains the inactive alternate in the same
binary. This tranche covers the default D2 ship and XL boost-strength setting 10. Optional ship scaling, user-configurable boost strength, multiplayer and
camera actions are outside this qualification

## Authored behavior

Boost activation constructs direction from the source wall and first destination
side, or from the destination normal for an object action without a source wall.
Reciprocally connected sides use the source's special -1 boost state and speed 60. Other boosts use 60 + 40 * value, with authored values clamped as XL does.
Negative values reuse the shared previous speed across owners and level loads.
Zero or missing links release the boost and normalize velocity to 60

Per-owner state preserves velocity constraints, source/destination, checked
object signature and any remaining orientation steps. Set-orient performs the
initial tenth step and up to nine subsequent 40-Hz steps. No-thrust physics
restores the earned velocity; thrust is integrated with XL arithmetic and
clamped per axis. Positive player boosts suppress wiggle, external linear
forces, collision bumps and automap entry. The -1 case retains its distinct
source behavior. Object collisions preserve the boosted player's incoming
velocity, and wall retries use the cached authored destination

Legacy function 11 and modern function 9 identify restricted boost areas. They
remain in the retained XL source, translated to an ordinary stock segment enum.
A positively boosted player cannot leave such an area for an ordinary segment
unless that side has a speed-boost action. This is an actor-specific collision
rule, not a global wall change affecting lighting or robot paths

Wall teleports stop the boost and normalize speed before their orientation
transform, including when the actor was not boosted. Object teleports preserve
earned motion. Removal, signature reuse, player death and level/source changes
clear the relevant actor state. Pending object-master descendants retain their
original actor identity. Delayed work cannot boost a replacement object

Development trailer schema 7 replaces schema 6 directly. Disk and rewind store
the same bounded boost rows and previous-speed value. Full validation precedes
publication of any event, boost, boss or equipment state; source identity also
covers boost geometry and area functions. No development-schema migration is
provided

## Reference comparisons and intentional corrections

Reference: arbruijn/d2x-xl 2576a98b5de310e3ac7c9777df2677d1627b8bae.
The source oracle extracts the actual SetSpeedBoostVelocity, StopSpeedBoost,
DoSpeedBoost, ProcessDrag and orientation/math functions. It shares no Redux
math implementation. Hash-pinned CRLF source inputs include:

- main/trigger.cpp:
  74f1284260fb4a4bb7396bc45eeb83868a7f2dfa5e3fdf0bcb767b94f1b12852
- physics/physics.cpp:
  2ca8d5c939f31da9d4686637ed3e4305bf89e208a575f2f7f6ea2b4d2e6fab76
- maths/vecmat.cpp:
  4115afe844ad77a81e9f0de57ae1bbf577b06d5f49aed8b02d19002ff2021335
- include/vecmat.h:
  83d7f73f89a2f588a0b56a08fb9e293b747cc65a5ed22bad6771d64319bdca0f

All 4,740 activation, release, orientation and drag samples match the extracted
functions exactly. Cases include source-wall, reciprocal-side and no-parent
directions, six sides, raw zero/negative/out-of-range values, remaining turn
steps, five frame lengths and positive/negative/no thrust. The default-profile
adapter samples controls every frame, as this runtime does. The existing 252
wall-teleport samples now include the previously missing speed-60 normalization

The port deliberately corrects unsafe reference state handling:

- Orientation target, steps and deterministic fractional phase belong to each
  actor. The reference has one global turn target and updates the local player
  even when another actor activated the action
- Direct object actions explicitly have no parent wall; the reference attempts
  an array-index calculation against the geometry-trigger array
- Every boost initializes its destination. The reference leaves it stale for
  side-normal and reciprocal-side cases; its backward-wall retry also uses a
  stale velocity copy. The port recomputes the retry from the current destination
- Wall teleport and exit cancel the superseded turn. Explicit release still
  preserves an in-progress authored turn, matching the source's release path
- Save validation and checked object signatures prevent partial state and
  recycled-slot activation

These corrections are not claimed as whole-engine parity. The active physics
consumers were audited separately from DoPhysicsSimOld, which is compiled only
under DBG_PHYSICS

## Integration acceptance

The derived boost mission uses original Neptune geometry and authored wall,
object-autoplay and delayed master actions. Native controls verify real wall
crossings, restricted exits and release; physical no-thrust player/robot motion;
an actual FVI player/robot collision; independent orientation; stationary-owner
turning; teleport distinctions; removal/reuse; death/exit; and delayed actor
dispatch. Disk restoration through Redux preserves motion and fractional phase.
Fifteen recomputed-checksum malformed trailers reject without partially changing
an earlier valid pending event

Inventory contracts cover serialized boost-area functions in 15/33, 17/36 and
23/40 and object/wall action admission. Camera type 16 remains rejected. Native
stock controls prove inactive XL hooks neither constrain drag nor alter velocity

All 26 focused feature regressions pass in 133.95 seconds, including events,
boss/equipment state, resource actions, water/property controls and original
Neptune/Observatory controls. Linux GCC also passes all 4,740 source samples
with -Wall -Werror. Reinspection of the seven spawn-bot and eleven equipment
candidate rows leaves both canonical reports unchanged; none is promoted

All 49 final-build Android steps pass in 207034 ms, run
3f0bc109dc4a42c09ccfbc9345763efc, using the registered
test_xl_speed_boosts_runtime runner. It uses controlled poses and actions, memory
and disk checkpoint byte comparisons, repeated restores, a stock mission
transition and delayed master completion. It restores staged missions and user
state after the run; no autonomous navigation is involved. Both checkpoint paths
compare complete captured bytes and perform three memory restores; the disk
path also verifies published bytes and restores from disk. Controlled activation
is renewed before the second capture so prior wall contact cannot turn an active
boost assertion into a test of an already released boost

Final APK SHA256:
427795c115302a2a4cca632ad423be9487b6bcaf89d34f9da6280978fb8774c5.
Protected-file hashes match after restoration. The final formatted native build
passes inventory, teleport and boost controls in 23.41 seconds. Scoped quality,
all 12 private-policy tests and both automation catalogs pass (398 top-level
entries, 453 support scripts). The private receipt requires xl_speed_boosts

Fixture identities:

- Original Neptune HOG:
  4016618d24238803135dabf2b33daee2a2c34a670c3f016d94ae4639c8887303
- Derived boost.hog:
  b2b460804679ac074933ed2a343c50d8024b436fa76bf790e90ebf382ea34027
- Derived LF boost.mn2:
  b61c3659a8ce2076b58aaa7d995cbbb8cd045aed0dc8302a93461c09ed3ef7bb

The survey's 281 boost actions motivated priority; they are not 281 verified
actions or newly playable missions. Original mission qualification still needs
its complete requirements, resources and gameplay-bearing interactions checked
