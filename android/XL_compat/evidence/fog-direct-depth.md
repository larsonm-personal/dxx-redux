# Direct scene depth for XL fog

The reported Adreno 830 rejects the numeric depth-transfer check. The owned
legacy SwiftShader emulator reproduces that failure: depth blits leave the
destination unchanged, while direct texture clears and raster writes work.
The renderer now selects a directly rendered depth texture only after both
numeric controls pass. It still rejects unusable depth and float blending.
No mission or GPU-name exception changes fog admission.

The fallback owns single-sampled depth24 and scene color, preserves the incoming
viewport color, renders normal geometry/objects into that target, and presents
the composited scene to the caller. Depth is detached while sampled by the
color-only compositor to avoid framebuffer feedback. Context restoration selects
the capture path before geometry. Full-size color/depth allocations are checked
before level admission. Stock scenes do not enter this path.

The selected presentation target may remain 2x/4x MSAA, but the fallback scene
itself is single-sampled. `scene_probe.samples` and `presentation_samples` expose
that distinction. This is a rendering limitation, not equivalent scene MSAA.
Physical ARM rendering and performance remain unverified.

## Indexed replacement walls

The new normal-frame tests exposed a separate latent problem: all seven indexed
Neptune POG replacements declare transparent headers but contain neither palette
key 255 nor 254. Keeping those flags queued opaque walls into the transparent
pass, which does not write depth. Fog consequently appeared through a closed door.

Pinned XL `gameio/loadtextures.cpp::LoadReplacementBitmaps` expands indexed
pixels and calls `CBitmap::SetPalette` (`2d/bitmap.cpp`). That operation recounts
key pixels and updates transparency; `SetSuperTransparent` updates wall texture
bitmaps. Prepared resources now carry those counts, and installation applies
the same key decisions. RLE control bytes are not counted as pixels. Encoded
payloads, mission content, unrelated flags, and resource ownership remain intact.

## Evidence so far

- Native Mesa: preferred blit and forced direct paths pass authored/zero-distance
  controls, rasterized geometry/object occlusion, projected volume geometry,
  state restoration, resource retirement/rebuild, and inactive stock checks.
- Indexed POG contract cases pass stale/missing header flags, raw key pixels,
  escaped RLE keys, run-control exclusion, and malformed-input rejection.
- The XL-enabled Windows D2 engine builds with the indexed installation change.
- Android normal fog run `a95e7b3f25b748118f766c578e13c3db`: **208/208 PASS**,
  142032 ms, owned `DxxPhoneFog20261102` / `emulator-5556`, SwiftShader.
  Includes real closed-wall occlusion, visible fog, disk/rewind, all seven derived
  levels, 2x/4x presentation, context loss, stock transitions and restoration.
  All 14 protected files match after restoration.
- Exact tested x86_64 source: `6facb4d51e91208cfc3034709121232054aa24d5`,
  version code 29030; both native library IDs match. APK SHA-256:
  `e8c9c83bf1b8f070ea74974f5b0e616623fa6825fa68f81bf60bae8e81203c55`.
  Receipts: `temp/xl-phone-fog-20261102/fog-208-pass` and
  `samples-apk-identity.json`. This diagnostic APK is not an ARM phone release.
- The initial original-Pumo warning was a separate soft-particle depth-transfer
  guard. The focused diagnostic retained its exact text. Soft particles now
  qualify the shared direct path and borrow its texture only for color-only
  draws, detaching depth/stencil references during sampling and restoring them.
  Lightning glow borrows the same captured depth for occlusion. Context restore
  reuses pre-geometry qualification instead of resizing the live scene.
- Universal candidate `4abb2b47dab5915d898959089fed8b826d669372`, version 29080,
  has matching IDs in all six ARM64/ARMv7/x86_64 D1/D2 libraries. APK SHA-256:
  `5d535b74c71de7a416a0f7f0555fb40e51215f59b69bd94796fce33f149f7788`.
- Exact candidate: original hub normal door/PMC exit activation repeated after
  save restore, 36/36 PASS, run `96dd3de0301f4ea6a0c2e85c2ac61c3e`, 83164ms.
  Fixed local approaches, no controlled exit dispatch and no human traversal claim.
- Exact candidate fog suite: 208/208 PASS, run
  `706382a8d1f94666805525f14b44e116`, 160083ms. Both protected runs restore all
  14 files exactly. Bundled installer also preserves those 14 files and the
  installed APK hash matches. The later Samsung test rejected this candidate; see below.
- Native production GPU checks include 18 copied/direct depth and stencil cases
  with analytical soft fading, unchanged scene depth and restored attachments,
  plus existing lightning and camera controls. Windows rendered build passes.

Earlier rejected-driver results in [fog.md](fog.md) are historical. The legacy
SwiftShader backend now takes the tested direct path, so it is no longer an
expected rejection target. The rejection runner remains for a device where both
supported numeric depth paths fail.

After packaging, the same universal APK additionally passed the original PMC1/2
renderer/lifecycle owner: 117/117 steps, 122482ms, including cameras, controls,
laser contacts, monitor draws, restart/save and background/context restoration.
This uses controlled exits and fixed firing poses; it is not earned campaign
progression. Receipts are supplementary in
`temp/xl-phone-fog-20261102/pmc-4abb2b4-pass`; the delivered ZIP remains unchanged.

