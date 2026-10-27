# Object-attached triggers: work in progress

This chunk extends the singleplayer roadmap after wall scheduling. Object
runtime work is under verification. The local branch admits the implemented
nonplayer leaf actions while object masters, other extended actions/flags,
player owners and boss teleports remain gated. This is not a completed chunk or
an original-mission qualification

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
count. Fully decoded records gain a count while remaining blocked

`xl_object_trigger_decode_reference` compares three derived records in each of
the three layouts against hash-pinned extracted reader code. This is decoder
and normalization evidence; no runtime event dispatch is tested by this check

The current five-test native selection passed: inventory contract, derived
reference decode, original GlassHouse reference decode, original Neptune
inventory and original Observatory inventory. The native build reports no new
warnings, and scoped mixed formatting/lint passes. Logs are retained under
temp/xl-object-triggers-20261026. No Android event integration has run for this
chunk yet

## Native runtime checkpoint

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

All 1456 exported production event samples match extracted reference
DoExecObjTrigger and CObject::Damage calculations across types, raw/fixed values,
disabled/permanent flags, event kind and shield boundaries. Sixteen malformed
extension controls reject count, owner, signature, type, flags, value, operated,
destination, numeric overflow, schema, identity, checksum and truncated/missing
data. Both wall and object records validate before either set is published

Object persistence uses schema 2 of the bounded trigger extension and includes
object actions/owners in source identity. Existing wall-only sources keep schema

1. The selected last destination is persisted rather than adopting the pinned
   reference loader's reset on restore. Negative threshold arithmetic is widened
   to avoid the reference's signed subtraction overflow at extreme authored values

The final native checkpoint passes 11/11 focused regressions: inventory,
derived/original object decode, object events, wall teleports/chains/scheduler,
modern properties/water, original Neptune features and Observatory descriptor.
Scoped mixed formatting/lint and the final admission-test style check pass

The shared D1/D2 x86_64 Android APK builds successfully. Its checkpoint SHA256 is
fe04c3f22de74e93c3c2af17e69097073fdd13cbe6ccd83bcf01fb5ad72ab95c.
This is compilation evidence; Android object-event and rewind acceptance has
not run. Logs remain under temp/xl-object-triggers-20261026

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

## Remaining acceptance

Object-master child context, independent teleport/sort comparisons, boss
bookkeeping, Android disk/rewind integration and complete feature-specific
admission/browser evidence remain open. Repeat affected checks after subsequent
source changes. Do not claim this chunk complete from
the current native checkpoint or qualify a new mission from these derived tests
