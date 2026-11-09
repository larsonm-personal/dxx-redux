# Use the ordinary XL nonplayer rotation force for bump damage

Pinned release physics/collide.cpp CObject::Bump applies full translation force,
then scales its vector by 1/(4+difficulty) for rotation and damage. Redux uses
the full force for both translation and damage. In active XL singleplayer,
use the already computed force2 magnitude for nonplayer damage only. Preserve
full translation, rotation, classic/D1-in-D2 and multiplayer, boss/static and
persistent gates, player damage and sound ordering.

Extend the existing no_damage_bump native owner with real player/robot and
player/clutter collide_two_objects dispatch. Equal unit masses and player
speed 32 isolate a mature ordinary robot's expected half shield point at
difficulty zero versus two before correction. Include difficulty four,
both orders/signs, boss/persistent/protected and stock controls, plus
controlled multiplayer clutter dispatch. Check full impulse, rotation and
existing awareness RNG with a closed sound gate. Save/restore fixture state.

Source review and arithmetic identify the fail-before case. No native
fail-before/pass-after or actual production mutants were executed locally:
no local build/checkout is allowed at the reserved storage floor. Full-force,
global scaled-force and scaled-translation mutations are expected fixture
targets, not observed mutation results. Exact-head hosted xl-pr-assets native
execution remains required; source checks are not native qualification.

The inherited xl_projectiles owner remains failed on the repaired #449 head.
That separate failure is neither fixed here nor a pass for this new head.
No merge or merge-readiness claim.
