# XL slow-motion contact stage

Base #392, 48b6759665d0f8cacf709669176f00bd05109d82.

Trace pinned collision retry and force consumers. Add the missing turn-impulse
scaling before sign-change saturation. Compare the extracted angle-delta tail
with actual native turning, including morph rate, sign changes, boundary and
console/world/network cases. Keep vector-to-angle fidelity explicitly separate.

Exercise native wall termination and bouncing retries under active clocks using
unchanged original geometry in the existing derived water-actor fixture. Compare
actual contact movement with pinned ComputeMovedTime and UpdateOffset, with
native FVI contact geometry supplied as input rather than claiming a new complete
reference collision engine. Preserve classic behavior and all admission guards.
Run native owners, source sanitizers, public/classic regressions and scoped quality.
Controls, authentic pickup rendering, audio and persistence remain subsequent
stages. Do not merge, rewrite published history or alter the readiness cohort.

## Findings and correction

The new native transcript initially failed source comparison at contact 9:
neutral clocks, dry room 61, non-console projectile, single player, terminating
wall hit. It moved 4096 fixed units in a 16384-unit frame. Source reconstruction
requires velocity 16384, but the engine retained 262144. The old engine extended
wall reconstruction beyond players/robots/debris only in fluid rooms.

Extend the existing reconstruction predicate to all single-player XL wall and
bad-start contacts, preserving speed-boost, stopped and bounced exclusions and
classic/network behavior. Do not change unrelated dry object-contact semantics.
The preserved pre-fix transcript still fails the final oracle at contact 9.

TurnTowardsVector needs its own world/console scaling before sign saturation;
scaling rotation angles alone does not implement this force consumer. The new
helper uses the same isolated clocks and leaves null vectors and real-time force
integration unchanged.

## Completed validation

- 1536 native turning rows match the pinned angle-delta, wrap, small-angle boost,
  scaling and saturation statements, including morph rates, old velocity signs,
  console identity and network isolation. Zero-vector calls remain inert
- 64 real axial projectile wall cases cover dry/wet rooms, termination/bounce,
  four clock profiles and console/network combinations. Pinned ComputeMovedTime
  and UpdateOffset reproduce the final retry position; the pinned FixPosition
  fragment reproduces terminating single-player velocity
- Reference comparison takes native FVI hit geometry and native vector-to-angle
  output as inputs. It does not qualify the entire reference FVI or trig engine,
  corner collisions, bad-start recovery, arbitrary object contacts or speed boost
- Existing 768 displacement/lifetime, 144 lifetime and 144 rotation rows pass
- Source oracle ASan/UBSan passes, leak detection disabled; native engine itself
  was not sanitizer-instrumented
- Fourteen missing/duplicate/changed/truncated/trailing/unknown/identity-negative
  transcript variants rejected with ordinary failure status
- Native water/lava/projectile/cloak owners 4/4; classic upstream/trigger owners
  2/2; public source/GPU/audio owner 55/55; private-assets Python tests 33/33
- Scoped quality and catalog validation pass. Catalog execution remains blocked
  by the host child-supervisor missing /proc/self/task/<pid>/children, exit 125
- Windows/Android and original rendered Warp Shock qualification not run

Original Neptune HOG SHA-256 remains
4016618d24238803135dabf2b33daee2a2c34a670c3f016d94ae4639c8887303.
All reference fragments come from physics.cpp at
2576a98b5de310e3ac7c9777df2677d1627b8bae, raw supported-setup SHA-256
2ca8d5c939f31da9d4686637ed3e4305bf89e208a575f2f7f6ea2b4d2e6fab76.

## Remaining consumer audit

Pinned source still applies separate clock rules in ai/aimove.cpp and
ai/aiframe.cpp (turning/fire timers), weapons/controlweapon.cpp, fireweapon.cpp
and omega.cpp (fire eligibility, homing and charge), main/wall.cpp (door/cloak
animation and reversal), objects/reactor.cpp (firing), objects/object_physics.cpp
(wiggle), main/effects.cpp and objects/powerup.cpp (animation), plus particle,
lightning, waypoint, HUD/audio and save/load paths. These need individual
qualification; globally scaling FrameTime would contradict the force controls.

Pickup admission remains disabled. Genuine sprites at assets PR #4 head
d09f199c3b66bab71970364b1939b1658851ea81 remain reserved for rendering integration.
No readiness improvement, original mission changes or merges are claimed.
