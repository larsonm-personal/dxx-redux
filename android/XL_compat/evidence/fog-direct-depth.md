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
  installed APK hash matches. Physical Samsung validation remains pending.
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
