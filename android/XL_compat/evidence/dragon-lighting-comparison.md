# Dragon Pit lighting comparison

This records calculation evidence, controlled native static-region comparisons
and Android lifecycle/render smoke checks for the opt-in imported-D1 pilot.
It does not certify full-scene visual parity, matched Android rendering, normal
mission completion or native-D1 support. Later sections qualify and supersede
the earlier comparison hypotheses as their controls were established.

The original `drgpit.hog` SHA256 is
`a8eebb7e83faa55e143a48ec87c82407d8ffcaef3ad6b8f9329b84a6a42b262e`.
Its loaded geometry has 632 segments, 1,737 vertices and 252 geometry lights.
The source reference is D2X-XL commit
`2576a98b5de310e3ac7c9777df2677d1627b8bae`.

## Reference limitations and controlled correction

The pinned executable SHA256 is
`77db2a39a2293b10554986eeb0c6a4b664621cfc9839bf5686ad9bd402505e18`.
Its `ComputeNearestLights` clears the vertex-visibility buffer before allocating
it; `CArray<uint8_t>::Create` then allocates uninitialized bytes. Visibility
construction ORs bits into those bytes. Repeated cold runs varied, and vertex 8
received light through a connection rejected by the reference's own route table.

A separate diagnostic binary re-enters the existing clearing stage immediately
after allocation. It clears vertex visibility and redundantly clears light
visibility before construction. The original binary remains unchanged.
At RVA `0x15b7a7`, bytes `e9d5fbffff0f1f4000` become
`31f6e99efbffff9090`. Diagnostic SHA256:
`24e5b2a202ffb1b1020b19802df00b63dd9b47377d4b206ac8d30ce89d82c325`.
Two cold single-threaded diagnostic runs produced identical source records,
nearest-light lists and RGB values. Vertex 8 then matched the port exactly.
This is explicit divergence from undefined reference behavior, not an assertion
of exact equivalence to the unmodified binary.

## Matched profile and result

The reference uses zoom `0x9000`, a 1024-square projection with XY diagonal
1.7652469, and square camera aspect. However, it restores the active screen and
canvas before visibility calculation. A diagnostic pause immediately before
nearest-vertex selection measured canvas half-extents 1440 by 900.

The native audit now accepts paired `-package-audit-canvas-width` and
`-package-audit-canvas-height` inputs and records them. Overrides cannot be used
with `-package-audit-check-ownership`, which checks the default runtime profile.
The comparison used 2880 by 1800, matching the reference canvas. Production
precomputation still uses a deterministic 1024-square virtual canvas; no desktop
resolution or mission-specific exception was added to runtime behavior.

With matched canvas settings:

- All 252 fixed-point source centers agree exactly
- All 1,737 ordered nearest-light lists agree exactly
- All 1,737 RGB triples agree within 0.0000002; maximum component difference is
  0.0000001909240722
- No RGB difference exceeds the existing 0.00001 comparison tolerance

Using mismatched canvases produced four color differences and six nearest-list
differences. Those results must not be described as remaining implementation
defects. Pixel rounding makes the precomputation viewport part of the profile.

Native build, scoped formatting and the default-profile mission ownership,
destruction and save/restore integration test passed after adding audit controls
(28.40 seconds for the integration test).

## Artifacts and remaining gates

Local artifacts are under `temp/xl-dragon-reference-20261024`: the original and
diagnostic captures, `diagnostic-clear/manifest.json`, `diagnostic-clear/patch.py`,
`reference-preselection-snapshot.json`, `native-matched-canvas.json`, and
`comparison-matched-canvas.json`. The diagnostic snapshot binary deliberately
stops before nearest-light selection; it is not a playable reference build.

The deterministic square profile was also compared directly: a separate reference
user profile selects custom 1024 by 1024 resolution. The pre-selection snapshot
measures half-extents 512 by 512, and the completed diagnostic reference matches
all native nearest lists with the same maximum RGB difference, 0.0000001909240722.
Artifacts are `reference-square-preselection.json`, `reference-square-complete.json`
and `comparison-square.json`. Both owned reference processes were stopped.

