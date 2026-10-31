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
until ownership, rendering, glow/blur, audio, lighting and save consumers are
implemented and qualified.

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
