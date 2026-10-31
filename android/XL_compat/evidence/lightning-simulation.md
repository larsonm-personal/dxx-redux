# Authored lightning simulation

Original Pumo `tulev4.rl2` object 307 requests three enabled random style-0
lightning bolts in segment 362. Its authored life is 300, delay 50, length 18,
amplitude 5, offset 1, node count 8 and interpolation period 2. It also requests
glow, blur (copied from glow by the reader), sound and source-managed light.

The portable simulation implements authored parameter conversion, random bolt
initialization, node placement, style-0 displacement, sequential smoothing,
interpolation and persistent active/delay/restart scheduling. It preserves
displacement history across restarts and stages drawn positions before lifetime
reset, matching the source order. Its caller supplies effects randomness.

`create_lightning_bolt` accepts the implemented random, angle-zero, style-0,
unclamped profile without children or a fixed plane. It rejects other geometry
profiles. This is not a runtime admission change: lightning levels remain gated
until rendering, glow/blur, audio and lighting consumers are implemented and
qualified. Native ownership and activation/save integration now use the existing
effect lifecycle owner.

## Source comparisons

Pinned revision: `2576a98b5de310e3ac7c9777df2677d1627b8bae`.
The CMake reference harness verifies these source SHA256 values:

- `effects/lightning_manager.cpp`: `490b7f1fbd77228f90d354f49de69656f82364f0157a88cdad8fe8a4df6ff416`
- `effects/lightning.cpp`: `e84717bacf25a1d1f3b4d8c3432f281bc76717d60eed43485d23764188bdb236`
- `effects/lightning_node.cpp`: `6e98c8c61a65cc6f6f61101123962759ad104c8451fb3f9399af2139d1340510`
- `include/lightning.h`: `fc7bdf7f0b7e42f3ef216841121becda3781969f3b3aeffeb89d4413dcb74caf`
- `include/vecmat.h`: `83d7f73f89a2f588a0b56a08fb9e293b747cc65a5ed22bad6771d64319bdca0f`
- `maths/vecmat.cpp`: `4115afe844ad77a81e9f0de57ae1bbf577b06d5f49aed8b02d19002ff2021335`

Actual extracted source routines execute the conversion, initialization, node
path, bolt creation, placement, animation and lifetime transitions. Geometry
uses the source vector operations, default normalization and scaled cross
products. Unsupported source styles/children/clamp paths fail the harness if
entered. GPU allocation and `RenderSetup` are explicit boundaries: this test
records drawing coordinates and does not claim to validate GPU output.

Windows and Linux both passed:

- 1,296 authored setup profiles plus lifetime/random-call scheduling controls
- 432 generated node paths and 1,440 interpolation frames
- 5,760 complete create/frame/restart ticks, comparing positions, planes,
  displacement history, staged drawing coordinates, timing and random visits
- Unsafe authored arithmetic and source byte/short capacity rejection controls

The path comparison caught the source's random style-0 argument order: the
plane flag is passed as `bFromEnd`, while both displacement axes consume random
values. The port preserves this behavior.

The existing segment-edge scaling helper is now shared by the portable default
normal and native legacy side-normal path. Legacy normalization remains
unchanged. The full native effect/resource/save/mission lifecycle owner passed
after this extraction.

Receipts are under `temp/xl-skybox-20261031`: `lightning-core-test.log`,
`lightning-linux.log`, `render-native-build.log` and
`native-runtime/controls.log`. All 44 clean public contracts passed in 15.69
seconds; the public registration guard requires the new lightning test.
Both automation catalogs, scoped formatting and diff checks passed.

## Emitter ownership and movement

The emitter comparison executes extracted `Animate`, `SetLife`, `Update`,
`Move` and `MoveForObject` functions. Windows and Linux pass 2,160 multi-bolt
ticks with changing owner positions, disabled intervals and reactivation.
It compares every node and staged draw position, source clocks, random visits,
bolt lifetime, direction and segment. All bolts animate before any lifetime
transition, and accepted updates reanchor even stationary or disabled owners.
Movement preserves the source's projected offsets and float rounding; staged
draw coordinates remain those captured before movement.