The Android x86_64 pilot was rebuilt after the zoom and fixed-coordinate changes.
APK SHA256: `9a675f1ecf0eb65501e65d902b546aa55720d7787ba51e62b250b568d2cfd86e`.
On emulator-5554, all 35 lifecycle steps passed in 90.468 seconds: opt-in load,
save/restore, stock Counterstrike with no classic lighting state, and return to
the Dragon Pit save with 1,737 cached vertices and active renderer usage.
The final frame was inspected; captured logs contain no ANR or fatal exception.
The runner restored backed-up state byte-for-byte and stopped the app.
Artifacts are in `temp/xl-classic-android-20261024-geometry`.
The reusable explicit test is `android/tests/test_xl_classic_lighting_lifecycle.ps1`
with companion `test_xl_classic_lighting_lifecycle.jsonc`. It requires an installed
XL pilot APK, retail D1/D2 data, pilot `player`, and the imported original Dragon
Pit package. It preserves/restores app state, rejects incomplete automation or
Android errors, and records a frame for inspection. Run with an explicit target:

```powershell
pwsh android/tests/test_xl_classic_lighting_lifecycle.ps1 -Serial emulator-5554
```

The registered runner passed 35/35 steps in 99.298 seconds, restored state
byte-for-byte, and exited successfully. Its captured frame was inspected and
logcat contained no ANR or fatal exception. Artifacts are under
`android/temp/xl-classic-lighting-lifecycle`. Both automation catalog validators
and scoped formatting/lint passed. Registration is explicit because the test
requires private original mission assets and a pilot-enabled APK.

Next compare rendered scenes against the reference and complete the original
mission through normal input. Boss death timing and native-D1 parity retain their
separate gates. The browser must not promote full support based on these tables.

A visible reference capture is now available under
`temp/xl-rendered-reference-20261025`. The automatic screenshot diagnostic's
16 front-buffer captures were black and rejected. A separate, hashed diagnostic
captures the composed back buffer without swapping; its 20 captures contain the
mine, textures, cockpit and colored lighting. `backbuffer-manifest.json` documents
all changes, inherited visibility correction and executable hashes;
`capture-evidence.json` records frame hashes and limitations. Both owned reference
processes were stopped. This establishes a usable diagnostic image, not parity:
a current port capture with matched camera, viewport and render settings is still
required. The older 640x480 smoke image cannot serve as that comparison.

The current port was rebuilt and captured at 1024x1024 with square aspect in
`temp/xl-current-render-20261025`. Its `capture-evidence.json` records executable
and frame hashes, settings and limitations. Visual inspection shows the same
spawn scene and colored surfaces as the reference, but with a vertical scene
offset and different cockpit/HUD assets. Exact camera pose/frame timing and
filter equivalence are still unproven. Do not use this pair for a pixel-parity
claim until those controls are established. The isolated port process was stopped.

Camera telemetry now confirms an alignment limitation. In
`temp/xl-camera-profile-20261025`, the port capture uses a 1024x682 gameplay
canvas; the live reference reports viewport `[0,341,1024,683]`. Both use identity
orientation and zoom 36864, but their vertical camera positions differ. The
reference memory read is not synchronized to a screenshot. A still-camera visual
fixture and matched viewport are needed before per-pixel lighting comparison;
this must remain separate from ordinary-gameplay qualification.

A still-camera fixture now provides a matched spawn view in
`temp/xl-still-view-20261025`. Both engines report the same fixed position,
identity orientation, zoom and 1024-square gameplay viewport. Port TexFilt1 and
reference image-quality2 select matching GL filter modes. The port fixture is
explicitly `-xl-preview-still`; the reference uses a separately hashed diagnostic
that disables wiggle in addition to its documented visibility/readback changes.
Neither fixture is ordinary-gameplay evidence.

Five static surface regions were compared without resizing, registration or
color fitting. Mean absolute channel errors range from 0.654 to 8.699 on the
0-255 scale; the floor has the largest mean error. Regional average colors differ
by at most 0.548 per channel. Exact rectangles, maxima, percentiles and frame
hashes are in `region-comparison.json`. This is bounded measurement, not a pass
threshold or full rendering qualification. Texture/mipmap sampling and lighting
interpolation remain to be distinguished; animated objects and HUD are excluded.

