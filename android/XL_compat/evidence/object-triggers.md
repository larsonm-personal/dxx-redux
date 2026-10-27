# Object-attached triggers: work in progress

This chunk extends the singleplayer roadmap after wall scheduling. Object
gameplay admission remains blocked while runtime activation and persistence are
implemented and verified. Decoding does not qualify a mission

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

Production damage, removal, reactor and autoplay calls; object-specific action
dispatch and ordering; once/repeat/disabled/alternate semantics; object slot
reuse; teleport destination history and boss bookkeeping; bounded atomic
persistence; stock transitions; independent event comparisons; native combat
and Android disk/rewind integration remain open. Do not enable an object action
using the wall teleport path or claim this chunk complete from parser checks
