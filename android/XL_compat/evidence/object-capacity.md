# XL object storage, allocation and persistence

The shared D2 binary now sizes admitted XL pools from authored object counts,
using max(count + 1000, count * 2), with 32768 physical slots. Redux and
imported D1 retain their 1000-slot active limit. Loading is
based on validated source requirements. This does not qualify an original
campaign, expanded custom robot data or the remaining XL AI/combat semantics.

## Source and representation

Reference: arbruijn/d2x-xl 2576a98b5de310e3ac7c9777df2677d1627b8bae.
The seven source files and their hashes are pinned by
`tests/reference_object_capacity.cmake`: objects/object.cpp, include/object.h,
include/segment.h, include/descent.h, main/gamedata.cpp, objects/createobject.cpp
and objects/updateobject.cpp.

The reference uses LEVEL_OBJECTS=gameData.objData.nMaxObjects. Its initializer
computes max(authored count + 1000, authored count * 2), with 20 slots of normal
frame headroom. The separate MAX_OBJECTS_D2X=20000 constant does not define
that live pool. All 20002 authored counts from zero through 20001 match the
extracted initialization expression. Allocation pressure begins at capacity minus two and requests ten
free slots. XL reclamation has its own implementation: debris, unattached
fireballs, flares, then other weapons, preserving the source's swap-removal
candidate order. Ordinary production cleanup performs actual deletions and
owner events. A cleanup guard prevents pressure-triggered recursive cleanup.

At exact requested occupancy, the reference decrements a zero remaining count
below zero and can mark every candidate. The port deliberately makes this a
no-op. Of 2048 native samples, 2016 match the extracted source and 32 verify
this correction. MSVC and strict GCC (-Wall -Wextra -Wpedantic -Werror) pass all source comparisons.

Signed short object indices hold 0..32767 and the -1 sentinel. The current
physical pool therefore supports up to 16384 authored objects with the required
dynamic headroom. Larger authored sources report their required runtime slots
and the supported bound; they are not admitted with reduced headroom. Wider
object references remain unfinished. The
Redux embedded hit bitmap stays 1000 bytes; XL uses its separate bounded
projectile history. Widening the bitmap along with the object pool would
otherwise produce quadratic storage and change classic object/save layouts.

Physical AI, lighting, tracing and ownership arrays follow the expanded pool.
Allocation, validation and serialized AI/allocator/afterburner counts follow
the active mission. D1 translation, stock weapon bitmaps and network bounds
retain classic widths. Inactive object slots are cleared on mission changes;
restoring a stock allocator initializes its unused physical tail explicitly.

Development XL save schema 12 replaces schema 11. Source-based restoration
selects the pool before reading live objects and runtime state. Negative,
truncated and over-capacity core counts reject before writing the object array;
a failed live-world restore requests return to the menu. The legacy loader is
not a transactional whole-world rollback. Stock save versions/widths are unchanged.

## Lifetime identity and demos

XL uses a 32-bit lifetime sequence. Redux retains its 16-bit sequence and rejects
wide allocator seeds. Fifty-seven actual engine assignments across the 32767
and 65535 boundaries and near INT_MAX match the extracted source assignment.
The source signed overflow is undefined; the port explicitly wraps at INT_MAX
and skips live identities. A full 32768-slot pool exposed why 16-bit lifetime
signatures could not remain: freeing one object otherwise reuses its identity.

Object, parent, AI-danger and save signatures already use ints. Linked audio
ownership now retains the full int; its separate sound-handle type is unchanged.
Native and Android controls exercise wide-owner links, synchronization and
cleanup, including actual Android mixer playback and reused object slots.

Classic demo objects keep their short signature bytes. A private render-byte
wrapper 255 precedes objects requiring 32-bit signatures; normal render types
never use that byte, so old readers reject it rather than silently narrowing.
The wrapper carries the ordinary render/type payload with a full int signature.
Imported D1 records cannot use it. Bounded readers reject negative or narrow
wrapped identities, absent identity payloads and invalid wrapped render types.
This is Redux recording support, not a claim to read every XL demo dialect.

