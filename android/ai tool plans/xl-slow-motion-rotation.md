# XL slow-motion rotation stage

Base: #391, 67a826bc9162caf27f3674d188f24d26246bd5a2

## Scope

Implement the pinned DoPhysicsSimRot angle division after angular thrust/drag
integration and after conversion to signed 16-bit angles. Select the console
clock by pointer identity. Keep turn-roll timing, rotational force integration,
classic and multiplayer behavior unchanged. Pickup admission remains disabled.

## Validation plan

Extract the exact angle-scaling fragment from physics.cpp at reference revision
2576a98b5de310e3ac7c9777df2677d1627b8bae, with the existing complete-file hash
pin. Compare native output for neutral, transitioning, slow and bullet profiles,
console/world identity, network isolation and signed-angle boundaries. Verify
actual native rotation orientation, unchanged angular velocity, thrust/drag and
turn-roll clocks, and no RNG consumption. Reuse the mandatory water-actors owner
without changing trusted FAST membership. Run existing physics/lifetime, fluid,
projectile, cloak, classic and public regressions plus source sanitizers.

## Remaining work and limits

This stage does not qualify reference matrix/trigonometry fidelity: it preserves
the existing native rotation math and checks the new slowdown arithmetic against
extracted source. Collision/retry and translational force controls, gameplay input,
authentic sprite rendering, audio, active-clock persistence and original rendered
Warp Shock qualification remain separate work. No original mission or asset edits.

## Results

- Required water-actors owner passed with 144 extracted-source rotation rows,
  288 actual native orientation updates (including full-turn angle wrapping),
  and 48 force/drag/turn-roll isolation updates
- Existing 768 displacement/lifetime and 144 lifetime boundary comparisons pass
- Source oracle ASan/UBSan passed; leak detection disabled for this environment
- Seven transcript mutations rejected: missing, duplicate, incorrect result,
  unknown profile, truncated, trailing fields and unknown record kind
- Native water, lava, projectile and cloak owners: 4/4 passed
- Classic upstream and trigger demo owners: 2/2 passed
- Public owner: 55/55 passed; private-assets Python policy: 33/33 passed
- Scoped mixed-language quality and automation catalog validation passed
- Catalog execution remains blocked: child supervisor exit 125, missing
  /proc/self/task/<pid>/children on this host
- Windows/Android and a full native sanitizer build were not run; source-only
  sanitizer coverage does not establish native sanitizer coverage
- No rendered original Warp Shock run or readiness improvement is claimed

Neptune input SHA-256 remains
4016618d24238803135dabf2b33daee2a2c34a670c3f016d94ae4639c8887303.
The complete pinned physics source hash is
2ca8d5c939f31da9d4686637ed3e4305bf89e208a575f2f7f6ea2b4d2e6fab76.
The genuine pickup sprites at private assets PR #4 head
d09f199c3b66bab71970364b1939b1658851ea81 are reserved for the rendering
stage; this rotation stage does not consume them.
