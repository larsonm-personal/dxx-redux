# Vertigo transparent cached wall diagnosis

## Objective and evidence

Identify the cause before changing rendering behavior. User reproduction:
Vertigo CD campaign, level 1, segment 288 side 3, base rock349 (texture 191),
overlay misc063 (334), orientation 0, WATER.256 / water.pig. Phone export
`debuglog_20260915_163154.txt`, tap around 19:39:46, frame 700.

Both source GPU images match decoded pixels. Base has no transparent pixels;
overlay has 1280 ordinary transparent pixels and no super-transparent pixels.
The 64x64 cached composite, slot 2 / handle 303 in this run, reads as 4096
transparent black pixels. Geometry is a solid boundary. This establishes a bad
composite at observation, not when or why it became bad. CPU source bytes absent
from this GPU-only bitmap are expected; their absence alone is not a failure.

## Prioritized hypotheses from code inspection

1. Merge creation inherits unsuitable graphics state. In
   `android_merged_wall_cached_texmerge_render_to_texture` in
   `android/app/src/main/cpp/shared/merged_wall_debug.c`, depth/blend/cull and color
   masks are controlled, but scissor and stencil are not explicitly isolated.
   A screen-space scissor rectangle can exclude a small off-screen target.
2. Incorrect vertex source interpretation. The merge supplies local CPU arrays
   to pointer functions without explicitly unbinding GL_ARRAY_BUFFER first.
   `gles3_shim_vertex_pointer` and companion setters in `gles3_shim.c` capture
   the current buffer binding. A nonzero binding can make CPU addresses be
   interpreted as offsets into a buffer. Verify the entry binding rather than
   assuming it is nonzero.
3. Source/sampler/program state or draw submission prevents valid output.
4. Composite is initially valid, then invalidated, overwritten, or reused under
   an inadequate cache key, especially across mission/palette resets.
5. Diagnostic readback is misleading. Confirm with direct framebuffer readback
   rather than relying exclusively on another shader-based sampling pass.

## Phase 1: one targeted diagnostic build and one phone reproduction

Arm before level load using source names plus palette, not transient bitmap or
GL handle numbers. Keep this diagnostic opt-in; do not read back every wall or
silently repair the result. Cover every creation of this pair, since a different
face can populate its cache before the user reaches segment 288.

- Record build/device/driver, selected mission owner/revision, level, palette,
  asset-reset generation and merge-cache generation.
- Give each cache entry a creation serial. Record creation frame, first face,
  orientation, slot, source/output handles, dimensions, source content hashes,
  source flags, and every invalidation/eviction affecting the entry.
- At merge entry and immediately before draw record actual GL framebuffer,
  attachment and completeness; viewport; scissor enable/box; stencil enable and
  relevant function/masks; rasterizer discard; depth/blend/cull/color masks;
  active texture and actual unit bindings; samplers/filter/mipmap state; actual
  versus tracked shader program, merge uniforms; VAO/buffer bindings and shim
  pointer/buffer interpretation for every vertex attribute.
- Record preexisting GL errors separately from errors after setup, draw and
  mipmap generation. Diagnostics must not attribute old errors to this draw.
- Read the output immediately after draw, before caching. For this pair its
  alpha should be fully opaque because the base is opaque and the overlay has
  no super-transparency. Record pixel hash, alpha-zero count, alpha range and a
  few pixel samples. Confirm level zero using direct glReadPixels on the merge
  framebuffer; restore read/pack and other state modified by instrumentation.
- Store the creation summary with the entry; extend `mwall tap` to emit it next
  to current readback and cache identity. Preserve the original entry until all
  observations are logged. Log via the exported TEXTURE category.

Initial phone procedure: fresh app process, start Vertigo directly, reach the
same wall, run the existing `mwall tap`, export log. No extra user commands
should be required for the first pass beyond enabling the targeted diagnostic.

## Phase 2: follow the evidence

| Observation | Next experiment |
| --- | --- |
| Output already blank at creation | Replay into a separate scratch target; change exactly one implicated GL state at a time |
| Creation correct, tap blank | Trace cache lifetime, handle reuse/deletion, context and palette generations; compare first reuse with tap |
| Direct readback correct, diagnostic sampling blank | Investigate sampler/mipmap/readback path before changing merge code |
| Composite correct but wall still absent | Inspect final wall draw, depth and alpha state; do not treat it as an asset-load problem |

For a bad creation, preserve original output and run bounded debug replays:
scissor disabled only, correct CPU-array buffer binding only, then other state
indicated by the captured evidence. Each replay uses separate output and restores
state. A fully normalized replay can establish a broad state dependency if the
individual tests fail, but is not sufficient evidence for a production fix.

Use a CPU composition of the same decoded source pixels, palette and orientation
as an independent reference. Compare alpha/coverage first, then color with
filtering and framebuffer orientation accounted for. A CPU fallback is a useful
diagnostic control, not the proposed permanent solution.

An explicit debug-only targeted eviction/rebuild at the same viewpoint can
separate bad first-use state from reproducible input failure. Log before eviction;
never let the first tap automatically erase the failing evidence.

## Phase 3: explain why Vertigo exposes it

Only expand after the first capture identifies a branch:

1. Automate the exact level/segment/viewpoint on the emulator using introspection
   and existing scripts. Host rendering alone cannot validate Android GLES.