`runtime/lightning_objects.cpp` binds native inert type-20/id-1 objects without
allocating bolts or consuming randomness. It validates the supported authored
profile and segment before replacing existing ownership. Existing systems update
newest first, following the source data pool, then new systems are created,
including disabled ones. Signature reuse and deletion invalidate ownership.
The clock hook follows absolute SDL time and the source's 40-Hz tick accumulation;
paused frames do not advance emitters. Effects randomness is isolated from SIM.

The native effect lifecycle owner covers atomic binding, disabled creation,
creation-frame ordering, movement, pause, activation, malformed saved state,
signature reuse and deletion. Its actual save-trailer round trip preserves
activation and rebuilds transient bolts. Trailer schema 28 includes lightning
ownership; no legacy pre-release trailer migration is provided. The authored
profile requires no waypoint, so there is no moving-waypoint state to save.

Real wall-enable and automatic object-disable triggers reach the same owner.
The updated Android save schema passed the original Pumo controlled route and
hub checkpoint regression, 64/64 steps in 370.683 seconds, with protected file
hashes restored. This covers existing levels through level 8, not lightning
rendering or earned traversal. Final optional-runtime/empty-owner clock guards
were rebuilt on Windows and Android and passed the native lifecycle owner;
the full Android route receipt precedes those two isolation guards.

This evidence does not qualify lightning pixels, light contributions, sound or
normal Pumo level-nine admission.

## Core, glow geometry and primitive rendering

The pinned source extraction now executes `ComputeCore`, `ComputeAvgDist`,
`ComputeGlow`, `ComputeDistScale` and `Draw`. The geometry comparison covers
288 node/depth/billboard/layer/endpoint/UV cases. Eighteen draw cases compare
visibility, three ordered color-random samples, unchanged alpha, blur color
scaling and GL_MAX/GL_FUNC_ADD transitions. These run in the existing lightning
contract target, with explicit boundaries for GPU calls.

The new isolated `LightningGpu` backend consumes owned geometry, an explicit
camera matrix and a caller-owned original corona texture. It renders both glow
layers and the core, preserves caller draw state and provides explicit resource
cleanup/context abandonment. It is noncopyable and does not own mission state.
The existing EGL owner compares complete output against source-style fixed
function client arrays and draw calls: 84 combinations of seven projections,
three widths, additive/maximum blending and visible/occluded scene depth. All
channels agree within one byte and scene depth is unchanged. Immediate-mode
reference drawing is deliberately avoided because driver batching changes the
quad decomposition relative to the source's separate draw-array calls.

The comparison uses Mesa 25.2.8 llvmpipe. Its draw-array quad path uses the 0-2
diagonal until a layer needs clipping, then the alternate 1-3 diagonal for that
layer. The backend preserves this reference behavior and delegates triangle
clipping to GL. This is measured reference-driver parity, not proof that every
legacy GL driver decomposes quads identically. Driver source inspected:
https://raw.githubusercontent.com/chaotic-cx/mesa-mirror/mesa-25.2.8/src/gallium/drivers/llvmpipe/lp_setup_vbuf.c

The active pinned `glow.cpp` blur vertex/fragment shader is extracted with a
verified file hash. The portable varying-based fragment shader produces identical
pixels for three brightness profiles, comparing every image in six successive
horizontal/vertical passes. The test includes the source's initial replacement
passes and later additive passes and confirms an impulse spreads to neighbors.
Existing particle, mixed-transparency, depth-format/MSAA and context-recreation
GPU checks also pass in the same owner.

This chunk does not connect rendering to native lightning objects or the shared
transparency queue. Offscreen accumulation/composition, corona resource lookup,
Android line rendering, context integration, spatial light and sound still need
implementation and validation. No admission gate is relaxed and no new original
mission route is claimed. In particular, desktop GL line smoothing is not an
Android qualification. The prior original-Pumo route through level 8 is reused;
there is no reason to repeat that route for these unconnected rendering helpers.

## Offscreen glow composition

`LightningGlowGpu` accumulates compatible bolts in one depth-backed color target,
then performs the source's six blur passes and additive composition at the batch
boundary. Initial horizontal/vertical passes replace, later pairs add; radii
3, 6 and 9 expand the drawing rectangle without changing shader offsets. The
source brightness is 1.05 and the original unblurred layer is not added again.
A compatible `begin` retains accumulated pixels. Each draw returns to caller
state, equivalent to source `Done`; only `end` performs the blur. The depth
snapshot belongs to the caller and is detached at the end of the batch.

