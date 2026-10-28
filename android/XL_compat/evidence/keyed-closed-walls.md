# Actor-specific XL closed walls

Loaded XL missions now use a separate closed-wall doorway path. A player owning
the exactly matching blue or red key can pass those closed walls. Gold and
combined key masks do not enable that rule. Actorless visibility, robots,
pickups and projectiles retain ordinary closed-wall behavior. Redux remains
the alternate; multiplayer and original-mission qualification stay separate

## Source and integration

Pinned reference: arbruijn/d2x-xl 2576a98b5de310e3ac7c9777df2677d1627b8bae

The executable oracle extracts the complete CWall::IsPassable function and
compares its closed-wall branch without linking the production helper. Pinned
SHA-256 values:

- main/wall.cpp: 05eaa2cab8dd7f5661afd210e0edc8dea4fcd2bb2075c00d3012bec395ab0c42
- include/wall.h: 08c59bf8c299ea05967f806b92b3d03b4226914ef747ae1ef5afa4154c70c2bd
- physics/sphere_collision.cpp: 499873929ce6606f00b504e59624595e6bee57f9f4e225f53c85ebd0c1a80b60

XL ignores door-opened and illusion flags on closed walls. The shared Redux
fallback can treat a door-opened bit as passable regardless of type, so adding
only a key exception would be insufficient. The new XL helper computes the
whole closed-wall result, including cloak/texture transparency, while leaving
other wall types on their current path

Actorless doorway queries feed rendering and visibility. FVI and player
containment use the actual actor. Matching keys grant physical passage without
making an actorless query passable. Blocked destinations and boost-area exit
rules remain outer constraints. Existing no-clip and observer dispatch remains
in place. Invalid actor/player/segment/side indices cannot acquire permissions

The rule reads live wall type and player key flags, so ordinary key ownership,
wall transitions, disk and rewind remain authoritative. No mutable gameplay
state or new save schema is introduced; development schema 10 remains current

## Native evidence

55,296 exact source comparisons cover nine actor/query types, all 16 authored
key masks, all eight player key combinations, six cloak values, opaque versus
transparent texture flags and four wall-flag combinations. MSVC and Linux GCC
with -Wall -Werror pass

120 real actor crossings cover both sides of blue, red and gold portals for
players, robots, pickups and projectiles under five key-ownership combinations.
The red portal also tests blocked destination precedence. Actorless transparent
rays pass. Active boost constraints still stop a blue-key owner, and release
restores passage. Invalid player IDs receive no key permission

Live key removal and wall-type changes select the current behavior. Disk restore
through a stock mission restores the earned key and the separate physical and
visibility results. The stock doorway control retains Redux's original flag
semantics and the XL helper leaves the stock player unchanged

All 15 focused regressions pass in 72.08 seconds, covering inventory, teleports,
chains/scheduling, object events, boosts, blocked regions, repair, fluids,
boss routes/groups and trigger flags. All 12 private-policy checks pass; 65 core
feature tests are required. Catalog acceptance is recorded with Android below

## Fixture and original audit

The derived keywall fixture changes six existing portals in the scaled
water-actor fixture. Walls 0/1 are blue, 2/3 red and 4/5 gold; each has the
otherwise misleading door-opened bit and cloak 9. Segment 15 is blocked and
24 is a boost area. Original Neptune bytes remain unchanged

- Original neptunxl.hog: 4016618d24238803135dabf2b33daee2a2c34a670c3f016d94ae4639c8887303
- Derived keywall.hog: 53a6d817b280cbb81a10f16cccfd024f368627756ab03634699dc97cad14f14f
- Derived keywall.mn2: e3c111d43ae21e63ae7d3e23e33bbbbe78b4976fb44e89831c50775c23db3577

[closed-wall-candidates.json](closed-wall-candidates.json) freshly inspects
121 surveyed closed-wall levels containing 15,086 such walls. Ninety-three
decode; 28 retain explicit rejections. Decoded MB Arena contains eight blue and
eight red closed walls. Each original retains runtime_qualified=false, including
missions whose other requirements are unsupported. Repeat audit preserves
identical report bytes and modification time

The initial native fixture basename exceeded the stock save format's eight-byte
mission identifier. Renaming this derived asset to keywall allowed the actual
disk-through-stock test to complete. This does not claim new long-name support

## Android acceptance

The registered test_xl_keyed_wall_runtime runner stages pinned derived assets and
backs up/restores protected files. Its script uses controlled key ownership and
real controller thrust to check passage/rejection, blocked precedence, live
visibility flags, disk through stock and rewind. It does not claim key-pickup
or autonomous navigation coverage. All 90 checks pass in 67,854 ms on
emulator-5554, run 473acbe901954e31837f835f1f5f24bf. Protected file hashes
match before and after; emulator-5556 was untouched. The tested APK SHA-256 is
759d0c9b59b9c8fa16153bbe72069f631a6df2ca5ae0021c38da7f30f58d5b11

The checks include physical rejection without a key, blue-key crossings both
ways, rejection after key removal, red-key blocked-destination precedence,
gold-key rejection, and actual crossings after disk, checkpoint and
stock-to-save restoration. The catalogs pass with 404 top-level and 459 support
tests, including 99 standalone JSON and 291 standalone PowerShell tests.
Scoped mixed-language quality and pinned PowerShell lint pass