2. Inspect retail level/HOG and water.pig using existing parsers; compare the
   source hashes with the phone. Locate other uses of this pair/orientation and
   the first face to create it. Test each texture paired with a different partner
   if the evidence suggests input dependence.
3. Same build/assets/settings: fresh process -> Vertigo; Counterstrike -> Vertigo;
   Vertigo -> Counterstrike -> Vertigo. Add save restore or app backgrounding
   only if the reproduction involves it or the traces implicate resets.
4. If fresh versus switched differs, audit reset/invalidation boundaries. If both
   fail, prioritize draw state/input dependence. Either outcome alone does not
   establish whether the bug is old or newly introduced.
5. Run a known pre-isolation build against the same assets, graphics settings and
   viewpoint. Prefer separate test installations/workspaces; do not downgrade
   over the user's only save/data set. Bisect only after this demonstrates a
   version-dependent regression.

## Completion criteria

Reproduce a precise cause, show the controlled change corrects the composite,
and make a regression that fails before the fix and passes afterward. Verify
opaque combined output for this wall, proper cutouts for genuinely
super-transparent textures, existing merge orientations, and mission-switch
cache invalidation. Validate on the original phone as well as emulator, and
build both engines for shared rendering changes. Avoid reimporting good assets
or masking the symptom with automatic cache flushing.

## Step 1 implementation

Implemented opt-in capture when the Texture log category is enabled before the
reported rock349/misc063 pair is first merged. No merge correction, replay, or
cache flush is performed by the probe. Creation records contain source hashes,
mission asset generation, palette, first face, cache serial, actual and requested
program state, shader uniforms, buffer/pointer interpretation, texture bindings,
clipping state, and GL errors separated by stage. Direct level-zero framebuffer
readback preserves framebuffer and pixel-pack bindings/settings. The tap reports
the saved creation hash alongside a new direct readback. Eviction and cache
clearing log the captured serial. Readback is restricted to the reported 64x64
case and does not run on every rendered frame.

Reusable emulator check: `android/tests/test_vertigo_merge_creation.ps1 -Install`.
It stages the local retail HOG under a diagnostic mission title and uses
`test_vertigo_merge_creation.jsonc` to visit segment 288 side 3 and run the tap
probe. It requires a disposable emulator and changes its test mod selection.

Initial emulator result: all 25 automation steps and diagnostic-stage checks
passed. The composite is already transparent at creation (hash 38699dc5,
zero_alpha=4096), and stays identical at tap. Before draw, scissor/stencil are
both disabled, the array buffer is zero, but the shim has external=0, the active
program lacks merge uniforms, and setup logs GL_INVALID_OPERATION. This narrows
the emulator failure to merge setup rather than later cache corruption. A final
requested-program field was added to make the phone comparison explicit.

Phone procedure: enable Texture logging in the launcher, fully exit and restart
the game, enter Vertigo directly, face the same wall, run `mwall tap`, and export
the log. Inspect `[mwall_create]` records together with the normal tap records.

Final instrumented build passed the same 25-step emulator test. The added field
confirms `requested_merge_program=0 requested_is_program=0` before the failing
merge. Direct creation and tap hashes are both 38699dc5. All Android ABIs built;
scoped formatting and diff checks passed. No rendering fix is included in this
diagnostic change; the phone run should confirm whether its shader state matches.

## Confirmed cause and fix

The phone's second run in `debuglog_20260915_201830 (1).txt` matches the emulator:
requested merge program 0, setup GL_INVALID_OPERATION, and 4096 transparent pixels
at creation and tap. Scissor is disabled and the array buffer is zero in this
capture. The mission asset reset calls `ogl_smash_texture_list_internal`, which
also calls `ogl_done_prog`, but omitted the shader recreation used by existing
graphics reset paths. This is a renderer lifetime regression in asset isolation;
the source texture evidence does not implicate the retail Vertigo files.

Restore merge shaders immediately after texture teardown when `gl_initialized`
is true and OGL_MERGE is enabled. The shared Android hook covers both engines,
including mission activation, return to base, and failed activation cleanup.
The graphics initialization guard preserves operation without a graphics context.
Desktop code and merge shader behavior are unchanged.

Extend the emulator regression to 51 steps: fresh Vertigo, tap the reported wall,
return to the in-engine menu, play Counterstrike, then return to Vertigo and tap
again. Require valid merge programs, no setup/draw/mipmap errors, fully opaque
creation and tap readbacks, matching hashes, and distinct tapped cache entries.
Allow additional opaque creations during menu navigation without requiring a tap
for each transient entry. The pre-fix phone and emulator records fail the new
shader-validity and opacity assertions.

Validation: Android debug build passed for arm64-v8a, armeabi-v7a, and x86_64
(both engines), with no compiler warnings in the fix build. Scoped quality
checks passed. The 51-step Vertigo test passed: requested programs 12 and 36
are valid, all creation/tap pixels have alpha 255, and the composite hash is
70599ddd both before and after switching. The existing 45-step Counterstrike
merged-wall route and door45 GPU regression also passed. Logs are in
`temp/vertigo-fix-build.log`, `temp/vertigo-fix-test.log`,
`temp/vertigo-creation-logcat.txt`, and `temp/vertigo-fix-wall-regression.log`.
Confirmation on the original phone remains pending.
