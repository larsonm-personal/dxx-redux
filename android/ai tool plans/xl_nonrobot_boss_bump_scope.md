# Scope ordinary XL boss bump immunity to robots

Pinned release physics/collide.cpp CObject::Bump checks IsBoss only inside
OBJ_ROBOT. Redux bump_this_object instead indexes Robot_info for robot, clutter
and reactor alike. For active XL singleplayer only, short-circuit nonrobots
before that lookup. Keep robot boss immunity, classic/D1-in-D2, multiplayer,
persistent and existing static-robot paths unchanged. Do not change damage
scaling, force/rotation calculations or sound dispatch.

Extend the existing no_damage_bump native owner, not a new harness. Actual
player/clutter collide_two_objects controls toggle the unrelated robot
definition's boss bit and compare force, rotation, damage and RNG in XL,
stock and controlled multiplayer dispatch. Include both orders, signs,
difficulty, persistent and protected contacts. Direct bump_two_objects
robot controls retain real boss/persistent immunity with damage disabled.
Restore object/global state and RNG. Existing static behavior-135 fixtures
remain unchanged.

Source review identifies a fail-before case: nonpersistent XL physical
clutter with id zero and Robot_info[0].boss_flag set was suppressed.
The paired fixture requires the same response as boss_flag zero. The old
unconditional lookup and a global type-scope change are mutation targets;
production mutation and native fail-before/pass-after execution have not
run locally. No local build/checkout near the reserved disk floor.

Require exact-head hosted xl-pr-assets native execution before qualification.
Existing inherited xl_projectiles failure and pending parent fixture reruns
are separate evidence, not a pass for this change. No merge readiness claim.
