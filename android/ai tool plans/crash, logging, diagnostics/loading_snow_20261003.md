# Samsung loading-screen snow

User observed fullscreen black-and-white noise during the paired co-op campaign.
It disappeared when gameplay began. Investigate both engines without changing
the separate outstanding-bugs file or the previous campaign's fixes.

## Plan

1. Correlate the saved Samsung logs and level-load/menu rendering paths
2. Add opt-in numerical framebuffer diagnostics before changing rendering
3. Reproduce on Samsung, compare Retroid, and identify which draw publishes noise
4. Correct the confirmed rendering defect and repeat both engine load paths
5. Run scoped quality/build/catalog checks as applicable and preserve evidence

## Initial hypotheses

- Loading boxes paint only their rectangle, while Android does not clear the
  default framebuffer after swap; newly acquired buffers may contain undefined pixels
- D2 clears before loading, but its clear is a draw operation and may inherit
  renderer state or be followed by another swap before a partial loading draw
- Menu bitmap/palette corruption or an incorrect framebuffer target remain
  alternatives until measured

Evidence root: `android/temp/loading-snow-20261003/`

## Findings

The standalone `show_boxed_message(..., 0)` renderer draws only its centered
rectangle and swaps the full screen. It bypasses the ordinary event loop's
`ogl_android_clear_window_backing` call. Android deliberately does not clear
after swapping. D1 invokes the loading message both from `LoadLevel` and from
`paging_touch_all`; a later loading flip cannot rely on a prior frame's pixels.
D2 has an earlier rectangle-based clear in its loader, but the standalone
message renderer itself has no complete-background guarantee.

Both devices report `EGL_SWAP_BEHAVIOR=12437` (`EGL_BUFFER_DESTROYED`). Buffer
contents after swapping are consequently undefined; retaining earlier pixels
is not guaranteed by EGL. See the [Khronos EGL buffer-preservation note](https://registry.khronos.org/EGL/specs/EGLTechNote0001.html).

This is a confirmed rendering defect and a likely explanation for the reported
snow disappearing at gameplay. The exact spontaneous black-and-white visual
was not recaptured, so attribution of the user's observation remains an
inference. Four ordinary paired runs (D1/D2 native rehost and Options restore
failure/rehost) passed. The initial 480 numerical frame summaries had no GL
read errors and no dense black-and-white noise in the sampled corner/center
regions. No screenshots were taken. Earlier Samsung logs also showed no
surface-generation recreation during this follow-up campaign's loads.

## Controlled reproduction and correction

The new `d1-loading-background` and `d2-loading-background` cases establish
co-op on the real devices, fill the window buffer with red/green/blue/white
quadrants, invoke the real standalone loading-message renderer, and sample
the four corners immediately before its swap. These are deliberately seeded
pixels, not a claim to reproduce the exact naturally occurring snow.

Before the fix, both engines failed on both devices: every corner retained its
exact seeded color. Readback and GL state restoration reported no errors. The
two paired cases took 33.67 and 34.03 seconds; both devices were inspected even
after the first failed assertion.

The Android-only correction calls the existing full-target GL clear before
drawing a standalone loading message in both engines. Messages rendered over
gameplay (`RenderFlag=1`) keep their existing behavior. The fix avoids relying
on palette, alpha-blended rectangle drawing, or discarded buffer contents for
the background. It does not change networking or game assets.

The corrected APK hash is
`75f532d6ed391374c83568e3ed363d13887fa575a17bdac2f28b56387d046992`.
Both engines built successfully. The first fixed-run attempt hit an ADB
`run-as cat introspect.json` timeout during Retroid startup before reaching
the regression; it is retained separately as infrastructure evidence. An
unchanged-APK retry passed both loading-background cases and both native
rehost cases. Across the two engines and two phones, all eight successive
loading draws sampled opaque black at every corner, with no GL errors and
successful readback-state restoration. Co-op traffic and controls remained
responsive after the controlled draws.

## Validation

| Paired run                                         | D1                    | D2                    |
| -------------------------------------------------- | --------------------- | --------------------- |
| Before-fix ordinary native rehost, sampler enabled | PASS, 63.73s          | PASS, 66.47s          |
| Before-fix restore/Options/rehost, sampler enabled | PASS, 173.03s         | PASS, 197.22s         |
| Before-fix seeded background regression            | Expected FAIL, 33.67s | Expected FAIL, 34.03s |
| Fixed seeded background regression                 | PASS, 41.80s          | PASS, 44.11s          |
| Fixed ordinary native rehost, sampler enabled      | PASS, 61.38s          | PASS, 65.28s          |
| Fixed restore/Options/rehost, sampler disabled     | PASS, 165.53s         | PASS, 182.34s         |

The separate first fixed-run startup timeout took 37.92s. It did not reach
the graphics assertion. Numerical pre/post evidence is normalized in
`background-regression-evidence.json` under the evidence root: four failed
before-fix samples and eight successful fixed samples.

Scoped code quality checks, the arm64 diagnostic build for both engines,
both automation catalog checks, and `git diff --check` passed. The corrected
diagnostic APK was installed and its hash verified on both physical devices.

All six paired post-fix cases passed. Across the complete investigation,
`result-index.json` records 13 attempts: ten passes, two deliberate pre-fix
regression failures, and the one startup ADB timeout. The sampler-disabled
passes also completed a save/restore cycle after recovering and rehosting.

Both diagnostic apps were stopped at completion. `cleanup.json` verifies
that neither diagnostic app process remains, the sampler marker is absent,
Wi-Fi remains connected, and the original 60s Retroid / 120s Samsung screen
timeouts remain in place. No normal-app data or security settings were changed.

## Reusable diagnostics

- `test_device_loading_background.jsonc`, owned by the paired campaign runner,
  checks two successive standalone loading draws and subsequent game liveness
- `loading_frame_background` is a debug automation action; the pre-swap probe
  logs expected/actual corner RGBA values and preserves GL readback state
- An optional private `files/loading-frame-probe` marker enables bounded
  numerical sampling during startup and loading; restart the diagnostic app
  after changing it. It records statistics, not images. The marker was removed
  from both devices before the final restore/Options/rehost validation
