# XL shield-damage and energy-drain wall actions

Implement on xl_more_15, stacked on group PR #157. No merges.

The supplied original survey contains three type-17 shield actions (3 and 201
fixed units) and one type-18 energy action (5 percent) in Dinter 23/40. These are
gameplay requirements, not cosmetic presentation. The original retains its
other independent blockers and is not qualified by a derived fixture.

## Semantics and scope

Reference arbruijn/d2x-xl 2576a98b5de310e3ac7c9777df2677d1627b8bae,
main/trigger.cpp and CShipEnergy in objects/player.cpp/include/player.h.
For the normal D2 ship profile, shield action subtracts float-converted fixed
units; energy action subtracts a percentage of current energy. Negative values
replenish. Both clamp to raw -1 through 200 fixed units. Zero shields remains
alive; negative shields starts the normal death sequence. Already-dead players
skip resource changes but still reach action flags. Links are inert. These
direct authored changes bypass combat invulnerability and no-damage protection,
matching the reference leaf rather than routing through combat damage.

Keep this in an XL implementation and Redux as the alternate. Support wall
actions and their existing immediate/delayed master dispatch. Direct object
resource actions remain unqualified; do not guess the reference's use of the
geometry-trigger table for an object index. Alternate leaves these types intact.
Existing optional XL ship scaling/automatic recharge and multiplayer are outside
the admitted default profile. Define extreme arithmetic with a wide intermediate
and final clamp, documenting where source float-to-int/addition is undefined.

## Acceptance

- Compile the actual pinned leaf cases and CShipEnergy::Set against default
  player/storage/death adapters; compare raw fixed-point results across damage,
  percentages, negative values, zero, caps, rounding and dead players. Mark
  defined-range comparisons separately from safe extreme-value controls
- Check real native wall/master dispatch, scheduler boundary, flags, lethal
  death, protection bypass, and player-resource disk/rewind restoration through
  stock. Preserve inert target data without accepting unsupported direct object
  actions or other independent requirements
- Run a reusable Android controlled-pose test with authored activation, resource
  values, disk/checkpoint persistence, stock isolation and lethal continuation
- Update browser/evidence, catalogs and required private tests; run focused
  regressions and scoped quality checks; publish separately with green CI

## Completed local acceptance

All 840 source leaf/Set/Update comparisons and ten defined extreme-value controls
pass with MSVC and Linux GCC. The actual-loader control passes protected-volume
and invulnerability bypass, wall/nonplayer immediate masters, replenishment,
flags, inert targets, pending disk restore and normal zero/negative death
boundaries. All 24 focused native regressions, all 12 private-policy checks and
both catalogs pass. Android passes all 62 steps, including two asynchronous
checkpoint roundtrips, a different-difficulty stock transition, disk restore and
lethal continuation, restoring protected files exactly. See
[resource evidence](../../XL_compat/evidence/player-resources.md) for source,
fixture/APK hashes and exact scope. Publish separately and require green
latest-head CI before reporting this tranche complete.
