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

## Corrected contact rationale

The original contact-9 explanation was invalid. The fixture is a terminating
laser, but the oracle applied an isolated FixPosition arithmetic fragment after
its death. Pinned physics.cpp ProcessWallCollision returns -1 for
OF_SHOULD_BE_DEAD (lines 714-715); DoPhysicsSim propagates that into a return
before FixPosition (lines 1291-1299). FinishPhysicsSim does not change velocity.
Thus the claimed 262144-to-16384 reconstruction was not the upstream result.
The preserved pre-fix transcript's rejection by that oracle was not evidence
that dry XL contact behavior needed broadening.

The correction is based on PR #393 head
c023cbba807b44077d70d7a8117453d349fa60a3. Remove the unsupported dry-actor
widening. In the existing water/lava override, suppress reconstruction only for
dead wall contacts. Preserve the previous bad-start and surviving-contact
scope, speed-boost/stopped/bounced exclusions, classic and multiplayer behavior.
The earlier water actor assertion also expected the incorrect reconstruction;
it now requires the terminating projectile's incoming velocity to remain intact.
The 64 native slow-motion wall controls make that requirement explicit too.

The reference now executes the pinned post-callback death check and caller
negative-result return before the complete FixPosition reconstruction condition.
Post-callback death state and completed retry state are adapter inputs. This is
not execution of the entire upstream collision callback or FVI engine.

TurnTowardsVector needs its own world/console scaling before sign saturation;
scaling rotation angles alone does not implement this force consumer. The
helper uses the same isolated clocks and leaves null vectors and real-time force
integration unchanged.

## Historical validation, not a fresh correction pass

- 1536 native turning rows match the pinned angle-delta, wrap, small-angle boost,
  scaling and saturation statements, including morph rates, old velocity signs,
  console identity and network isolation. Zero-vector calls remain inert
- 64 real axial projectile wall cases cover dry/wet rooms, termination/bounce,
  four clock profiles and console/network combinations. Pinned ComputeMovedTime
  and UpdateOffset reproduce the final retry position. The original terminating
  velocity comparison was invalid and is withdrawn
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

## Surviving contacts and correction validation

The source-controlled completion matrix covers players, robots and a powerup
storage type across dry/water/lava scales, wall/bad-start/object/no-hit results,
and speed-boost/stopped/bounced exclusions. Only wall cases supply a dead state,
because the proven death short-circuit belongs to the wall callback. Alive
controls explicitly assume the retry loop completed. They do not demonstrate a
native surviving wall trajectory.

The actual Redux reconstruction predicate is compiled with and without the XL
configuration. Its dry all-type policy is deliberately not widened by this fix.
A surviving, head-on, nonbouncing physics powerup is a useful future native
comparison: unlike a robot, it lies outside the original player/robot/debris
predicate. Upstream's complete guard reconstructs that storage type if the
final result is still HIT_WALL. A native fixture must establish survival and the
final fate; a tangential slide may instead finish HIT_NONE. No general survivor
parity, bad-start recovery, corner behavior or powerup admission is claimed.

Fresh correction checks:
- Focused CMake configure/build and the registered asset-free CTest pass
- 360 pinned completion/eligibility cases and 960 checks of the actual Redux
  predicate pass in Debug, optimized NDEBUG and UBSan builds
- Seven compiled negative variants fail normally: missing/inverted death return,
  missing caller return, arithmetic-only reconstruction, dead-water regression,
  overbroad dead bad-start exclusion and restored dry-type widening
- The public owner selection test passes, including explicit omission, duplicate,
  skipped, failed and error results for the new mandatory test. Private FAST
  membership remains exactly 113
- Direct scoped clang-format, Ruff and cmake-format/cmake-lint checks pass. The
  repository quality wrapper could not resolve its upstream Git baseline in
  the bounded sparse staging environment

The full native water/lava/projectile/classic owners and native 64-contact
transcript have not been rerun for this correction. Full catalog execution,
Windows, Android, ASan and original rendered Warp Shock qualification remain
unverified. The new tests execute extracted source decisions and the production
predicate, not a rebuilt native engine. Historical owner results above must not
be reused as evidence that the corrected native engine has run.