Correction to "matched camera": pose, viewport and legacy zoom match, but source
inspection reveals different effective projections. Reference uses perspective
at `105 degrees * zoom`; the port uses 90-degree perspective with depth scaled
by zoom. At 0x9000 the respective XY factors are about 1.76525 and 1.77778.
The pixel results above therefore remain confounded by projection.
A nearest-filter control (`temp/xl-nearest-complete-20261025`) did not remove the
errors, ruling out mipmaps as a sufficient explanation. Next verify the live GL
matrix and match effective FOV in the visual fixture before judging lighting.

Effective projection is now checked against the live reference GL matrix:
XY=1.7652468681. An explicit still-preview zoom of 37126 gives port
XY=1.7652319129 (0.0008472% relative difference from fixed-point quantization).
In `temp/xl-projection-matched-20261025`, direct nearest-filter pixel errors drop
to 0.021-0.065 average channel levels in four of the five regions. The floor stays
at 9.408 and remains unresolved; it is stable across settled reference frames.
This isolates a surface-specific difference for texture/UV/light investigation.
It is not full rendering parity, Android validation, or mission completion.

The residual crop spans segment4 side3 (nonplanar texture22 floor) and a small
part of segment8 side0, rather than the spawn floor. Compiled-triangle ray samples
are recorded in `temp/xl-projection-matched-20261025/floor-rays-compiled.json`.
The port renders the floor with its compiled1-3 split; the reference's non-mesh
path emits a four-corner fan with0-2 split. Source and retained reference face
indices support this as a specific explanation to test. A controlled draw-path
comparison is still needed before attributing the entire residual to that cause.

That controlled comparison is now recorded in
`temp/xl-reference-fans-20261025/region-comparison.json`. With the same pose,
projection, nearest filtering and five unchanged rectangles, the explicit
`-xl-preview-still -xl-preview-reference-fans` draw path reduces the floor mean
absolute channel error from 9.407797619 to 0.055873016. The other four regional
errors are unchanged (0.021076389, 0.023047619, 0.065133333 and 0.037895833).
The camera sidecar records `reference_fan_diagnostic: true`. This isolates the
draw triangulation as the explanation for the measured floor discrepancy; it
does not establish full-scene equivalence or a new passing threshold.

Normal rendering retains compiled face splits. The reference's non-mesh fan
mode is a documented visual difference on warped faces. Collision geometry is
unchanged. The diagnostic requires the explicit still fixture and is excluded
from ordinary gameplay evidence. Other views, dynamic lighting, Android rendered
comparison and normal mission completion remain open. Both capture processes
were stopped by the runner, and the native preview build passed.

Android follow-up after the fan-diagnostic integration rebuilt and installed the
x86_64 pilot APK (SHA256
`da56d4a850575c06b0286533dfb4f72fc57192de4dfed7c0622bdcd35fa08d34`).
The existing state-preserving lighting lifecycle runner passed 35/35 steps in
95676 ms, run `4ec850d7d2b6456fb9262f34a242b023`, on emulator-5554. It verified
stock isolation and restoration of the original mission's 1737 lighting vertices.
The final screenshot in `android/temp/xl-classic-lighting-lifecycle/render.png`
(SHA256 `e59652b43bf06b52d8cbec5c1d1d233bf48a2acb42c55576c65b5b2d95789115`)
was visually inspected: mine surfaces, textures, colored illumination and the
cockpit are visible. The runner found no ANR or fatal exception, restored state
byte-for-byte and stopped the app.

This is Android lifecycle/render smoke evidence, not matched reference pixels.
Final introspection reports cockpit0, segment0, position
(-282.594543457,-65.982345581,-18.974761963), forward(0,0,1), and vertical
velocity1.784881592. It is a moving view with cockpit and touch overlays, unlike
the square still fixture. A frame-synchronized main-view camera record and
matched projection are required for quantitative Android comparison; raw global
render state may have been overwritten by an auxiliary or gameplay view.