The existing EGL owner executes extracted, hash-verified `CGlowRenderer::Render`
and `CGlowRenderer::End`, with only engine/GL boundary adapters. Twenty-four
complete-pixel comparisons cover single/two-bolt accumulation, visible/occluded
depth, full/interior/outside-edge rectangles and an offset viewport. All channels
agree within one byte, and original scene depth is unchanged. Primitive inputs
use the separately qualified backend on both sides so this comparison isolates
composition, rather than claiming another independent geometry oracle.

Additional controls verify compatible Begin retention, offscreen rejection,
empty and repeated End, repeated discard, and restoration of program, vertex
array/buffer, unpack buffer, read/draw targets, viewport, scissor/stencil, color
mask/clear color, active texture/bindings, blend, culling, depth and line state.
The source test harness restores its fixed-function state before returning to
Redux's cached GL state. Existing real context-recreation particle controls and
all GPU regression owners pass afterward.

The compositor consumes caller-supplied projected bounds and a validated depth
snapshot. Native ownership of those inputs, queue flush boundaries, original
corona artwork, Android rendering/context behavior, light and audio remain
pending. This component evidence does not qualify the original level 9 mission.

## Native corona ownership and frame snapshots

Prepared level resources now retain the original lightning corona through the
existing addon image/texture owner. Lookup checks the selected mission's mod
path first, then its default image or trusted startup installation. Missing,
malformed and foreign-shadowed images fail preparation. Corona images do not
inherit particle-atlas layout restrictions. The texture exposes its existing
lazy upload handle, sharing the established context-loss and decoded-pixel
lifetime instead of creating another image registry.

The existing resource lifecycle controls now run the same ownership, precedence,
case-insensitive lookup, rejection and retained-storage scenarios for corona.
The original `corona.tga` from the local XL distribution was also passed to the
existing pinned CTGA oracle: all 262,144 RGBA bytes of its 256-by-256 image match.
The original file remains private and is not committed; its SHA256 is
`aaf4a8ca34879ef0ab79fdbca295ee2a426755aa97be05bf538900e77ed81f57`.

Native emitter updates now capture owned core/glow geometry at the source
RenderSetup boundary: after each active bolt animates, before any lifetime
transition or movement. Camera-dependent billboards and average depth therefore
remain snapshots instead of being recomputed during queued drawing. Creation,
disabled intervals, pause, deletion and restore follow the existing owner
lifetime; restore discards camera geometry along with transient bolts.
The render-setup callback leaves the existing source simulation comparisons
unchanged. The depth range is 4,999: pinned XL initializes zFar to 5,000 in
`ogl_lib.cpp`, matching Redux's current fixed perspective far plane.

This chunk prepares the resources and frame data for queue integration. It does
not yet draw native lightning, qualify Android core lines or implement light and
sound. The normal level-nine admission gate remains unchanged.

Validation for the resource/snapshot chunk: final Windows and Linux source
comparisons pass, including all existing simulation and geometry cases. The
native lifecycle owner passes its new snapshot assertions and all four addon
resource profiles. The desktop EGL owner passes after the texture-handle
extraction, including real context recreation. Both automation catalogs and
scoped formatting passed. The previously qualified original route through level
8 is reused; these checks do not constitute a level-nine Android playthrough.
The Android x86_64 debug APK also builds successfully; it is not yet a lightning rendering qualification.

## Native drawing and Android renderer controls

Native transparency now collects immutable lightning frames after particles,
records owner visits independently of transparent-object classification, orders
bolts by the farther endpoint, and checks native signatures again at dispatch.
Rendering transforms captured world geometry with the current camera and uses
an owned scene-depth snapshot. The existing particle-image context hook also
releases lightning GPU resources. Normal lightning admission remains blocked.

The source's CFloatVector viewport overload accumulates bounds without resetting
them between bolts. Draw rejects negative viewport results, but this overload
returns only zero or one: even initially hidden input must remain in the glow
buffer if a later member makes the batch visible. The compositor now preserves
both those behaviors. Thirty-six pinned End/Render comparisons cover full and
offset canvases, depth rejection, hidden-first and hidden-later members.

