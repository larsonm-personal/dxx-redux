# XL projectile hit ownership before wider object storage

Continue the thirteen-hour overnight run, ending 2026-10-28 15:18:29 UTC.
Work in xl_more_33_projectiles, based on published xl_more_32_triggers at
459dbd24bc6be558fc54aacc6249392aa7110358 (PR #177). No merges; leave the user's
clean cmake checkout untouched. Full singleplayer fidelity remains the target

## Evidence motivating this order

The pinned source sets MAX_SEGMENTS_D2X to 20000 and MAX_OBJECTS to its runtime
segment capacity. Redux currently uses 1000 objects, with a 1000-byte persistent
weapon hit array inside every object. Raising MAX_OBJECTS directly would grow
this storage quadratically, affect ordinary save-extension widths, and alter
stock allocation/cleanup behavior. These are architectural dependencies, not
reasons to narrow the eventual target

XL uses a separate per-projectile table of MAX_HIT_OBJECTS=20 signed-short IDs.
Its FindHitObject/AddHitObject callers and object lifecycle must be audited before
choosing the final owner representation. Keep Redux's existing behavior as the
alternate in the shared binary. This batch should implement the XL hit behavior
and persistence needed for wider object storage, without claiming wider object
admission before the remaining consumers and allocator are complete

Reference revision 2576a98b5de310e3ac7c9777df2677d1627b8bae:

- physics/collide.cpp: d10f88f82e5d283077914881d1acb719f06785a86be22e50617d92d6fce47bc9
- include/object.h: ed546edd2bbda56a84e9fef8543706707d47a08b48e36616852346b5a2f96127
- objects/objectio.cpp: 528f64e09469b5a452e9c6cf0ae8e9d5dd3eaffeaf5478378669f0783deaf68f
- main/gamedata.cpp: 782acd85fae6fa5fbb0162a9f05474ae449063d70bf566d172c9d47ae4ed1a12

The source AddHitObject uses overlapping memcpy when full, shifts right, then
replaces the last slot. That is undefined behavior and also duplicates the oldest
entry rather than clearly evicting it. Audit actual source results/callers and
explicitly document any deterministic correction. Do not silently call a changed
eviction policy source parity, or copy undefined behavior to pass a fixture

## Implementation and acceptance

1. Audit reference creation/reset, collision eligibility, duplicate-hit selection,
   full-table behavior, removal/slot reuse, serialization and demo restoration
2. Introduce XL-owned per-projectile state with explicit live-owner identity,
   cleanup and separate dispatch. Keep stock hit arrays and ordinary save widths
   stable; do not merely replace MAX_OBJECTS everywhere
3. Verify actual robot, player and reactor collision consumers as applicable,
   piercing/repeated contacts, distinct victims, owner reuse and boundary cases.
   Preserve source selection/RNG/damage semantics; document intentional fixes
4. Add checked disk and memory restoration, including through stock, malformed
   owner/count/ID state and recovery. Do not claim whole-world atomic rollback
5. Extract independent pinned source functions and compare production behavior
   wherever defined. Use meaningful native combat/physics controls and a
   registered owned Android integration with protected-state restoration
6. Run scoped formatting, relevant stock regressions, both catalogs and required
   platform builds. Commit/push a stacked PR only after acceptance; monitor #177
7. Then widen object storage/allocator identities, allocation headroom and all
   consumers in the next dependency step. Remaining segment/light/generator
   capacities and expanded gameplay definitions remain open

Implementation and scoped acceptance are complete. The separate XL hit table,
collision dispatch, impact attrition and checked schema-11 persistence have
MSVC/GCC reference, actual native combat/physical travel and stock controls.
FIFO eviction and full-history persistence are explicit source corrections.
Eleven related native integrations pass. Both Android games build, and all 53
owned integration steps pass, including paused worker capture, three memory
round trips, disk through stock, repeat damage and projectile reuse. See
[evidence](../../XL_compat/evidence/projectile-history.md). Protected hashes match;
formatting, PSScriptAnalyzer and both catalogs pass. Final publication follows

## Published prerequisite state

- PR #175 wide walls: all seven workflows green
- PR #176 separate cloaks: all seven workflows green
- PR #177 trigger capacity: one commit / 66 files, correct base xl_more_31_cloaks;
  all seven workflows green at 08:44 UTC, including Linux headed replay
- #177 runs: Tooling 40650725148, Engine 40650725077, Linux saves/replays
  40650725175, LAN 40650725305, sample 40650725121, XL 40650725078,
  Rust 40650725152

Trigger acceptance: 1037 extracted source cases (MSVC/strict GCC), public three-
layout boundaries, native high wall/object links, 254-trigger and 2047-wall
metadata snapshots, disk/malformed recovery and demo controls. Related seven
native integrations and stock D2 pass. Final Android run
6efd1d1ffad24c12b1e195bff637fbd9 passes 77/77 in 70508 ms; protected hashes match.
Both catalogs and scoped formatting/PSSA passed. Dinter/Antholgy still have 13/5
independent blocker categories and are not qualified as playable