Android now records the actual main-view pose, unscaled orientation, projection
scale, zoom and GL viewport before auxiliary views can overwrite those globals.
The record is linked to the sampled framebuffer request and cleared at each
flip; absent/mismatched views report invalid. The updated lifecycle test passed
36/36 (run `243e009380a8492096973ab7b915cc79`), including an invalid startup-menu
sample and valid gameplay/restored evidence. Final request IDs match at5, GL
error is0, viewport is `[0,360,1920,720]` inside1920x1080, zoom36864 and projection
scale `[24576,65536,36864]`. Evidence:
`temp/xl-camera-frame-20261025/camera-evidence.json`.

This closes the missing camera-metadata association for the tested sample,
not Android pixel parity: raw pixels from that exact sample are not exported
yet, and the later ADB screenshot is not the same frame. Post-game invalidation
and auxiliary-view stress coverage are also still required. The tested APK hash
is `328bdfaf7df41e5261fc872c92f4427c7622d51b326ccf89c1656a12a3881c43`.


The same-frame pixel export is now implemented and verified. Debug action
`capture_framebuffer=once` publishes a JSON header plus bottom-up RGBA8 payload
as one file, with pixel SHA256 and the main-view receipt. The38-step lifecycle
run `9b87f1f5971043deafd201da060eddaa` passed; both the PowerShell wrapper and an
independent Python decoder verified8294400bytes against SHA256
`8b1c463220a96e324d3e1a0e2f48e86b43e9b545da74bbc17d653bd8f6237597`.
The decoded PNG was visually inspected and contains the intended mine/cockpit
scene. See `temp/xl-frame-export-20261025/export-evidence.json` and
`captured-frame.png`. Camera and pixel request IDs are4/4, flip222, mission
activation5. Tested APK:
`d78384231ced3ef442d8b260688e8b6bf8d637b5000d603fbabd55c29d073e4e`.

This provides pixels tied to an actual camera, but not matched reference pixels:
pose `[-18520117,-3915860,-1243530]` and1920x720 viewport differ from the square
reference fixture. Post-game invalidation and auxiliary-view stress coverage
remain open. No support status changes follow from successful export.


Post-game invalidation and one rear-view configuration are now exercised by the
43-step lifecycle test. After a successful gameplay export, aborting to the menu
causes a fresh export request to fail with invalid main-view evidence. Restoring
the mission and enabling the left rear-view window produces a valid forward
main-camera export. The rear window was visually confirmed in the decoded frame.
Run `3eadc5febf10469c8205384bfc9782ba` passed43/43 plus host payload validation and
byte-exact state restoration. Evidence:
`temp/xl-frame-invalidation-20261025/invalidation-evidence.json`.
The wrapper validates the export's pixel_capture receipt; a newer asynchronous
introspection sample can legitimately have a different request ID and be invalid.
This does not cover every auxiliary view or establish reference pixel parity.


## Matched Android still-view witness

A controlled1024x1024 Android fixture now matches the reference position
`[-18520117,-4073563,-1243530]`, identity orientation, and effective projection
within the precomputed fixed-point relative difference8.472e-6. It explicitly
disables wiggle, uses full-screen view, nearest filtering and zoom37126, then
restores those controls. Capture metadata labels the diagnostic. Normal compiled
face splits remain active. Display and saved app settings are restored afterward.

Run `4c5760833fec4105aeed65b5d7398064` passed29/29 plus host payload validation;
APK `479ae821ac04d36dacd00504de73a23878f7a9623439e804e7d371c5d62d06ab`.
Export pixel SHA256:
`4ddcf5a00abb4ebc71eddda73d1745fad4b70d008e38b6e4f6a7a76bd064ea33`.
Evidence is in `temp/xl-android-still-20261025/comparison.json`; `android.png` was
visually inspected. The same predefined regions and hashed reference TGA are
compared directly, without resizing or color adjustment.

| Region | Mean absolute RGB channel error (0..255) | 95th percentile |
| --- | ---: | ---: |
| Rear red wall | 0.01034 | 0 |
| Left red wall | 0.01745 | 0 |
| Upper-left rock | 0.03500 | 0 |
| Middle grate | 0.01804 | 0 |
| Lower-left warped floor | 9.40272 | 30 |