## Samsung D16 precision failure and D24 qualification correction

The user confirms version 29080 rejects original pm-hub.rl2 on Samsung SM-S931U /
Adreno 830. The warning is clipped. Explicitly authorized read-only ADB logs show,
on two launches, D24 transfer = 0.625, D16 transfer = 0.624511719, and D16 direct
sampling = 0.624511719, all against expected 0.625. The phone runs Android 17 SDK37,
OpenGL ES 3.2 driver V@0800.64.8. This is a numeric format failure, not evidence
that the phone shares SwiftShader's unchanged-copy behavior.

The EGL surface requests 16-bit depth, so D24 copy success alone cannot qualify
ordinary capture. Source 18d06ec392adf8a3c1b00a8694907904bb4b3e6e tests both copy
formats, then qualifies the actual D24 direct texture if either source format
fails. It no longer requires unused D16 direct rendering. Clear sampling at
0.625 and actual raster sampling at 0.5 retain the strict 0.0001 tolerance and
non-finite rejection. The diagnostic records direct_depth_bits, and failures
use short lines with actual/expected values. Phone rendering remains unverified
until the user tries this new candidate; there is no GPU-name exception.

Windows D2 builds. All four native GPU tests pass, including simulated D16-copy
failure with real D24 clear/raster checks and an explicit qualified-format
assertion. Existing normal geometry, depth/stencil, state restoration, MSAA
presentation and resource rebuild controls pass. Scoped quality and 19 CI
receipt/inventory tests pass. The source was committed before the universal
Android build. All six ARM64/ARMv7/x86_64 native IDs match version 29160; APK
SHA-256 b9b85aa50fb6e4a2e00362772c05788746e6441b99ea2e4cb5789c21ecaa6d7c.
Exact APK: hub 36/36 PASS, run 51d0725d353c42c0becbac338c6c21f6, 63518ms,
287 protected files restored. Fog 208/208 PASS, run
94eb94add1cb4579ac017195fec5a01b, 142842ms, 15 protected files restored.
The exact bundled installer also preserves all 15 protected files and the
installed APK hash matches. Rendered evidence was inspected. Universal build
succeeds in 2m29s. Bundle: temp/pumo-phone-depth24-playtest-20261102,
ZIP SHA-256 ee2de892f633cc4feaf1cc9ea9ffa085ac184d0c3df14b3528504925012a89d8.
The runtime archive is unchanged from the previous bundle. The user has been
asked for a short live session; no physical acceptance is implied.

Phone log evidence: temp/xl-phone-fog-20261102/samsung-debuglog-150904.txt and
samsung-debuglog-151108.txt. Physical phone access was read-only; no installation,
force-stop, clearing or settings changes were performed on it.

## RGB8 multisample color resolve on Samsung

Version 29160 passes the phone's D24 clear/raster checks, 0.625 and 0.5, and
opens the briefings. The first game scene then returns to the launcher with
OpenGL error1282 at direct scene color preservation. Read-only DXX logs identify
4x MSAA at 640x480 using RGB8 (8/8/8/0, 0x8051). The capture inferred RGBA8 from
red bits alone. [Khronos ES3 blit rules](https://github.com/KhronosGroup/OpenGL-Refpages/blob/main/es3.0/glBlitFramebuffer.xml)
require matching source/destination formats for multisample resolves.

Source 5455715e62b74fdd85ef38110514f758accf7674 shares the exact four-channel
format policy already used to allocate Android MSAA targets. Direct color now
matches RGB8/RGBA8/RGB565/RGB10_A2 rather than dropping alpha-channel information.
It logs the allocated format/dimensions. Original color, fog admission and MSAA
settings are preserved. The production GPU control now covers both RGB8/RGBA8,
both depth formats, and zero/2x/4x samples; it checks the actual capture format as
well as analytical pixels. All four native GPU targets pass; fog paths each
cover 360 authored and 360 zero-distance cases, 1080 pixels each, including 240
multisample cases. Windows D2 and scoped quality pass.

Universal APK version 29180 has six matching IDs, SHA-256
9fd0bb3fd6e3132093a1424ead465bbd27d5a9a2f9c536d693253a743dbf5c75;
build succeeds in 2m1s. Exact Android fog checks pass 208/208, run adeca555773b4bbaa4fe9f593fa8ae64,
177377ms; all 15 protected files restore. The GLES GPU receipt confirms 360 cases,
1080 pixels, 240 multisample cases and restored state. Normal hub passes 36/36,
run 69802325837647f592798e3dd6c743ef, 78172ms; all 287 protected files restore.
The exact installer preserves 15 files and its installed APK hash matches.
Rendered evidence was inspected. Bundle: temp/pumo-phone-color-playtest-20261102;
ZIP SHA-256 1413ba407d2e4fd0079c12c27aa85edca3d9df3d0c29c09e89056fb6f972a503.
Phone acceptance of this candidate is requested and remains pending.

Phone evidence: samsung-debuglog-153845.txt and samsung-depth24-msaa.txt under
temp/xl-phone-fog-20261102. This launch measured archive fingerprinting at
1.883804s and level initialization at 6.503040s; briefing dwell is excluded.
Those timings do not establish responsiveness or a playable phone session.
