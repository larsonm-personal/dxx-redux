# Object-attached singleplayer events

This chunk extends the singleplayer roadmap after wall scheduling. Object
runtime is verified within the scope below. Admission permits nonplayer leaf
actions and immediate object masters while delayed descendants, other extended
actions/flags, player owners and boss teleports remain gated. This is not an
original-mission qualification

## Decoded source semantics

The bounded inspector reads all 53-byte object actions, preserving 16-bit flags,
raw value/time, targets and owner references. It retains the old previous/next
links and 700-entry head table in game 33, sparse object/head rows in game 36,
and direct owners in game 40. These legacy links do not define runtime traversal:
the pinned reference rebuilds groups by sorting owner then action type

Source object master values and most object times are normalized on reference
load. Inventory deliberately retains authored fields. Independent extracted Read
and owner-loading code compares the actual consumed extent and decoded fields,
including normalized master value/time, across 15/33, 17/36 and 23/40

The reference is arbruijn/d2x-xl revision
2576a98b5de310e3ac7c9777df2677d1627b8bae. Extraction checks the SHA256 pins for
main/trigger.cpp and gameio/loadobjects.cpp recorded in the
[chunk plan](../../ai%20tool%20plans/mission%20metadata/xl_object_triggers_20261026.md)

## Current controls

`xl_inventory_contract` tests all three reference layouts, multiple owners,
unowned records, wide flags, raw field retention and inert legacy links. Negative
controls include invalid owners and geometric targets, truncated actions/owners/
tails, negative or overflowing counts, sparse bounds, trailing payload and a
section boundary before the end of the owner table. A declaration whose payload
does not decode remains unknown in the browser rather than gaining a trusted
count. Fully decoded records gain a count; admission checks action and owner
requirements independently

`xl_object_trigger_decode_reference` compares three derived records in each of
the three layouts against hash-pinned extracted reader code. This is decoder
and normalization evidence; no runtime event dispatch is tested by this check

## Runtime evidence

The production loader binds source owners to native signatures and sorts actions
by owner/type with reference equal-key swaps. Robot damage, cleanup removal and
reactor destruction call the event runtime. Supported leaves share classic
mechanics; object teleports use segment centers and preserve orientation and
velocity. Immediate object autoplay disables even permanent actions, and removal
consumes ownership even when disabled or selected only by damage

`xl_object_events` passes real nonlethal local/nonlocal robot damage, lethal
combat followed by production explosion/cleanup, powerup removal, permanent and
once-only threshold actions, alternating removal, permanent autoplay, sorted
enable/disable actions and reactor consumption. Disk saves retain bindings,
flags, values, operated state and teleport destination history across source and
stock transitions. A deleted/reused object slot does not inherit the binding.
Direct stock illusion-off/on dispatch also passes after the shared leaf refactor

All 2184 exported production event samples, including 728 spawn-bot threshold
and consumption cases, match extracted reference
DoExecObjTrigger and CObject::Damage calculations across types, raw/fixed values,
disabled/permanent flags, event kind and shield boundaries. Eighteen malformed
extension controls reject count, owner, signature, type, flags, value, operated,
destination, numeric overflow, schema, identity, checksum and truncated/missing
data. Both wall and object records validate before either set is published

Object persistence uses schema 2 of the bounded trigger extension and includes
object actions/owners in source identity. Existing wall-only sources retain
schema-1 rows. The selected last destination is persisted rather than adopting
the pinned reference loader's reset on restore. Negative threshold arithmetic is
widened to avoid signed subtraction overflow at extreme authored values

Immediate object masters retain their owner through nested wall children,
including classic leaf dispatch and object-style teleports. Recursion is bounded
and subsequent dispatch recovers. Source master semaphores normalize to zero.
Wall-child teleport history is persisted in schema 2's sixth wall-row field;
wall-only schema 1 keeps its existing five-field rows

The native controls now include 56 independent sort comparisons (3472 total
records), 720 teleport comparisons with complete retry RNG transcripts, and the
extracted master's object-context/child-order checks. Eighteen malformed-state
controls include invalid wall-child history and a restored delayed descendant.
The inspector rejects both direct and nested delayed descendants before loading

The pinned master's foreign-array pointer subtraction is undefined. Redux uses
the real wall-child index while passing the object actor/context, as requested
by DoMasterTrigger. This deterministic indexing correction is documented rather
than claimed as reference allocator parity. Delayed object-chain context and
actor-release/reuse semantics remain an explicit unsupported requirement

The final native selection passes 11/11 focused regressions: inventory,
derived/original object decode, object events, wall teleports/chains/scheduler,
modern properties/water, original Neptune features and Observatory descriptor.
Scoped mixed formatting/lint and the final admission-test style check pass

The shared D1/D2 x86_64 Android APK passes all 59 registered integration steps,
run ID 63ca8e197c93414db055f5d58832d030, elapsed 127016 ms. Checks cover local/
nonlocal threshold damage, actual lethal explosion/removal, disabled and once-
only cleanup, reactor consumption, master context/history, disk restore and a
stock-to-source transition. Two captures each undergo three production rewind
restores, covering live and consumed object state. Protected app-private files
have identical pre-test and restored hashes; emulator-5556 was not used

Tested APK SHA256:
617b8073e400092ea2b70be6a081e2429fb52c55a135836935b6c0d3e6c0280b

Derived fixture HOG SHA256:
80a7d98f01c138716f3650f388216ffca0e45ab829b263726bb77d75f2014cd6

Derived descriptor SHA256:
221deb90c04e3c1d8b954dce20968c1d6df9f7926d6bda4b0c69bb4b75bcae01

Logs remain under temp/xl-object-triggers-20261026 and
temp/xl-object-events-android. Both automation catalogs and all 12 runner-policy
tests pass. Trusted native CI requires object decode and runtime controls and
now expects 44 required passes; unfinished campaign acceptance remains separate

## Original mission inspection

GlassHouse's original g-house.hog SHA256:
bcdbf127464ff1b82431997edba654c259462b405f9c388f58f6a09445f87980

Its 23/40 source has three decoded object camera actions (type 16), owned by
objects 126, 127 and 128. These are not object teleports or removal-controlled
doors. The optional `xl_glasshouse_object_decode` compares its source records
and owners against the pinned reader; cameras and other mission requirements
remain unsupported

Native inspection of Dinter, Sphere, Boiling Point, Water Shock 2, Anthology and
Tankboy still rejects other object layouts or malformed geometric targets.
Those blockers cannot be removed merely because their object-trigger section
now decodes. Full reports remain in temp/xl-object-triggers-20261026

## Remaining work

Delayed object-chain lifecycle, boss bookkeeping and other unsupported action/
owner variants remain on the broader gameplay roadmap, together with additional
layouts/capacities and original missions exercising these features. This finishes
the selected nonplayer event/immediate-chain chunk. It does not qualify a new
mission or complete the overall gameplay roadmap
