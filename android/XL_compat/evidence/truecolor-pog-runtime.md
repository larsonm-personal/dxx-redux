# Owned true-color POG runtime

Single-frame RGBA replacements now have an explicit native bitmap format and
an XL renderer path. They no longer enter the indexed installer identified in
the [earlier audit](truecolor-pog-admission.md). This is resource support, not
Pumo campaign admission: every original Pumo level retains independent gameplay
requirements.

## Source behavior and admission

The source revision remains 2576a98b5de310e3ac7c9777df2677d1627b8bae.
`tests/reference_pog.cmake` pins the consumed files and extracts the actual
POG dimension setup, TGA reader/property conversion, point-transparency function
and texture-merge shaders. Storage adapters do not replace those algorithms.

The bounded reader preserves indexed/RLE replacements and distinguishes the
source's RGBA flag, including its large-width height encoding. RGBA pixels
retain their dimensions and four channels. Purple (120, 88, 128) retains its
super-transparency identity with zero alpha; other zero-alpha pixels become
black. The source's MIN_OPACITY conversion, flag thresholds and weighted average
colors are reproduced. Parsing also retains frame masks, but runtime frame-strip
animation remains rejected with the bitmap index and required frame count.

Preparation owns normalized bytes before live replacement. Installation allocates
fresh storage, retains the previous registry descriptors and restores normal PIG
paging on cleanup. Duplicate indices follow source order, with the last entry
winning. Repeated installation does not depend on a changed source file or on
the lifetime of a previous installed buffer.

RGBA replacements of cockpit/gauge images remain rejected because those layout
consumers require separate support. GPU admission checks the renderer capability
and texture limits; real upload errors fail instead of producing a successful
resource receipt. Malformed bounds, indices, RLE/RGBA combinations and excessive
storage are rejected. These checks apply to XL resource preparation; classic
levels selected through the separate legacy loader are not newly qualified as
XL solely by an unclassified companion resource.

## Rendering and gameplay consumers

The production uploader retains RGBA storage without palette quantization or
power-of-two resampling, including mipmaps and a separate super-transparency
mask. Merged draws use the source RGB mixture and summed/clamped alpha, with
masked composition where needed. Redux's indexed composition remains its
existing alternate. The desktop alpha test retains source framebuffer precision;
Android uses the GLES shader threshold. Platform-specific low-alpha rounding
and other renderer/settings profiles are not claimed universally equivalent.

Actual FVI pixel decisions and destructible-texture hits now use source RGBA
key/zero-alpha semantics, overlay order and all four orientations. Partially
transparent color alone does not make a shoot-through hole. Side transparency
uses the normalized bitmap flags and XL cloak range. The optional source
auto-transparency setting is outside this increment's profile, as are unrelated
wall/material features. Existing XL collision and wall gates remain in place.

## Evidence

- Public parser/source tests cover indexed/RLE recovery, malformed input,
  source normalization, dimensions, frame masks, alpha thresholds and point
  transparency. Unsupported frame strips are exercised as explicit native
  admission failures, not discarded animation.
- The Linux EGL test links actual `ogl.c`, `oglprog.c` and the production RGBA
  uploader. Synthetic controls exercise mixed indexed/RGBA layers, key masks,
  all orientations, blending, alpha boundaries, non-square/non-power-of-two
  storage, 1024-square uploads, mipmaps and indexed draws before/after XL.
  Independent pinned XL shaders render the same inputs for comparison.
- All five original Pumo POGs (26 replacements) and Arena's one POG (nineteen)
  match source conversion. The source run reports 24,625 controls. All 45
  original replacements are installed in the native registry and uploaded
  completely to the desktop GPU. The original GPU run covers 810 cases and
  55,170,048 uploaded/compared pixels, with source shader output checked for
  each replacement used as both base and overlay. That total is not a campaign
  coverage percentage or 810 separate mission features.
- Native integration exercises 48 real FVI rays and 48 destructible-texture
  hits; malformed and unsupported replacements leave live state intact. Mixed
  indexed/RGBA ownership, duplicate precedence, changed-file reinstallation,
  disk restore through stock Counterstrike and ordinary PIG paging pass.
  The Windows x86 run additionally exposed two defects before publication:
  the engine's MSVC byte packing disagreed with the new C++ resource record,
  and late POG cleanup could overwrite a newly loaded PIG bank with old offsets.
  The resource ABI now has explicit four-byte fields and padding; bank changes
  retire the old replacement owner before reading new headers. Controls against
  unchanged alien1, water and groupa PIGs compare actual headers and paged bytes,
  and fail before attempting to page an unexpected offset. The complete Windows
  native/source run passes for all 45 original replacements after both fixes.
- The normal Android x86_64 binary passes all 58 reusable owner steps. The
  original Arena resource has nineteen exact GPU readbacks (1,196,032 bytes)
  after normal loading, rewind, disk restore, forced EGL context recreation
  and restoration through Counterstrike. At the controlled initial view,
  fourteen replacements had actual draw submissions. One context recreation
  is observed. The fixture has no super-mask textures, so the desktop synthetic
  controls carry that specific mask evidence. Android readback establishes
  texture storage and lifecycle; it is not a source comparison of every
  composited scene pixel. The runner restores its backed-up app files and
  verifies their hashes.

Original input hashes remain unchanged: Pumo HOG
6e3e723e549897188a8fdc19db8b04630de9d428d2b31f8e3f48af108915b043 and Arena HOG
adbfea019c1ac29ac922e15876c22ca6ce30eb02d598a98e6ef7dfdce2b2dd3e.
No private original is rewritten or stripped of a blocking feature. Pumo
resources are tested individually without bypassing its level admission.

The required public selection now includes `xl_pog_contract` and
`xl_pog_reference`, plus `xl_pog_gpu` on Linux: 42 Windows / 44 Linux tests.
The private fast selection adds `xl_pog_native` and requires its actual source
and GPU arguments: 100 tests. Arena is already present in private CI. Full
Pumo original controls additionally require `XL_PUMO_HOG`; they are local
evidence until the proposed original distribution is added to that asset store.

The final Linux owner passes all 100 required tests in 336.01 seconds, with
339.368 seconds for testing and original inspection. Its receipt retains sixteen
format witnesses, three modern source-level comparisons and no experimental
exceptions. Windows passes all 42 public tests and the complete original native
POG controls. The final Android owner completes 58/58 steps in 85.624 seconds.
Both automation catalogs, nineteen private-owner unit tests and scoped
formatting pass. Evidence is retained in `temp/xl-truecolor-pog-20261029`.

## Reproduction

Use the existing standalone CMake reference-source configuration for the public
POG tests. `XL_FOG_GPU_TESTS=ON` enables both fog and POG offscreen controls on
Linux. The private native test additionally needs `XL_ARENA_HOG`,
`XL_ARENA_DESCRIPTOR`, `XL_PREVIEW_LIFECYCLE` and `XL_RETAIL_DATA`. Set
`XL_PUMO_HOG` to the unchanged original HOG to require all five Pumo resources.
The CMake owner checks the original hashes before and after execution.

```text
ctest --test-dir <contracts-build> -R ^xl_pog_ --output-on-failure
pwsh -NoProfile -File android/tests/test_xl_pog_runtime.ps1 -Serial emulator-5554 -Hog <original-rtarena.hog> -Descriptor <original-rtarena.mn2>
```

The Android owner requires provisioned retail D2 and the existing `player`
profile. It uses controlled poses and engine events, with no autonomous
campaign traversal. Arena's experimental water profile remains explicit and
its wider visual/gameplay qualification is unchanged.