Android's test emulator lacks texture border clamp. The corona therefore owns a
portable padded texture alongside its normal image. Each original mip occupies
[N,2N) in a transparent 4N square, through the original final 1x1 level. The
sampler maps UVs to uv/4+1/4 and retains implicit LOD selection. An integer texel
fetch shader copies the original GPU mip colors; it avoids an observed emulator
failure attaching automatically generated source mip images to a framebuffer.
No source artwork or replacement mip-generation algorithm is introduced.

This storage currently requires a square power-of-two image fitting within one
quarter of the GPU's maximum texture dimension. The original 256x256 corona
fits and consumes about 5.6 MB of padded GPU storage. Texture destruction and
real context recreation release/rebuild the owned padded allocation. Source UVs
are checked against their generated [0,1] domain.

The expanded Mesa comparison owner checks 288 complete images plus 576 isolated
glow-layer images using base linear, nearest-mip and trilinear filtering, including
minification. Isolated layers and the GL_MAX blurred profile retain the one-unit
RGBA8 tolerance. Two additive layers can accumulate two independent rounding
units; their measured maximum is two. The earlier single-unit complete-image
bound was insufficient for that additive profile and is not claimed here.
Existing particle, mixed-transparency, depth and upload regressions still pass.
The production native renderer also passes real EGL recreation, identical
post-recreation pixels, offset-canvas and depth checks, and padded-texture cleanup.

The existing Android POG runner adds an explicitly renderer-only mode:
`-OriginalHub -LightningGpu -CoronaImage <original-corona.tga>`. Its owned script
is `test_xl_lightning_gpu.jsonc`. The original corona is resolved by the normal
resource loader while the already-supported hub is paused; a controlled frame
is rendered and compared byte-for-byte after real EGL context recreation.
The host-GPU emulator passed all 28 steps in 74.75 seconds, changed 134 pixels,
and retained identical protected app-data hash lists. Its reported line-width
range is 1 through 10. This is not a native level-nine scene or earned traversal.

Scoped formatting, both automation catalogs, Windows native compilation and
Android x86_64 compilation passed. Evidence is under temp/xl-skybox-20261031:
lightning-hidden-batch-linux.log and lightning-texel-gpu-android.log; durable
Android receipts are in temp/xl-lightning-gpu-android. The hidden-first correction
has desktop comparison coverage; the visible Android/context result is reused.

Remaining gates include source projection/visibility/order qualification,
zero-distance-scale behavior, Android wide/smooth line fidelity, actual original
level-nine scene integration, light and audio. Neither this renderer control nor
the existing controlled route through level8 qualifies those missing behaviors.

## Zero-distance-scale drawing

Pinned Draw short-circuits SetupGlow when ComputeDistScale returns zero,
including floating-point underflow near the far depth range. It still consumes
three color samples, applies the blur profile's quarter color and GL_MAX, and
draws the core at authored width directly into the current scene. It neither
starts nor flushes a glow batch. If a batch already exists, its viewport bounds
still include this bolt; a later positive-scale Begin retains that batch.

The native renderer now follows that path without loading a corona or capturing
a new depth snapshot. The primitive skips both glow layers at zero scale, and
the compositor can accumulate bounds without redirecting the core into its
private color buffer. Normal positive-scale drawing retains the prior behavior.

The pinned Draw owner passes54 visibility/color/RNG/blend cases, including exact
zero and underflow distances. The GPU owner passes1152 primitive comparisons
(including core-only pixels) and48 batch comparisons, covering zero-only,
zero-before-glow and glow-before-zero sequences. Native scene checks require
zero-scale pixels before batch completion and identical pixels after real EGL
context recreation and explicit release. Windows compilation passes. Logs:
lightning-distance-linux.log, lightning-distance-scene-linux.log and
lightning-distance-windows.log under temp/xl-skybox-20261031.

This closes the zero-scale rendering gap for the supported positive glow/blur
profile. Android wide/smooth line fidelity, projection/visibility/order,
original level-nine integration, light and audio remain unqualified.

## Source viewport projection

