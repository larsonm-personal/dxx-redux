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

## Loaded water and mixed-buffer controls

Tranche 120 carries decoded segment water properties into the compiled particle
world. Its receipt includes those properties and the compiled water flag, and
compares them with the actual pinned HasProp/HasWaterProp bodies. The native owner
exports unchanged stock and XL geometry plus two explicitly derived property
controls covering all 128 combinations of the seven decoded bits. Original
mission bytes and live native state remain unchanged.

Both GCC and MSVC pass four loaded-world cases: 507 compiled segments and 9633
native lookup/collision queries. The wet control has 93 wet segments; the other
three have none. Each case also runs 720 bubble emitter frames against actual
source creation/update/emission using independently compiled source geometry and
water predicates. The wet case performs 475 lookups and 464 collisions, while the
dry stock case performs 70 lookups and no collisions because water retirement
precedes collision testing. Fire and smoke loaded-world controls also pass.

The generic buffer scheduler now supports source smoke/bubble shader
compatibility and an explicit no-shader mode. Actual source comparisons pass
523 profiles and 287172 inputs, checking every intermediate/final dispatch,
mixed image types, overlap, replacement, singleton suppression, boundary flush,
zero distance and capacity. Existing runtime callers still submit only the
previously supported particle kinds; this does not enable mixed-image drawing.

Linux source/GPU regressions passed 3/3 in 43.87 seconds. After adding the positive
water controls, final source-only runs passed on GCC (2.20 seconds) and MSVC
(8.56 seconds), and both compilers passed the regenerated native receipts.
The Android x86_64 build passed in 4m10s. Both automation catalogs passed.

The first device attempt passed its initial GPU controls but lost the host adb
daemon connection during the background/resume phase, leaving no automation
result file. The captured log contains no application crash. The runner restored
identical protected app-data hashes; adb and emulator 5556 were responsive when
checked afterward. This attempt is recorded as failed, not as device validation.
The unchanged retry passed 28/28 steps in 60579 ms, run
ae001d0b57c34e54a784231caad81cfc, including context recreation. Its protected
app-data hashes were identical after restoration. It remains regression coverage
of the existing renderer rather than a bubble presentation qualification.

The first source build exposed that the extraction helper expects multiline
function delimiters. The inline water predicates now use checked complete-line
extraction. The initial two native worlds were both dry, so the positive/negative
property controls were added before claiming water integration coverage.

Bubble image loading, ownership, deferred wiggle and the source particleFS
shader remain separate runtime work. The active non-array source shader binds
smoke, spark and bubble textures, carries an image selector in the third texture
coordinate, uses alpha-controlled blending, and selects per-type soft-depth
intervals. Substituting the existing single-image glare shader would not qualify
that path. Bubble admission remains closed.

## Multi-image particle shader equations

Tranche 121 adds a portable fragment shader for the active non-array particleFS
paths. The existing EGL owner extracts actual particleFS[0], particleFS[2] and
particleVS from particle_shader.cpp, SHA256
33d7e40ccbe4611c7137f29f4cacd29a2fca124d6b389570253d58f7e40b1ca3,
at the same pinned source revision. The inactive texture-array variants are not
used by these comparisons.

The source and portable shaders pass 1176 complete 8x8 RGBA framebuffer
comparisons with a maximum one-byte channel difference. There are 1004 visible
pixel-change witnesses. Controls cover smoke, spark and bubble texture selection;
zero/nonzero vertex alpha; foreground/equal/behind scene depth; near/far depth;
and the source per-type intervals of five, three and five units. Distinct texture
colors and alpha values expose selection and premultiplication mistakes. Blend
factors come from the actual pinned SetBlendMode body and are checked as
ONE/SRC_ALPHA. Tests exercise the active, nonsuspended shader path.

The full existing EGL GPU owner passed in 29.37 seconds, including its other
renderer regressions. Scoped formatting and both automation catalogs passed.
Android/native/device validation from unchanged runtime prerequisites is reused.
This is desktop shader-equation qualification; it does not qualify an Android
shader backend, texture loading, buffer submission, GL-state restoration or
context recreation for the new shader. It is not connected to runtime rendering,
and no particle admission rule changes in this tranche.