The floor discrepancy remains consistent with the earlier normal-split native
comparison. Four close regions are not a full-scene fidelity claim. Additional
material/dynamic views, explicit treatment of the floor rendering difference,
normal mission completion and native-D1 parity remain unqualified.


## Floor rendering policy audit

The pinned reference at `2576a98b5de310e3ac7c9777df2677d1627b8bae` selects
its draw primitive by renderer profile. Clean source files
`gameio/createmesh.cpp:1532` and `ogl/ogl_fastrender.cpp:393` show that lighting
method0 selects no triangle mesh and four-corner fans. Nonzero lighting methods
select a triangle mesh; `SplitIn1or2Tris` preserves the side's0-2/1-3 split,
with separate four-way subdivision for eligible large quads. Consequently, the
fan snapshot is evidence for that specific reference profile, not a universal
XL geometry requirement. This does not dismiss its measured mismatch.

For the measured segment4 side3, the fan's0-2 diagonal and the compiled1-3
diagonal intersect in the x/z plane at approximately(-307.59454,21.02521).
Their interpolated y coordinates are-70.157637 and-72.157639: a2.000002-unit
surface difference. This is one sampled geometric gap, not a maximum or a
runtime collision measurement. It explains why changing triangulation changes
texture/light interpolation even when corner data agree.

The reference collision path iterates side face counts and delegates to side
face/normal queries (`physics/sphere_collision.cpp:1153`,
`include/segment.h:437`, `main/side.cpp:1119`), rather than selecting the GL draw
primitive. Keep collision semantics separate when implementing a visual profile.
Evidence, corner data and source hashes:
`temp/xl-floor-policy-20261025/geometry-evidence.json`.

Decision: retain compiled collision geometry and current normal drawing until a
named legacy-fan visual profile has source-aligned culling and oblique/grazing
view coverage. Do not enable the still diagnostic globally from one close crop,
or classify every classic warped side as an unsupported XL file feature. The
browser should distinguish format/physics support from renderer-profile fidelity.
The current pilot remains unqualified; this audit is not a passing exception.

## Face submission and hardware culling audit

The pinned reference's `render/renderlib.cpp:55` submits a split side when
either compiled normal faces the viewer. For a 1-3 split it measures the raw
viewer displacement from corner 3. `include/vecmat.h:1021` computes the dot
product in double precision, then truncates to fixed point. Our still-only fan
diagnostic instead inherits the quick-normalized displacement from corner 1
and an integer shifted dot product. These are not identical at grazing angles.

The reproducible arithmetic witness in
`temp/xl-face-culling-20261025/audit.py` uses the audited segment 4 side 3.
At fixed position `[-19830836,-4728924,2033268]`, one fixed-point unit below
the horizontal compiled face plane, reference dots are negative while the
diagnostic's first dot rounds to zero: reference rejects, diagnostic submits.
One world unit above/below gives the expected agreement. Source hashes and
all five cases are in `evidence.json`. This is a Python transcription, not an
execution of either renderer; the synthetic camera is not established as a
legal player position.

Submission is only the first stage: reference `BeginRenderFaces` enables face
culling (`render/fastrender.cpp:335`), with clockwise front faces configured in
`ogl/ogl_lib.cpp`. Redux likewise enables clockwise culling in `ogl_start_frame`
(`d2/arch/ogl/ogl.c:2267`). Therefore a submitted fan can still lose either
triangle to hardware culling. The arithmetic discrepancy alone does not prove
a visible hole or pixel mismatch. Do not remove hardware culling to compensate.

Before enabling a legacy-fan profile, implement its raw-displacement submission
rule explicitly and verify both submission and raster results at oblique views,
including a view where the two fan triangles have opposite facing signs.
Retain the existing compiled collision geometry and keep the profile diagnostic
until those runtime comparisons pass. No runtime renderer was changed here.

## Android oblique floor capture

