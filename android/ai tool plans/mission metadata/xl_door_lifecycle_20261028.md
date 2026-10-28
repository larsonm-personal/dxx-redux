# XL active door lifecycle

## Goal

Complete separate XL opening, closing, waiting and active-entry lifecycle behavior
for admitted singleplayer wall animations, keeping Redux as the alternate. Cover
real allocation boundaries, animation frames, physical passage, audio cues and
saved state. Do not infer mission qualification from an isolated fixture.

Base: xl_more_34_objects, PR #179, f09215f0a4e3c98897c4bf19d8341e362dceb333.
All 72 configured XL tests, both stock native integrations and the final 53-step
Android object test pass. All seven PR #179 workflows pass. No merges are authorized.

## Source audit

Reference: arbruijn/d2x-xl 2576a98b5de310e3ac7c9777df2677d1627b8bae.
main/gamedata.cpp creates activeDoors with MAX_DOORS=90. include/cstack.h starts
with growth zero, and no activeDoors.SetGrowth call exists in the pinned tree.
Thus the source supports 90 simultaneous entries, not an unbounded growing pool.
Its Delete compacts only live entries and the frame loop revisits the moved entry.

The current XL authored-close handler reserves a spare Redux array entry and
therefore supports only 89. Its reference adapter explicitly fakes allocation
failure at 89, so the passing matrix does not establish the actual source limit.
Redux animation also resets the slot after the live tail when entering waiting,
and can skip the entry shifted into a deleted slot. XL needs its own lifecycle.

## Work and acceptance

1. Pin actual stack allocation/deletion, source wall transitions, animation and
   caller behavior. Replace the artificial capacity adapter with source-backed
   boundaries, including reuse while full and failure without partial mutation.
2. Implement separate XL open/close/frame paths. Reuse the existing plain active
   door record and save representation where identical; keep Redux handlers
   intact. Verify two-sided textures, collision flags, sound and timer ownership.
3. Check active-door saved counts and record references before accepting them.
   Exercise malformed counts, payloads, topology, duplicate ownership and timers,
   followed by valid recovery; do not claim transactional whole-world rollback.
4. Extend native/source controls for 89/90/91 entry boundaries, simultaneous
   completion, waiting/obstruction/reversal, asymmetric clips, linked doors and
   real physical passage. State deliberate corrections to reference defects.
5. Add owned Android lifecycle, disk-through-stock and memory checkpoint checks,
   with protected-file restoration. Use controlled poses and authored actions,
   never autonomous navigation. Run relevant regressions, both catalogs and
   scoped formatting; publish a stacked PR only after acceptance and monitor CI.

Gameplay-affecting animation timing is required. Optional slow-motion profiles,
additional texture formats and campaign qualification remain separately tracked.

## Status

Implementation and local acceptance complete. Separate XL opening, closing and
frame paths use the full source capacity, checked saved ownership and own-face
clips. The source's allocation limit is established with its actual stack Grow
and Delete implementations rather than the previous artificial spare-slot
adapter. Paired-wall review added mixed types/automatic flags and linked-part
completion controls, including Android disk and rewind preservation.

All 3072 transition/allocation and 660 animation comparisons pass under MSVC
and strict GCC. All 72 configured XL tests pass; the final linked correction
also passes four affected native integrations. Stock D1/D2 integrations pass.
The final APK passes 50 full-table and 80 physical-door Android steps with exact
protected-file restoration. Both catalogs and scoped quality checks pass.
See [the evidence record](../../XL_compat/evidence/door-lifecycle.md) for receipts,
intentional reference corrections and remaining scope. Publication and CI follow.
