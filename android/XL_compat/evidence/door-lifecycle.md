# XL active door lifecycle

XL now has separate opening, closing, waiting and frame processing. Redux keeps
its existing alternate. The shared active-door record and serialized layout
remain unchanged; the XL handler can use all 90 physical entries without a spare
tail slot. This is a lifecycle qualification, not original-mission certification.

## Source and capacity correction

Reference: arbruijn/d2x-xl 2576a98b5de310e3ac7c9777df2677d1627b8bae.
`tests/reference_close_door.cmake` pins main/wall.cpp (through the keyed-wall
extractor), main/segment.cpp, include/wall.h, include/cstack.h and
main/gamedata.cpp. The latter creates the activeDoors stack with MAX_DOORS=90.
Its growth increment starts at zero and the pinned tree does not change it.
The source therefore has 90 entries, not an unbounded growing door pool.

The preceding close-door adapter simulated full storage at 89 entries to match
Redux's spare-slot requirement. That could verify transition arithmetic but
could not establish XL allocation behavior. The updated adapter extracts the
actual CStack Grow/Delete functions and MAX_DOORS definition. All 3072 close
transition/allocation samples and 660 opening/closing animation samples match
the source. The animation matrix covers frame boundaries, state, passage flags,
textures and elapsed time. MSVC and strict GCC pass these comparisons.

Each face uses its own animation clip and texture layer. Missing overlays use
the base layer; overlay orientation bits are preserved. Clip durations must
represent every frame without division by zero. Missing door or paired-face
animations produce a specific admission error, including the opposite wall ID.
Opposite faces with different wall types retain their types and animate using
their own valid clips.

## Intentional corrections

- Compaction moves only live entries and processes the shifted entry in the
  same frame. A completed automatic opening resets its own waiting clock.
- Both faces remain waiting until the deadline; the source prematurely marks
  the opposite face closing during the wait.
- A faster face cannot retire a still-opening partner. Waiting begins after
  all parts finish. For inconsistent per-face automatic flags, the initiating
  face controls the pair's final open/waiting state. This avoids the source's
  stranded waiting face or stale timer while retaining the authored flags.
- Linked opposite pairs keep source per-part timing but store the actual wall
  identity where the reference mistakenly stores a side number.
- Elapsed-time overflow saturates explicitly. Existing two-sided obstruction
  checks remain, including actors on the opposite side that the source loop
  misses. Force-close overrides waiting, not physical obstruction.

## Persistence and native integration

Restore bounds and reads the active-door table before installing it. XL also
checks part counts, geometry, clips, paired identities, duplicate ownership,
state combinations and nonnegative timers. Active door states without an owner
reject. Failed live restores request the menu; this does not make the legacy
whole-world restore transactional.

The derived fixture retains Neptune geometry and adds 91 connected door pairs.
Native production controls cover 89/90/91 allocations, full-table reversal and
reuse, failure without partial mutation, all 90 completions in one frame,
automatic deadlines and reuse after compaction. A disk save carries all 90 live
entries through a stock transition with exact identities/timers. Ten malformed
count/payload/part/topology/ownership/time saves reject, followed by successful
valid recovery.

Additional controls cover asymmetric durations, linked pairs, all four overlay
rotations, absent-overlay fallback and twenty opposite-type/automatic-flag
combinations. Every intermediate mixed-pair state passes the production save
validator. The existing close-door integration exercises authored actions,
twelve real actor obstructions, physical and visibility rays, earned waiting
and closing saves, and stock isolation.

## Android and regression acceptance

The final Android APK passes both integrations on emulator-5554:

- Full-table lifecycle: 50/50 steps, 67767 ms, run
  3c3fac43548e468fb7c85ab41d83737b. The first pair is linked with different
  automatic flags; its second-part identities and authored flags survive disk
  and memory restoration along with all 90 entries.
- Physical-door lifecycle: 80/80 steps, 82509 ms, run
  e20e716c903f4d47bcaf0c6c24b5eedb. Real player obstruction, passage, visibility,
  mixer attempts, waiting/closing restoration and stock transitions pass.

Both protected-file hash comparisons have zero differences. APK SHA-256:
85b9877c9f68dcdea13408c41968f068623858f54504493e8361a2a34a855d9f.

All 72 configured XL tests pass in 296.28 seconds. After the final linked-part
completion correction, the four affected native close-door/doorway/wide-wall/
cloak integrations pass again in 30.33 seconds. The stock D2 integration passes
in 46.81 seconds and D1 in 1.82 seconds. Both automation catalogs pass: 414
top-level entries, 470 support scripts, 99 standalone JSON and 301 standalone
PowerShell tests. Scoped formatting and pinned PowerShell lint pass.

`test_xl_door_lifecycle_runtime.ps1` owns the new script and protects/restores
player, configuration and mission files. The controlled probe invokes production
opening/frame/save code; it does not navigate the level or prove a playthrough.
The generic memory-checkpoint check now compares the entire active-door table
immediately after each restore, before simulation can advance its timers.

- Original Neptune HOG: 4016618d24238803135dabf2b33daee2a2c34a670c3f016d94ae4639c8887303
- Derived doorlife.hog: 9db4eb31fdd7e54870ed9f1cdad9f1763145a012d850b8f8f324ed4bd4b75a05
- Derived doorlife.mn2: e32d601e2f8c4a6f8a34a3206b760a7bea2a78c9007fae87e35ddcdd58a3bfde

The unchanged original candidates in [close-door-candidates.json](close-door-candidates.json)
remain separately qualified. Optional slow motion, ignore-marker flags, expanded
texture animation formats, full demo fidelity and other wall/material behaviors
remain unfinished. No mission gains campaign qualification from these controls.
