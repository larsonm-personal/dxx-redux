# Authored spray particles

Original Pumo `tulev2.rl2` contains 15 serialized subtype-1 spray emitters.
They were rejected by the fire/smoke-only admission gate. Spray is not the
subtype-2 bubble system.

Pinned D2X-XL revision: `2576a98b5de310e3ac7c9777df2677d1627b8bae`.
`CObject::SetupSmoke` multiplies modern face-emitter particle counts by four
for spray. `DoStaticParticles` maps spray through its default smoke branch:
smoke image, fade type 1, ordinary static-particle lifetime and brightness.
The existing particle reference target extracts both routines and checks their
files against these SHA256 values:

- `objects/effectobject.cpp`: `0a2fcf48756916f8bdb65f1ee01cc06fd9d108fb02d0a81d3b0d3ab1b3b0e048`
- `effects/objsmoke.cpp`: `ce39fc41866b703f0978af4ec0af6b4b98e30071b6c8015eaa60b719f8d32b80`

The decoder already implements that source conversion. This change admits
stationary single-player spray objects, routes their resources through the
owned smoke atlas and creates `SmokeEmitter` instances. Authored subtype and
face density remain source-owned; saves restore authored activation and rebuild
transient clouds. Other particle kinds remain gated.

The existing native particle owner now exercises smoke and spray alongside
fire, checking live emission, the fourfold face density, smoke image/fade,
alpha blending and resumed emission after activation restoration. The resource
owner applies its missing, malformed, ownership, precedence and installation
controls to a spray-only request for the smoke image.

The original Android route is extended to level 8 with all 15 spray owners,
activation disturbance/checkpoint recovery, live smoke-emitter checks and atlas
GPU checks, followed by the existing hub save restoration. All 59 steps passed
on Android in 302.111 seconds. This is controlled dispatch, not an earned player
walkthrough or a full-world spray pixel comparison.

The first original route attempt stopped in level loading: generic Redux
`fix_object_segs` relocated an effect owner, and strict particle binding rejected
its changed segment. Android then terminated the game through its fatal-error
handler; this was not evidence of a memory-pressure kill.

Pinned XL `CObject::VerifyPosition` and `FixObjectSegs` explicitly skip
`OBJ_EFFECT`. A minimal D2 hook now preserves these owners while loading or
running an XL level. Classic and multiplayer paths retain the generic repair.
The particle binding guard remains strict and now reports object and segment
identities. A native control places an effect outside its authored segment and
checks that the real repair pass preserves its authored position/segment, with
stock-policy isolation. The full native effect/resource owner and original
Android route passed with this source-aligned loader behavior.

The paused checkpoint test uses the existing `checkpoint_cycle drain` command
to collect its asynchronous callback without advancing simulation, then
`roundtrip_paused` before comparing saved activation and owned atlas pixels.
Waiting for ordinary gameplay-frame collection while paused had left the
checkpoint pending; no production checkpoint change was needed.

Validation receipts are under `temp/xl-skybox-20261031`: `native-runtime.log`,
`native-runtime/controls.log`, `admission-test.log`,
`android-spray-position-build.log` and
`android-original-spray-checkpoint-flow.log`. Scoped formatting, both automation
catalogs and diff whitespace checks passed. The route owner restored protected
device files after the run.
