# Boss object teleports

The later [delayed-chain tranche](object-chain-delays.md) retains actor identity
and event context across delays. It replaces the development event trailer with
schema 6 while preserving the boss destination state verified here

The later [boss group tranche](boss-groups.md) supersedes this record's combined
owner/group gate for neutral-owner segment groups. Team ownership remains gated.

Boss-owned object teleports now rebuild only the moved boss's cached destinations
through the separate XL runtime. Direct damage actions and immediate object-master
children use this path. Redux retains its existing globals and dispatch. No
original mission is promoted by the derived controls below.

## Behavior and persistence

The reference is arbruijn/d2x-xl 2576a98b5de310e3ac7c9777df2677d1627b8bae.
`tests/reference_boss_routes.cmake` pins objects/boss.cpp, main/trigger.cpp and
include/descent.h, extracting actual SetupSegments, Setup and DoTeleportBot.

- Moving to another segment rebuilds the owner's list using current passability,
  its destination group and size filtering. A same-segment action keeps the old
  list, even if a wall has changed. The other boss remains independent
- Traversal preserves the reference's ordering, home revisit and boundary-child
  omission. XL's 1,000-entry storage bound is separate from the native geometry
  limit. The inherited stock one-wall fallback is removed from XL setup
- Position/relinking follows the object-action path; facing and velocity survive.
  Spawn, cloak, AI teleport, hit and death clocks are preserved. Ordinary wall
  teleports retain their distinct behavior and do not rebuild these lists
- Checked schema 5 stores each earned list and its setup home. Restore validates
  owner count, row types, bounds, first/home entry, groups and duplicate structure
  before publishing event or lifecycle state. Publication transfers prepared
  storage without allocating. The trailer is bounded at 8 MiB, accommodating the
  existing 1,000 native object slots with bounded per-owner lists
- Restore uses captured destinations even when walls changed after setup. It
  does not substitute a new search from the saved actor position

Existing delayed object-chain, unsupported owner/action/flag, payload, capacity,
resource, dynamic-boss and multiplayer gates remain. Positive owner/group records
also retain their independent inspection gate; reference graph comparisons do
not admit previously unknown authored group semantics. Source setup's unused
gate-segment array is not claimed as another implemented spawning path.

## Reference and native controls

All 24 native samples match the extracted object/setup functions: zero, single and
multiple destinations, previous-destination retries and complete RNG transcripts,
same-segment actions, size-filter inputs, two owners and immediate-master context.
Native checks seed nonzero spawn/cloak/teleport/hit clocks and a rolling-death
case, ensuring a reset cannot pass because the initial values were zero.

The production traversal is also compared directly against the extracted setup
on 20 independent graphs: sizes 1, 2, 17, 1000 and 1001 with differing groups,
failed size checks and closed first walls. Source queries receive recorded engine
centers, passability and size-test results; this verifies list/action semantics,
not independent collision mathematics or a full reference-engine run. MSVC and
warning-free Linux GCC pass.

The actual-loader `xl_boss_transport` test verifies authored damage and autoplay
master actions, both isolated destinations, subsequent AI teleports and stock
isolation. After opening an old boundary, a same-segment action preserves the
earned list. A forced-recompute negative control produces a larger list. Disk
restore through stock retains the saved smaller list and subsequent AI behavior.
Sixteen malformed trailers are rejected before valid wall/object mutations can
publish. The full 19-test native regression sweep passes.

The fixture derives from unchanged Neptune HOG
4016618d24238803135dabf2b33daee2a2c34a670c3f016d94ae4639c8887303,
using the previous enlarged two-boss fixture. It replaces the two object actions,
one wall child and two pairs of illusion walls with solid barriers, isolating
destinations 70 and 84. Its eight-character mission basename is bosstran; the
native save format does not retain a longer basename. These edits do not qualify
original Neptune.

Derived HOG SHA256:
09e5deac6470879bd501ab60f982eec79c610f5b65d0dd4afc0e4142f887c1a2

Descriptor SHA256:
7a929982ead40553557c9364916cc773a15dad23e5a8d895fd142737f936fb2e

## Android and test wiring

The normal APK passes 48/48 steps: authored damage/master transport, separate
destination state, three exact asynchronous checkpoint roundtrips, a different-
difficulty stock transition and ordinary disk restoration. Protected files have
identical before/after hashes. Only emulator-5554 was used; there was no autonomous
navigation. Native controls establish subsequent AI teleport selection; Android's
short resumed-AI interval alone is not a claim that an AI teleport occurred.

Final run ID: b4a5381215714827b4bcd98f0144a1fd, elapsed 148622 ms.
Acceptance APK SHA256:
1002f0de8ec3dde15ac1ca1dab31bd2994c8b698fd2d9eb02235bdbbedc0c43c

Reproduce with `android/tests/test_xl_boss_spawning_runtime.ps1 -Transport`, the
derived HOG/descriptor and an isolated serial. The existing runner owns the new
JSON script and retains/restores the selected mission's files. Both catalogs and
all 12 private-policy checks pass.

The private runner previously required source comparisons without configuring
their reference root. It now bootstraps the exact public source commit, or accepts
an explicit --reference-root; CMake checks consumed-file hashes. A fresh bootstrap,
reuse and CMake registration check pass locally. Missing comparisons still fail
instead of being omitted. The private receipt now requires xl_boss_transport.
The bootstrap explicitly uses CRLF working-tree bytes, matching the existing
source pins on both hosts rather than inheriting the machine's Git setting.

Logs, source samples and bootstrap checks are under temp/xl-boss-transport-20261027;
the Android receipt and protected-file hashes are under
temp/xl-boss-transport-android. These are bounded feature controls, not full
campaign, arbitrary boss-profile or universal AI qualification.