Glow bounds now use the pinned ProjectPoint convention: positive camera Z,
projection-matrix 3x3 multiplication, absolute camera depth, half-canvas scale
and integer truncation. Vertex drawing still uses the normal GL MVP. The prior
bounds calculation used the drawing MVP and negative GL camera Z, which can
change bounds for asymmetric projections or translated matrices. Android exposes
its existing projection stack through a read-only shim accessor.

The existing lightning source owner extracts hash-pinned ProjectPoint, matrix
Flip, three-vector multiplication and Dot bodies. All1458 finite front/behind,
near-plane, aspect-ratio and canvas-size cases agree exactly;1428 distinguish
the previous calculation. Zero depth, nonfinite results and out-of-integer-range
results reject explicitly; source undefined conversions are not emulated.
This qualifies projection of supplied camera points, not every world-camera
transform or original-level visibility/order decision.

Windows native and Android x86_64 builds pass. The complete existing GPU owner
passes1152 primitive and48 compositor cases plus native context and particle
regressions. The Android original-corona control passes28/28 steps in52.512s,
including exact pixels after real context recreation and identical protected
app-data hashes. Logs are lightning-projection-linux.log,
lightning-projection-device.log and lightning-projection-android.log under
temp/xl-skybox-20261031. Catalogs from the prior chunk remain applicable because
registration, runner and automation scripts are unchanged. No level9 admission.

## Source visibility selection

The native collector now calls a shared lightning visibility predicate whose
behavior is compared with extracted, hash-pinned CLightning::MayBeVisible and
CFixVector::Avg in the existing particle visibility owner. All147456 cases pass:
owner absent/unrendered/rendered, start-segment visibility, endpoint and midpoint
lookup success/failure, every eight-segment visibility mask, three source lengths,
and exact ordered lookup positions. The existing source scratch initialization,
flag rollover and transparent/blocked adjacency controls remain active.

This confirms the current single-thread policy rather than changing which bolts
are selected. Lightning and particle emitters use visibility[nThread+2]; its
local visible flags remain clear and only global early-visible queries succeed.
For nThread0, source endpoint/midpoint FindSegByPos calls use start=-1,
exhaustive=1,skybox=0 and reach the already-qualified exhaustive/grid lookup.
Midpoints truncate (start+end)/2 as source does; production widens the sum to
avoid undefined signed overflow. Multiple source render threads are not claimed.

The reference owner and rebuilt native lifecycle integration pass, including
copied render frames, disabled/stale owners, signature recycling and no collection
RNG consumption. Existing original-Pumo admission rejection remains unchanged.
Evidence: temp/xl-particles-20261030/contracts-linux/effect-decode/reference.log,
temp/xl-skybox-20261031/native-runtime/controls.log and
lightning-visibility-reference.log. Catalog registrations and Android scripts
are unchanged; the preceding successful catalog checks remain applicable.

## Source lightning depth ordering

Native collection now uses a shared endpoint-depth selector compared with the
actual pinned AddLightning, Depth, Add and RenderBuffer bodies. All4608 endpoint
admission decisions and2938 mixed lightning/particle queue entries agree, across
zero/small/large depth limits, two viewers, reversed endpoints, equal distances,
visible-near/over-limit-far bolts and randomized positions. Existing5733 mixed
transparency entries remain unchanged. This preserves current behavior: reject
when the nearer endpoint is beyond the limit, otherwise use the farther endpoint
(start on ties), zero bias and depth clamping; equal buckets prepend entries.

The supported native scene uses zMin0 and no shadow-map pass, matching this source
profile. Checked world subtraction and distance overflow continue to reject
outside the defined fixed-point range. This qualifies depth selection/insertion
for supplied collection order; it does not certify every effect-manager traversal
or sprite-specific glow transition. Source negative-additive sprites can retain
a glow batch and remain outside the current broad native-object flush claim.

The registered effect reference owner passes in2.51s and Windows native compilation
passes. Existing native ownership/collection controls and unchanged automation
catalogs are reused. Evidence: lightning-order-linux.log and
contracts-linux/effect-decode/reference.log under temp/xl-skybox-20261031 and
temp/xl-particles-20261030 respectively. Normal lightning admission stays blocked.