Run `063b9a901791499895251c9c607d5f47` passed 29/29 steps (51455 ms)
using the same APK `479ae821ac04d36dacd00504de73a23878f7a9623439e804e7d371c5d62d06ab`.
The fixture moves the camera to segment 4 at fixed position
`[-19830834,-4073563,1377908]`, pitch 8192, bank/heading 0. The engine accepts
the position inside the mine; captured forward is `[0,-46340,46340]`.
This is diagnostic camera placement, not a normal-control traversal or proof
of sphere clearance. Zoom remains 37126 with a 1024x1024 viewport.

`temp/xl-android-oblique-20261025/render.frame` passes payload length, SHA256,
and frame-linked camera receipt checks. Pixel SHA256 is
`1052a33d491e688e1bbc4316d6741515c0c0f05e0562fe333b4c99644521fd7e`.
The decoded `captured-frame.png` was visually inspected: the textured floor
occupies most of the view with the adjoining red wall visible at upper left.
App configuration was restored byte-for-byte, display restored to 1080x1920
without an override, and the app stopped.

This establishes the Android half of a second view only. There is no paired
reference frame yet, no numerical parity result for this pose, and no proof
that the fan triangles have opposite facing signs here. Normal compiled
splits are still active. The matching reference camera and a mixed-facing
view remain necessary before changing the visual profile.

## Paired oblique reference and host controls

The matching reference capture is now available in
`temp/xl-reference-oblique-20261025`. `prepare.py` copies the reference game
directory and changes only player 0's start segment, position, orientation and
last position in the copied HOG. The manifest records original bytes, offsets,
replacement bytes and hashes; all geometry, lights and other members remain
unchanged. This is explicitly a visual fixture, not original mission gameplay.
The original archive is untouched.

The reference snapshot matches Android's fixed position and all nine orientation
components. Effective projection differs by the same precomputed 8.472e-6
relative amount. The snapshot is not atomically tied to the screenshot; the
last two reference screenshots are byte-identical in all compared regions.
Reference TGA SHA256:
`428beb6a5631c50854a46fa4520eca0e6fcec3b6f4f412b24a071e99599b186e`.

The existing host executable was run twice at the same modified start, with
and without `-xl-preview-reference-fans`. Camera, filtering and executable
hash are checked equal. Executable SHA256:
`1b3e3f0029c548d7e48f26e24c72308d3ec4a84c8b6895a4ba68fd93dea23566`.
Reference and host fan images were visually inspected. Exploratory crops
exclude the HUD/reticle; these are measurements, not newly invented acceptance
thresholds. No registration, rescaling or color adjustment is applied.

| Region | Android compiled | Host compiled | Host fan diagnostic |
| --- | ---: | ---: | ---: |
| Red wall | 0.00652 | 0.01126 | 0.01126 |
| Left floor | 10.05042 | 10.04803 | 0.09424 |
| Right floor | 8.00947 | 7.99420 | 0.07964 |

Values are mean absolute RGB channel error against reference (0..255).
`compare.py` retains rectangles, maxima, percentiles, image hashes and camera
checks in `comparison.json`. The same-host control supports triangulation as
the principal cause of this view's floor discrepancy, rather than a platform
difference. It does not establish all-angle culling parity, Android fan rendering,
or gameplay fidelity. Owned reference/host processes were stopped. Next:
exercise mixed-facing fan triangles and implement the scoped visual profile
with its explicit submission rule; normal rendering remains unchanged.

## Mixed-facing floor witness

Three isolated fixtures were retained rather than treating every capture as
valid coverage:

- `temp/xl-reference-mixed-facing-20261025`: reference moved the requested
  player start `[-19830836,-4532315,1050228]` to
  `[-19872688,-4375362,1029304]`; host retained it. The camera assertion failed.
  This is not a valid image comparison and does not by itself establish a
  gameplay collision defect.
- `temp/xl-reference-mixed-clearance-20261025`: a geometric clearance search
  selected `[-19503156,-4368693,214644]`, which both engines retained. At pitch
  8192 the back-facing triangle projects outside the frame, so close pixels
  here do not qualify the intended culling coverage.
