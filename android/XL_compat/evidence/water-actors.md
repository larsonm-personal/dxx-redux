# Modern water with stock robots and generators

Modern 23/40 water can coexist with admitted stock robots, robot matcens and
equipment makers. Admission still evaluates every independent requirement.
Legacy experimental water with robots, multiplayer, expanded/custom gameplay
definitions and other unsupported segment properties retain their gates

## Source and scope

Reference: arbruijn/d2x-xl 2576a98b5de310e3ac7c9777df2677d1627b8bae

The oracle extracts CPhysSimData::Setup and CObject::UpdateOffset from the pinned
source, with separate XL vector math. Under default fluid physics, moving actors
use 75-percent displacement in water. The medium at the start of the physics
frame determines that entire frame; the following frame uses the new medium

CheckSegmentPhysics contains apparent robot turbulence and lava-damage branches,
but the ordinary CheckWallPhysics caller returns immediately for nonplayers.
Those unreachable robot effects must not be added by reading the helper alone.
The existing player-only contact path and actor motion scaling are retained;
this tranche removes the modern combined water/actor admission restriction

Audited CRLF source hashes:

- physics/physics.cpp: 2ca8d5c939f31da9d4686637ed3e4305bf89e208a575f2f7f6ea2b4d2e6fab76
- objects/updateobject.cpp: c4f3b9f1cd225f4dda99bb2a4e42af8546bc37ae36cc0bd19091dad6f48b01ac
- physics/collide.cpp: d10f88f82e5d283077914881d1acb719f06785a86be22e50617d92d6fce47bc9

Optional speed scaling, missile launch acceleration and slow motion are fixed
to their normal values in the oracle. No claim covers those optional profiles

## Native controls

Seventy-two exact offset comparisons cover robot, weapon and item objects in wet
and dry rooms, four frame durations and three signed/fractional velocities.
They pass on Windows MSVC and Linux GCC with -Wall -Werror.
Actual engine physics also verifies both portal directions and the next frame's
changed medium, projectile wall termination/reconstructed velocity, unchanged
unobstructed velocity, and no player-turbulence RNG for nonplayers

Authored mixed generators create energy, Vulcan ammunition and stock robot zero.
Global producer clocks retain their normal timing in water. The robot completes
morphing and fires through ordinary AI frames. Ordinary weapon collision handlers
verify robot-to-player and player-to-robot damage. Generated energy grants the
normal pickup reward. Disk controls restore pending production and an earned
robot through a stock mission. A level HXM presence control remains rejected

An initial fixture used stock robot one, which is a melee robot. Its failure to
produce a projectile exposed the fixture error. The final authored mask selects
robot zero, and the test checks the loaded attack type; no robot definitions are
modified to make the firing control pass

All 23 focused native regressions pass (239.22 seconds), covering admission,
modern properties and legacy water, movement, schedulers, object events, bosses,
generators, resources and source transitions. Both Android catalogs pass with
400 top-level entries and 455 support scripts; the automation inventory contains
99 standalone JSON and 287 standalone PowerShell tests. All 12 private-asset
policy tests pass; the required original-feature test set now contains 61 tests

## Derived fixture and Android acceptance

wetbots.hog is derived from hash-verified original Neptune geometry and the
equipment fixture. It scales geometry and object positions fourfold to provide
room for normal-size actors, applies modern water to segments 20, 22, 23, 60 and
92, and combines robot and item production. Original mission bytes stay unchanged

- HOG SHA-256: cea1f83d7b3bf2633cb1613935c740daa11067d243019181b7559094cabe06ba
- Descriptor SHA-256: 0bc9afd816c6dba98030a23bd491fbbc843313a2e305ff095765cd00e1f63c03

The registered Android runner is test_xl_water_actors_runtime.ps1, using the
owned JSON script of the same name. All 86 steps pass in 325,704 ms, run
cb8e16cb4849464da05b45d2d670b47a. Its controlled combat pose retains loaded
robot/weapon definitions and runs ordinary AI and physics. Both robot and player
projectiles inflict damage through live physics. The final disk load restores
the damaged generated robot at 36.2 shields in water. Pending/earned producers,
pickups, memory checkpoints and stock transitions are also covered. Protected
app state hashes match before and after the run

Accepted x86_64 debug APK SHA-256:
cae1b14ee083d1e16f0071b1669c5e56bb4bd7dcb80a0499e7255eb093c5e443

The reusable matcen_combat_pose automation action takes a generator and target
segment. It validates both actors' fit, stages them facing each other, clears
translation and selects ordinary chase/awareness state. It creates no robot or
weapon and applies no damage. Introspection reports each generated robot's
water state and live projectiles with matching parent object/signature

These controls establish feature behavior, not original-mission or campaign
qualification. Gameplay-affecting effects, other properties and resource
requirements continue to be reported independently

## Original candidate audit

[water-actor-candidates.json](water-actor-candidates.json) records fresh native
decoding of all 15 modern-water candidates from the hash-verified survey bundle.
All 15 decode; none is promoted to runtime qualification. The survey identifies
1,613 water segments, 11 levels with placed robots, eight with robot centers and
five with equipment centers. Capacity, effects, properties and other independent
requirements remain visible. Repeating the audit preserves the report's bytes
and modification time when unchanged

Reproduce with audit_spawn_candidates.py --feature water, the original survey
bundle, missions root and current xl_inspect executable. Inspection is not a
substitute for the loader's asset-aware admission or original-mission gameplay