## Native and public controls

`xl_inventory_contract` covers 15/33, 17/36 and 23/40 with 999, 1000, 1001,
16383 and 16384 authored objects, rejects 16385 and truncated payloads, and
checks bounded translation. These public controls need no private mission data.

The private `xl_object_capacity` integration derives object records while
preserving Neptune's original geometry. It checks:

- Ordinary admission/loading at 999, 1000, 1001 and 16384 authored records,
  with exact source-sized headroom
- Full 32768-slot exhaustion, high-slot reuse and distinct wide live signatures
- Actual pressure reclamation and allocation, plus invalid free-list rejection
- High-ID robot/fusion damage and repeat-hit suppression
- Exact active allocator order, AI and projectile state through disk restoration
  after a stock mission; stock inactive allocator input cannot leak back
- Actual high-ID robot AI firing, player damage and a death-triggered wall action
- Classic/wide demo codecs at 0, 32767, 32768, 65535, 65536 and INT_MAX,
  byte-preserving rewriting, complete frame decoding, four malformed wrappers
  with explicit decode errors, unchanged prior output and valid recovery
- A canonical metadata snapshot containing the expanded object population
- Negative, zero, oversized and truncated core save counts in both XL and stock
  pools, with valid restoration after every rejection
- Oversized, truncated, invalid-segment and unowned translated source rejection,
  successful recovery and unchanged original HOG bytes

D2 `test_upstream_compat` passes in 43.54 seconds and the D1 native equivalent
in 2.70 seconds after the final identity changes. All 72 configured XL CTests
pass across the full run and focused reruns. Eight initial failures were stale
private-asset paths; restoring hash-checked originals and the required D1 retail
assets resolved them. The boss reference sample writer also needed to report
active pool availability rather than physical backing capacity. Its corrected
50-case reference comparison passes, including allocation exhaustion.

## Android acceptance

The owned `test_xl_object_capacity_runtime` test fills original Neptune's full
source-sized production pool,
leaves controlled effect headroom, and creates high-ID fusion/robot contacts.
The generic checkpoint control now verifies the entire active allocator at
each of three immediate restoration boundaries, before simulation resumes.
The disk control uses the same save-slot API as the UI and compares allocator
order plus earned projectile/victim state immediately after restoration.
Final formatted-build run 701294cdbf5f4c0e8afec303e03f8c2f passes all 53 steps in 91597 ms, with
source-sized headroom and signatures above 65535. This supersedes run
09bcc3dcd5404d28bac244bd17e9a29d, which used the incorrect fixed pool.
Protected player/configuration/mission hashes match after cleanup. The normal
Android Debug APK builds both games. This is controlled restoration evidence,
not a UI-response-time or large original-mission performance qualification.

Earlier UI-only observation attempts failed after simulation resumed: saved
projectiles were absent by the later pause/check. Another earlier assertion
incorrectly required the preparation-time allocator to remain unchanged across
ordinary effect expiration and later frames. Exact allocator assertions now
run at restoration boundaries; subsequent checks still require valid allocation
and the original earned projectile history/damage. No production physics or
collision behavior is disabled to make restoration pass.

## Original missions and remaining work

The hash-matched corpus audit is in `object-capacity-originals.json`.
D.I: Monolevel has 1857 objects; Anthology has 1279. Both now lose their object
capacity blocker and retain independent effect, property and action blockers.
Neither is promoted to playable. This inspection does not establish resource
closure, rendering performance or correct gameplay for either original.

Wider object slot references remain unfinished beyond the 32768-slot bound.
Other capacities, custom data, optional profiles, multiplayer and broad Android
performance remain unfinished. The large native fixture exercises maximum slot
capacity; Android exercises the original source's active pool, not a 32768-object
rendering-performance workload.

Final review corrected the initial fixed-20000-pool implementation before
publication. That earlier design would have changed source reclamation timing;
its passing controls did not cover authored-count sizing. The current reference
initializer and boundary loads specifically cover the discovered dependency.