- `temp/xl-reference-mixed-visible-20261025`: same position with identity
  orientation. Both engines retain the exact camera; the back-facing triangle
  now projects into the image. Both reference and host fan captures were
  visually inspected. The isolated fixture changes only the recorded player
  start fields; original mission assets and runtime source remain untouched.

For the final pose, normalized signed distances to fan planes 0-1-2 and 0-2-3
are approximately -0.46733 and +4.45156 world units. Thus their facing signs
differ. `compare.py` verifies camera agreement, records projected corners and
hashes, and measures a 2675-pixel projected back-facing triangle footprint
excluding the HUD/reticle: mean absolute RGB error 0.06704, maximum 12.
The upper-left, left-floor and right-floor crops have means 0.03291, 0.04624
and 0.05203; successive reference frames are identical in those crops.

This is a useful mixed-facing raster witness, not a GPU primitive trace or an
all-angle equivalence proof. Reference camera sampling remains separate from
its screenshot. Host uses the same executable hash as the oblique controls.
All owned processes stopped. With this bounded witness available, next work
can implement the explicit visual-profile submission rule and expose a
diagnostic Android comparison while preserving collision and hardware culling.
Normal gameplay completion and overall pilot qualification remain open.

## Android fan diagnostic implementation and oblique verification

The existing still-only host diagnostic now uses a dedicated split-face
submission helper in `runtime/start.cpp`: corner 3 for 1-3 splits, corner 0
for 0-2 splits, raw viewer displacement, and XL's truncating dot-product sign
rule. It deliberately retains the engine's compiled normals; this is not a
claim of identical normal generation or a quad visibility rewrite. Collision
and hardware culling are unchanged.

Android automation exposes `still_view_reference_fans=on/off` only within an
active D1 classic-lighting still-view fixture. The effective flag checks player
identity, level and mission generation, and is cleared by still-view restore
(including the existing success/failure/replacement cleanup paths). Captured
main-view metadata includes `diagnostic_reference_fans`. It is not a mission
admission flag or normal gameplay setting.

Scoped formatting passed. Android D1/D2 x86_64 and Windows renderer/headless
lifecycle targets built successfully. The Windows build tree has no registered
CTest tests; its build success is not a lifecycle execution result. APK SHA256:
`f17093512ee79a83014f3cc0c3e07f0b44f553af6f2a4b0e6885b632c21558c2`.

Run `ca5e0506bc3a4aa3a9e65bc9e71d353c` passed 30/30 (51911 ms), checking
the exact established oblique camera, fan marker on capture, and marker off
after explicit restoration. Export length, SHA256 and camera receipt passed;
decoded `android.png` was visually inspected. App state restored byte-for-byte,
display restored, app stopped. Evidence: `temp/xl-android-fan-profile-20261025`.
Pixel SHA256:
`95c8054d01505bb82936efbe53a57ed1c66eeb017f15d49904d7e3ced113a40d`.

Against the same oblique reference and rectangles, mean RGB errors are
0.00652 (red wall), 0.04292 (left floor), and 0.02610 (right floor), versus
the prior Android compiled values 0.00652, 10.05042 and 8.00947. These bounded
measurements verify the diagnostic on Android at this pose; they do not
promote normal rendering or overall mission support. Start and mixed-facing
Android captures, failure-cleanup coverage, and the eventual normal-use
profile selection remain outstanding.

## Android start and mixed-facing fan verification

Using the same APK `f17093512ee79a83014f3cc0c3e07f0b44f553af6f2a4b0e6885b632c21558c2`,
two further original-mission Android fixtures passed exact camera, fan marker,
frame export and explicit restore checks:

- Start: `5e135941912b4eb09dfd29202e84d5ad`, 30/30, 47689 ms.
  Pixel SHA256 `a7be2c0f59754a08b0f91acd3bfc342930aa006c62561f1b9f1d96733e0ea163`.
- Mixed-facing: `f88ff38c9b964a5aab6faffd408c6a38`, 30/30, 49674 ms.
  Pixel SHA256 `920bc00e6652ecf5b5cfc93cc3ff6d1feaa581603ceac8b59533f25c444fa650`.

