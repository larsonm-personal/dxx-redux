# Authored bubble qualification

## Current status - completed through tranche 122

The original 27/40 object-316 point-emitter payload is now qualified in the
runtime, with either initial activation byte. The integration owns the original
bubble and smoke images and linked audio, preserves source water retirement,
orientation, deferred queued-frame wiggle and FX ordering, and uses the qualified
multi-image shader. Unsupported bubble profiles remain rejected.

Protected Android controls passed 90 wet-fixture steps and 11 dry-fixture steps,
including live scene pixels, activation, ordinary checkpoints, audio ownership,
stock isolation and real graphics-context recreation. Both runs restored identical
protected app data. These are isolated derived fixtures using the original
serialized payload; original pmlev3 still has independent camera and lightning
blockers and is not qualified for human playtesting.

The completed runtime receipts appear in the final section. The chronological
sections below retain the scope and failures observed at each earlier checkpoint;
their statements that runtime integration or admission is pending are historical.

## Historical source foundation - tranche 119

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

## Runtime image ownership foundation

The overnight continuation adds bubble.tga to the existing owned addon-image
preparation path and prepared-level resource lifetime. It validates the original
four-by-four atlas layout and uses the same TGA decoding and mission/installation
ownership policy as fire and smoke. This does not enable a bubble emitter or
change admission, and it does not qualify the multi-image runtime shader.

The existing native effect inspection owner now exercises a bubble resource
profile alongside fire, smoke, spray and lightning. All five profiles passed
mission/default/installation precedence, missing and malformed resource rejection,
foreign shadow rejection, case-insensitive lookup, retained pixels and recovery.
These resource controls use synthetic TGA pixels; they are not an original-asset
rendering witness. The native MSVC build and integration owner passed, including
the existing four loaded-world source cases, 507 segments and 9633 queries.
Scoped mixed-language quality passed. The Android x86_64 build passed in 2m57s.
No bubble runtime drawing or device qualification is claimed by these checks.

## Runtime owner and deferred submission

The continuation binds BubbleEmitter into the native particle manager with the
full native object basis retained at creation, matching source point-jitter
updates that do not replace orientation. It uses the compiled water world and
the shared activation, pause, retirement and restore lifecycle. Admission still
rejects bubbles explicitly until rendering and audio consumers are qualified.

The native owner passes the original pmlev3 object-316 payload on isolated
Neptune geometry with derived wet/dry properties. It checks full orientation,
point jitter/cadence, water retirement, pause, activation restore, fresh transient
clouds, signature recycling and simulation-RNG isolation. These controls do not
load or qualify original pmlev3.

The shared transparency queue now retains bubble references and queued frames,
resolving billboards only when the source scheduler submits a buffer. Native
controls pass mixed smoke/bubble ordering, live atlas coordinates, queued-frame
world-X wiggle before a rotated camera transform, object-boundary flushing and
singleton suppression. The submitted two-bubble/one-smoke batch consumes exactly
two FX draws; the suppressed bubble consumes none. Restoring particle state clears
the frame queue before replacing emitter slots.

The multi-image runtime backend now compiles on MSVC and Android x86_64. It binds
the active smoke/smoke/bubble textures and captured depth, carries image selectors,
uses source 5/3/5 depth intervals and ONE/SRC_ALPHA blending, and restores all four
touched texture units. Its program, VAO and buffer participate in context disposal.
Android compilation passed in 1m22s. The protected Android shader qualification
below covers device pixels and context recreation. Live scene and linked-audio
qualification remain pending, and bubble admission remains closed.

## Protected Android multi-image shader qualification

The existing protected POG runtime owner now supports BubbleGpu with original
owned smoke and bubble atlases. Its 28-step script passed on the owned x86_64
emulator, including an actual EGL context recreation. Each of two checks passed
774 pixel samples with a maximum one-byte channel difference, 646 visible pixel
witnesses and two mixed-selector batches. Controls cover soft and nonsoft paths,
three image selectors, all sixteen atlas cells, zero/nonzero vertex alpha and
four depth gaps. Hostile incoming GL state is restored, including four texture
units. Before/after protected application-data hashes are identical.

The receipt is temp/xl-bubble-gpu-android/result.json, run ID
7bf3da23173e44d2aad5c021bfd225d3. Scoped quality, Android compilation and both
automation catalogs passed. This uses the production backend with retained
original assets; it does not load original pmlev3 or qualify live bubble-scene
submission, linked sound or ordinary save/restore.

## Pinned bubble audio creation contract

