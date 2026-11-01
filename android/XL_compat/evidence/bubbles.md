# Authored bubble source controls

Tranche 119 adds bubble creation, movement, emission and billboard primitives.
It does not admit bubble objects into the runtime. Original Pumo pmlev3 object
316 still requires runtime ownership, water geometry, resource loading and
deferred rendering integration before its admission blocker can be removed.

The controls compile actual D2X-XL Create, Update, emitter and Render/Setup bodies
from pinned revision 2576a98b5de310e3ac7c9777df2677d1627b8bae through the existing
reference-effect owner. They compare particle state and FX random consumption,
not a second hand-written approximation of those algorithms.

The source comparisons pass:

- 1024 creations covering point/face emission, full orientation, authored size,
  default/custom color and seeds
- 9216 updates covering water exit, missing segments, collision, lookup cadence
  and lifetime boundaries
- 2880 emitter frames, 24 drained rings and six zero-capacity rejections, with
  4601 lookups and 4441 collisions across irregular timing, movement and
  disable/re-enable transitions
- 1152 rendering comparisons covering queued-frame world-X wiggle, live atlas
  coordinates, lifetime, animation, color and non-axis-aligned billboards

Source bubbles keep decay at one and apply per-update direction displacement;
they do not use smoke's random direction changes. Emission demand consumes the
source random choice before checking whether the emitter remains enabled.
Creation preserves the source argument expressions and their compiler-selected
FX evaluation order. GCC and MSVC each compare against source compiled by that
same compiler; this is not a claim of identical cosmetic positions across them.

Wiggle uses the frame captured when queued, while UV coordinates reflect the
live frame after animation. Its random choice belongs at buffer flush, not at
emitter traversal. The new quad helper documents this requirement, but runtime
queue integration is deliberately still pending. The default source wiggle and
wobble behavior is used by the controls.

The final Linux source/GPU regression selection passed 3/3 in 34.34 seconds;
the MSVC source selection passed in 10.27 seconds. Existing fire/smoke source
controls remain in that owner, and the shared billboard GPU tests passed.
The Android x86_64 build passed in 4m3s and both automation catalogs passed.
The protected renderer regression passed 28/28 steps in 59569 ms, run
e0add57185f747fc9348e856a15e4936, including context recreation. Protected app-data
hashes were identical after restoration. This is regression coverage of the
existing renderer, not an on-device bubble presentation claim.

Early development comparisons exposed a missing post-collision bounce branch
and an incorrect sequential evaluation of the point-spread random expressions.
Both were corrected before the final source runs. No admission or runtime
fallback was loosened to make these tests pass.
