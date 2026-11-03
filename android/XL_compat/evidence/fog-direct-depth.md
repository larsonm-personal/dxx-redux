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
- Original Pumo entry is undergoing separate qualification. An opening-script
  attempt dismissed a load warning and did not reach gameplay; its logs are
  retained. Neither original-mission acceptance nor phone acceptance is implied
  by the derived fog controls.

Earlier rejected-driver results in [fog.md](fog.md) are historical. The legacy
SwiftShader backend now takes the tested direct path, so it is no longer an
expected rejection target. The rejection runner remains for a device where both
supported numeric depth paths fail.
