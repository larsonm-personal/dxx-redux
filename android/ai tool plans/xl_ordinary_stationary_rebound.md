# Ordinary stationary reactor and clutter rebound

Pinned XL BumpTwoObjects applies velocity times minus twice the moving actor's
mass in its ordinary nonphysical branch. Redux uses minus mass, producing a
stopping impulse. Add an XL singleplayer bridge that reuses the validated
camera_reflect_velocity arithmetic for this branch only. Keep ordinary
speed-boost suppression, existing static behavior-135 handling, physical/physical
collisions, classic/D1-in-D2 and multiplayer behavior unchanged.

The bridge checks mass and fixed-force bounds. Return both source impulse and
resulting velocity so existing native bump probes retain before/after ordering
and accurate source force. Do not change collision damage, impact or sound
dispatch, and preserve protected clutter's earlier no-damage return.

Extend existing no_damage_bump_test controls through actual collide_two_objects
for stationary reactor and clutter in both orders, zero and oblique velocity,
both signs, zero/tiny/non-power-two masses, protected clutter and controlled
multiplayer dispatch. Use the existing modern-property stock recovery for
classic controls. Existing physical and behavior-135 controls remain intact.

Extend the existing complete BumpTwoObjects source oracle with ordinary
nonphysical pairs, delegating its force endpoint to the already extracted raw
release ApplyForce implementation. Compare the existing reflection helper,
and distinguish factor-one and omitted-impulse alternatives. This source
arithmetic/control coverage is not a substitute for native consumer execution.

No new local checkout or build is allowed near the reserved disk floor.
Formatting is checked on a small source staging area; actual source-oracle,
native consumer and production-mutation execution remain hosted/pending.
Do not treat prior-head native status or the known projectile-owner failure
as qualification for this head. No merge or campaign-completion claim.