Evidence and reusable comparison script:
`temp/xl-android-fan-views-20261025/{start,mixed}` and `compare.py`.
Both decoded images were visually inspected. Each wrapper verified byte-for-byte
app-state restoration and restored the display before the next run.

The same five start regions give mean channel errors 0.01034 (rear wall),
0.01745 (left wall), 0.03500 (rock), 0.01804 (grate), and 0.03024 (floor).
Only the floor changes from the previous compiled-split Android measurements.
For the established mixed-facing pose, region means are 0.01640 (upper left),
0.02082 (left floor), and 0.02044 (right floor). The same 2675-pixel projected
back-facing triangle footprint gives mean 0.03464, maximum 12. These use the
existing rectangles and mask, without fitting or new acceptance thresholds.

This extends Android diagnostic coverage to all three established poses.
It does not establish all-angle equivalence, normal gameplay, native-D1 parity,
or a package-driven production renderer selection. Reference snapshot/frame
timing limitations remain as documented above.

## Fan diagnostic failure cleanup

The same APK was tested with an intentionally invalid fan command after a
successful active-fan capture. Automation run
`c77c662da5624569822805968fbf8532` reports the expected FAIL at step 28/28
(45305 ms), with the exact invalid-value reason. The host wrapper treats only
that failure as expected; it then requests fresh introspection from the same
running process without replacing the script or restarting the game.

The later valid main-view receipt has a generation greater than the retained
active-fan pixel capture and reports both diagnostic flags false, zoom 36864,
and physics flags 75 (wiggle restored). Thus this tests the actual failure
cleanup path, rather than inferring it from explicit restore or process exit.
The wrapper passed export validation and restored saved app state byte-for-byte,
restored display size and stopped the app. Evidence:
`temp/xl-android-fan-views-20261025/failure/{result.json,cleanup-state.json,cleanup-verification.txt,restoration.txt}`.
The engine result intentionally remains FAIL; it is not relabeled as mission
success. Script-replacement cleanup is still supported by source inspection,
not separately exercised here.

## Prepared classic lighting selects the normal-use visual profile

`xl_classic_legacy_fans_active()` now derives the no-mesh visual profile from
successfully prepared classic ambient lighting, a live game window, D1 mission
format and single-player mode. No mission identifier is consulted. Ordinary
split-face drawing uses the verified fan submission path while this profile
is active. Collision remains compiled; GPU culling remains enabled. Android
merged-wall draw context is populated for the four-corner path as for other
normal draws. A lighting request without prepared companions does not activate
the profile. Classic lighting itself remains an experimental opt-in.

Frame receipts now report `visual_profile` (`legacy_fan` or `compiled_splits`)
separately from `diagnostic_still_view` and `diagnostic_reference_fans`.
Host camera receipts also report effective profile: diagnostic off can no longer
be used to infer compiled drawing when classic lighting is prepared. Earlier
compiled control images retain their recorded older executable hashes.

Scoped formatting, Android D1/D2 x86_64 and Windows renderer/headless builds
passed. Android APK tested:
`b035647a50e385d51fca9691d8ab92900147d244e3e82eb7551b2ec64a3e409e`.
The subsequent host-only camera-receipt addition was separately rebuilt on
Windows; the Android test exercises the already-built Android receipt path.

Extended lifecycle run `9f44518731784d458597f73260ed6ec6` passed 43/43
(119584 ms), with prepared-profile activation, quick-save/load, menu frame
invalidation, stock Counterstrike isolation, and pilot restore with auxiliary
rear view. Both saved frame assertions require `visual_profile=legacy_fan`
while both diagnostic flags are false. The final 1920x1080 export was decoded
and visually inspected; its receipt identifies the main forward view rather
than the auxiliary rear view. Pixel SHA256:
`c1ac1cde515d3b6b88606128a8e5484c158f73a685f28cea3db02f7f501ee3d8`.

Evidence copied to `temp/xl-classic-visual-profile-20261025`, including the
script log, result, frame, camera receipt and restoration hashes. The wrapper
restored app state byte-for-byte and stopped the app. This proves normal-use
profile lifecycle within the experimental path, not mission admission,
normal-control completion, all-angle fidelity or native-D1 parity.
