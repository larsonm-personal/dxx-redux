# XL equipment generators

Implement on xl_more_10, stacked on the unmerged boss PR #152. Keep a separate
XL equipment runtime and stock Redux as the alternate path. No merges.

Reference: arbruijn/d2x-xl 2576a98b5de310e3ac7c9777df2677d1627b8bae,
main/producers.cpp, main/trigger.cpp, gameio/loadobjects.cpp and
objects/createobject.cpp.

## Semantics found in the source

- Modern function 11 binds equipment row i through the segment's authored
  producer index; redundant disk segment/station fields do not select the owner
- Initial enablement uses the combined producer list's segment at equipment
  index i, not necessarily that equipment maker's own segment. Trigger discovery
  considers destructible wall controls and valid object controls
- Activation toggles enablement without resetting timer or pending phase
- The delay is three seconds times difficulty plus one, then half the morph clip
  duration. A powerup or an object whose ID equals OBJ_PLAYER blocks the first
  phase, resetting its timer. Preserve this source predicate explicitly
- Selection uses the two authored powerup words, with no RNG for zero/one choice
- Failed effect allocation retains the waiting phase; failed item allocation
  starts another full delay. Items are immortal and use normal pickup behavior
- Equipment production is independent of robot suspension

## Acceptance

1. Decode and expose equipment records; validate modern layout, ownership,
   capacities and complete selectable-powerup/effect dependencies
2. Translate the native geometry without leaking XL function 11 into stock
   enums; own production/toggle state in the XL runtime
3. Preserve enabled, pending and elapsed state through checked disk/rewind saves;
   reject malformed state before publishing any event state
4. Compare production timing, blocking, selection, failure and initialization
   against pinned source extracts across difficulties and RNG transcripts
5. Exercise actual authored activation, pickups, replenishment, pending-phase
   restores, stock transitions and independent robot generators natively and on
   Android, using controlled poses and introspection, without navigation
6. Run focused regressions/catalogs/formatting, update maintained evidence and
   open a separate PR with green CI. Do not infer original-mission qualification
   from derived fixtures; retain independent payload/data/multiplayer blockers

The survey contains eleven equipment-bearing levels. Sphere and Boiling Point
contain modern function-11 makers; pmlev1p has a legacy 17/35 layout with two
type-15 makers. That older function encoding remains outside this chunk.
Audit original bytes separately before any admission claim.

## Completed evidence

All 210 production comparisons and 13 initialization comparisons match the pinned
reference extracts. Native mixed-order controls and 22 malformed-save/assignment
guards pass. The normal Android APK passes all 61 steps, including real pickups,
replenishment, mixed producers, three authoritative checkpoint roundtrips and a
pending disk save restored through a different-difficulty stock mission. Protected
device files restore exactly. See the maintained
[equipment evidence](../../XL_compat/evidence/equipment-generators.md) for pins,
scope and receipts. All 15 focused native regressions, 12 private-asset policy
checks, both catalogs and scoped formatting pass. Linux GCC matches the source
oracle too. Original missions remain blocked independently.
