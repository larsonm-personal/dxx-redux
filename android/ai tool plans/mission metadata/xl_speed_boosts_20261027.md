# XL speed boosts and restricted boost areas

Continue the authorized second eight-hour batch on xl_more_17_movement, stacked
on PR #160 (xl_more_16_events, 950abb450). The catalog records 281 authored boost
actions, making movement a useful next gameplay tranche. Do not merge either PR

## Required result

Implement actual singleplayer boost behavior in the shared binary, with its own
XL state and Redux as the inactive runtime alternate. A velocity assignment alone
is not support. Qualify the complete authored movement contract before removing
its admission blockers

1. Audit all active consumers in pinned XL source
   2576a98b5de310e3ac7c9777df2677d1627b8bae. Separate active physics code from the
   DBG_PHYSICS old simulator. Cover trigger activation/release, direction and
   destination construction, orientation steps, drag/steering limits, collision
   response/retry, boost-area boundaries, control restrictions, object deletion,
   death and level transitions
2. Preserve raw authored values and flags in inspection. Use actual source
   functions with bounded adapters for arithmetic/selection comparisons. Document
   any unsafe reference behavior corrected by the port separately from parity
3. Give live actors checked identity-bound boost state. Preserve earned velocity
   constraints, destination and any remaining orientation steps through disk and
   rewind, validating before publication. Ensure delayed/object masters use the
   correct initiating actor rather than the current player or a recycled slot
4. Add small engine hooks for the actual physics/collision/control consumers and
   implement XL behavior under android/XL_compat. Preserve stock outputs when XL
   is inactive. Check both games when shared hooks require it
5. Test real authored wall crossings, action dispatch and physical movement with
   controlled poses: no thrust, steering, constraints, zero/release, linked-side
   and side-normal directions, orientation, collisions, restricted exits,
   multiple actors, deletion/reuse, death, source transitions and disk/rewind
   boundaries. Do not attempt autonomous 3D navigation
6. Add reusable registered Android controls, source/native comparisons and a
   cross-feature test with delayed events. Run focused stock isolation, scoped
   quality checks and catalogs. Refresh capability reports/evidence without
   promoting original missions on the strength of derived fixtures
7. Commit/push an independently reviewable PR, verify latest-head green CI and
   continue the remaining batch. Monitor PR #160 concurrently

## Initial source map

- main/trigger.cpp: SetSpeedBoostVelocity, StopSpeedBoost, DoSpeedBoost,
  TriggerSetObjOrient, UpdatePlayerOrient, dispatch and exit cleanup
- physics/physics.cpp: CPhysSimData setup, ProcessDrag, boost collision handling,
  player force constraints; the later DoPhysicsSimOld is DBG_PHYSICS only
- main/segment.cpp and physics/sphere_collision.cpp: boost-area exit/passability
- physics/collide.cpp: player boost collision interactions
- objects/object_physics.cpp: wiggle suppression
- objects/updateobject.cpp: player velocity reset
- objects/playerdeath.cpp and gameio/loadgame.cpp: lifecycle cleanup
- input/systemkeys.cpp: boost-specific player control restrictions

Current reference root is the hash-pinned CRLF tree at
C:/local/dxx-redux/temp/xl-boss-transport-20261027/reference-bootstrap-crlf/reference-source.
Do not assume that a source block compiled only for debug is the active behavior

## Implementation and local acceptance

Implemented the separate per-owner XL runtime, physics/control/collision hooks,
source-based admission, schema-7 state, actual source comparisons and derived
native/Android integration controls. Reference-global turn state, invalid
object-parent lookup and stale retry destination/velocity are documented
corrections; Redux keeps its existing runtime alternate

All 4,740 source samples pass on MSVC and Linux GCC. All 26 focused regressions
pass, followed by three final formatted-build controls. The final APK passes all
49 Android steps, including memory/disk checkpoint comparisons and restoration
through stock. Protected-file hashes match. Scoped quality, both catalogs and
all 12 private-policy tests pass. See the
[acceptance evidence](../../XL_compat/evidence/speed-boosts.md) for exact identities
and limits. Reinspected original candidate reports remain unchanged

PR #160 is green and remains unmerged. Publish this movement tranche as a
separate stacked PR and require its latest-head CI to pass before considering it
ready for review. Continue the remaining authorized extended run afterward