The source oracle now extracts the actual release KillObjectSmoke body, generic
sound class, addon index and airbubbles filename from hash-checked source files.
The previous scaffold's unused sound constants were not audio qualification.
Bubble creation requests generic class 0, forever playback, half fixed-point
volume, 256-unit range, default loop bounds and 11:airbubbles.wav. Bound emitter
updates do not create another sound. The release disable path stops the particle
system without destroying its linked sound; the debug implementation differs.
The source comparisons passed on MSVC and GCC, including 2592 sound creations.
The original airbubbles.wav was extracted without executing its package;
its SHA-256 is 539b434ca912a17442017e4f68aad21953c285ba2d816bb5bb7c2d6678f0e5cc.

The runtime now prepares this resource through the owned named-audio bank and
creates a generic linked owner at emitter creation. Repeated creation uses the
existing resource/object/signature deduplication, preserving restored playback
position and stopped owners. Release disable leaves the sound bound; normal
audio synchronization handles range and native object lifetime. Atomic restore
also validates particle-linked sound identity and authored playback parameters.

The existing native audio integration owner passes the bubble controls in both
23/40 and 27/40 layouts with the original WAV: actual spatial PCM, enabled and
initially disabled emitters, disable/re-enable, range exit/return, paused updates,
restored phase, stopped-owner restore and recycled/deleted object lifetimes.
Seven malformed resource/parameter snapshots are rejected without mutation.
Portable fixtures provide a labelled generated WAV; BUBBLE_WAVE selects and
hash-checks the original asset. The Android x86_64 build passed in 3m26s, and the
full native effect inspection owner passed after audio integration. Device live
bubble scene, audio and ordinary checkpoint qualification remain outstanding.

## Live integration qualification in progress

The local continuation provisionally admits only the original object-316 payload
in 27/40, with either initial activation byte, to qualify the integrated scene.
Eleven nearby payload variations remain rejected. Distinct wet/dry derived hub
fixtures copy the original serialized payload and change only the emitter
segment's water flag; unrelated source bytes remain identical.

The first protected wet-fixture run failed at the initial live scene readback:
72 bubbles were alive, but no qualifying visible batch completed in 15 seconds.
The saved probe counters cannot yet distinguish missing submission from fully
clipped quads. The inherited smoke test view may miss the smaller bubble plume;
this is a hypothesis, not a diagnosis. Protected application-data hashes match.
This failure is retained under temp/xl-pbubble-android, run
0d26560703064696adf9e91c8e48122f. Live scene/checkpoint qualification is incomplete,
and this admission change is not ready to be presented as completed support.

## Completed original-profile runtime qualification

The scene failure above was traced to the test view: the original hub emitter
was twelve units behind the inherited smoke camera. Looking toward side 2 of
segment 59 keeps the camera inside the same geometry and faces the bubbles.
No emitter or rendering behavior was changed to obtain visible pixels. Requested
scene probes now distinguish submitted batches from fully clipped batches.

The protected wet fixture passed all 90 steps, run
fab51744d5b5460988d06c33abd72d78, in 123099 ms. It covers actual live scene pixels,
activation/draining/re-enable, owned source images, ordinary disk checkpoints,
exact restored audio voices and linked owners, fresh transient clouds, actual EGL
context recreation, stock isolation and restore from stock. Four live scene
readbacks passed across these transitions; the final check changed 9349 pixels
in one three-quad batch, with no clipped batches. There was one EGL recreation.

An intermediate checkpoint comparison exposed stale spatial caches after the
test moved its paused listener with face_view. All playback positions and the
bubble owner already matched; three unrelated sound owners differed slightly in
cached pan/volume. The save control now runs normal audio synchronization at its
final camera pose before capturing the expected snapshot. The exact comparison
remains intact. Failure receipts are archived in 122-checkpoint-failure and
122-checkpoint-diagnostic-failure under temp/xl-overnight-20261101.

The protected dry fixture passed all 11 steps, run
d91edbb33022430b85ef41f6e554c9e2, in 34177 ms. It observes particle creation before
disable, then verifies retirement while the generic linked sound remains owned.
The native/source controls independently distinguish wet survival from dry
retirement. Both protected runs restored identical application-data hashes.
Complete success receipts are archived in 122-wet-qualified and 122-dry-qualified
under temp/xl-overnight-20261101.

Admission is limited to the original 27/40 point-emitter payload, with either
initial activation byte. Other bubble profiles remain rejected. Original pmlev3
still has independent camera and lightning blockers; these isolated controls do
not qualify that level or provide human playtesting evidence. Scoped quality,
MSVC/native controls, Android builds and both automation catalogs passed.
