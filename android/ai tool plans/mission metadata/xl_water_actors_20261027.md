# Modern water with robots and generators

Continue the authorized eight-hour implementation run on xl_more_19_water_actors,
stacked on PR #163 (d57881ef7). Do not merge. Monitor the latest heads of PR #161
and #163 until their CI is green

## Scope

Qualify modern 23/40 water together with stock robot definitions, authored robot
and equipment generators, combat and persistence. Keep the shared binary and
separate XL behavior. Legacy experimental fluids, optional physics settings,
multiplayer, custom HXM gameplay definitions and independent mission requirements
retain their existing gates

1. Audit the actual XL physics consumers, not only helper functions. CPhysSimData
   chooses 75-percent movement time for water. CheckWallPhysics returns before
   CheckSegmentPhysics for nonplayers, so the latter's apparent robot turbulence
   and lava-damage branches are not ordinary robot-frame behavior
2. Exercise actual robot and projectile physics, wall contact and water boundary
   crossings. Compare displacement, reconstructed velocity, RNG and active
   lifecycle with dry controls. Preserve player-only turbulence and sound logic
3. Use an authored combined fixture with wet robot/equipment stations, real
   generation, pickups, robot combat, and independently timed event/producer
   state. Verify actual AI/fire and ordinary collision handlers through controlled
   poses; do not navigate the level autonomously
4. Verify disk and memory restoration through stock and fresh source loads.
   Extend admission only after runtime controls pass, and preserve HXM and
   version-specific restrictions. Do not accidentally widen the existing water
   arena's custom-definition exception to ordinary robot missions
5. Add a registered Android integration with owned JSON and master timeout.
   Run required scoped quality, catalogs, private policy and focused regressions.
   Record exact evidence without promoting an original mission merely for loading
6. Commit/push one coherent tranche, open a stacked PR and require latest-head
   green CI. Continue remaining work within the run; no PR merging

## Reference

arbruijn/d2x-xl 2576a98b5de310e3ac7c9777df2677d1627b8bae, pinned CRLF tree:
C:/local/dxx-redux/temp/xl-boss-transport-20261027/reference-bootstrap-crlf/reference-source

Read physics/physics.cpp, physics/collide.cpp, objects/updateobject.cpp, the robot
firing/collision consumers and generator production. Existing native arena
controls cover synthetic robot/weapon motion but do not establish active AI,
generation or Android interaction; this tranche closes that evidence gap

## Progress

The existing XL physics path passes 72 exact source-offset comparisons on MSVC
and Linux GCC. Actual wet/dry crossings, terminal wall velocity, normal AI shots,
ordinary damage in both directions, generated pickups and pending/earned disk
restoration pass. Modern admission now allows the combination; legacy and HXM
controls remain restrictive. All 23 focused native regressions and both Android
catalogs pass, as do all 12 private-policy tests

Final Android acceptance passed 86/86 steps in 325,704 ms, including live
projectile damage in both directions, persistence of the damaged robot, mixed
producers, pickups, disk/rewind and stock switching. Run
cb8e16cb4849464da05b45d2d670b47a used APK
cae1b14ee083d1e16f0071b1669c5e56bb4bd7dcb80a0499e7255eb093c5e443.
Protected emulator state matched its pretest hashes. Fresh inspection of all 15
modern-water survey candidates decodes without granting gameplay qualification;
an identical repeat preserves canonical report bytes and modification time
