# OpenGL efficiency opportunities

Initial scope: review current D1/D2 and shared Android rendering code, recover prior
performance findings, and propose experiments without changing runtime code

The implementation follow-up below records the authorized first pass and its
validation results

## Review plan

- [x] Read project instructions and locate the automap/font and renderer surveys
- [x] Trace remaining repeated submission, state-query, upload, and CPU work
- [x] Check candidates against existing fixes and current D1/D2 paths
- [x] Rank opportunities and specify measurements and correctness checks

This is a source survey, not a new benchmark. Existing worktree changes are
outside this review's edits

## Prior results worth preserving

The [automap study](../launcher/plan_level_automap_preview_study_20260717.md)
contains both of the original improvements:

- Line batching, commit `6be65333`: 3,039 visible lines previously meant 3,039
  draws and more than 9,000 buffer allocation/upload calls. The fixed Uneasy 4
  view improved from roughly 1,330 ms to 193 ms per event iteration, about 6.9x
- Font batching, commit `d7dca83a`: the documented change was one font-atlas
  draw per string instead of per glyph, rather than switching palettes less
  often. With all instructions visible, the fixed view improved from 22.7 FPS
  to 58.5 FPS. The existing code still brackets `ogl_internal_string`'s glyph
  loop with `ogl_ubitmap_batch_begin/end`
- Between these experiments, fixing nine uninitialized preview marker slots
  removed unnecessary marker rendering and improved 5.2 FPS to 22.7 FPS
- Dense automap edge traversal did not improve performance and was removed

The [normal renderer survey](normal_renderer_performance_survey_20260719.md)
already covered several tempting candidates:

- Packing vertex/color/UV arrays into one `glBufferData` per draw was retained
- Android min/mag texture-filter caching and removal of per-face model heap
  allocations were retained
- Uniform caching was tried without meaningful incremental improvement
- Per-frame streaming with `glBufferSubData` helped SwiftShader but hurt the
  phone badly: 572 us average render time became 94,421 us in the hardware A/B
  test. The streaming implementation was removed; do not reintroduce it based
  on emulator results
- The phone's 25 FPS in the fast mode was an intentional saved frame limiter
- Water animation did not cause steady-state texture uploads or merged-cache
  misses in the tested reactor view

## Ranked experiments

Rank reflects expected payoff relative to scope, not measured current savings

### 1. Batch cockpit gauge scanlines and small HUD primitives

Evidence:

- `d2/main/gauges.c:1883` draws the two energy masks one `gr_uline` per scaled
  row whenever energy is below 100
- `draw_afterburner_bar` at line 1926 draws rectangles inside nested row loops;
  `sb_draw_afterburner` at line 2376 draws one line per erased row
- `render_gauges` at line 4873 calls these on each cockpit/status-bar render
- `d2/arch/ogl/gr.c:1023` implements every `ogl_ulinec` as its own two-vertex
  GL draw, and `ogl_urect` likewise submits each rectangle separately
- The energy loops also exist in `d1/main/gauges.c:1647` and the current
  D1-in-D2 path, `d2/main/d1_in_d2/d1_in_d2_cockpit.c:1056`

Proposed experiment: collect the existing line endpoints/colors or rectangle
triangles and submit one ordered batch per gauge. Start with these bounded
loops, then consider reticles, mouse indicators, and observer bars. The automap
batch API only covers `g3_draw_line`; it does not currently batch these 2D lines

Measure draws, uploads, and HUD CPU time at full/half/empty energy and
afterburner, at two resolutions. Higher resolution increases these row counts
without increasing the number of conceptual gauges

Verify endpoint coverage, fade/blending, cockpit scaling, and full/status/HUD
modes in D1, D2, and D1-in-D2. Preserve the existing primitive order and pixel
coverage before attempting to replace masks with simpler geometry

### 2. Remove repeated buffer-binding queries from the GLES shim

Evidence: `android/app/src/main/cpp/shared/gles3_shim.c:531` queries
`GL_ARRAY_BUFFER_BINDING` in each vertex/color/UV pointer setter; line 862's
draw wrapper queries it again. A normal textured client-array draw therefore
performs four queries, and the external two-UV path can perform five. These
queries remain even after the upload consolidation

Proposed experiment: explicitly track array-buffer binding across all binding
and deletion owners, or provide a known-client-array submission path and retain
the general path for external callers. Measure query counts and sampled CPU
cost first; a GL query is not proof of a GPU stall

Correctness constraint: these queries support a real compatibility fix, commit
`6f012462`. `d2/xmodel/xmodel.cpp:198` supplies offsets into a persistent VBO,
including offset zero. Do not assume every pointer is CPU memory or remove the
binding restoration. Account for raw GL calls, context recreation, probes, and
merged-wall rendering. Run `test_gles3_shim_vbo_arrays.jsonc` for both engines

This is Android-local and broadly applicable, but has more state-coherence risk
than batching the bounded HUD loops

### 3. Gate inactive merged-wall diagnostic bookkeeping

Evidence: in `android/app/src/main/cpp/shared/merged_wall_debug.c`,
`android_merged_wall_track_face` at line 4989 copies geometry, UVs, projected
coordinates, strings, and bounding boxes without an early diagnostics gate.
`android_merged_wall_log_cover` at line 5079 builds per-face projected data and
performs exact-match and overlap scans over the tracked faces. The cap is 32
tracked faces, so this is bounded work, not an unbounded quadratic algorithm

These functions are called from ordinary D1/D2 textured draws. Disabling the
eventual log writer does not avoid the geometry preparation and scans

Proposed experiment: determine whether any snapshot, tap probe, or texture-log
consumer needs the frame, then skip only diagnostic collection when none does.
Track calls, faces copied, comparisons, and sampled CPU time with diagnostics
off, texture logging on, and an explicit snapshot requested

Do not disable the actual merged-texture cache, clipping, transparency handling,
or draw-context ownership merely because they live in a file called `debug`.
Preserve complete requested-frame snapshots and crosshair probes. The current
framebuffer readbacks and texture-label anchor work already have request gates;
they are not unconditional per-frame readback candidates

### 4. Check the merged-wall cache before binding its source textures

Evidence: `d2/arch/ogl/ogl.c:1390` binds the bottom texture on unit 0 and overlay
on unit 1, returns to unit 0, and only then looks up a cached merged bitmap.
On a hit it calls `g3_draw_tmap` and binds the composite. The source textures
are therefore bound even when that draw samples only the composite. D1 has
the same structure at line 1380

Proposed experiment: for already-resident, valid, plain-transparent textures,
try a valid composite hit before setting up source texture units. Use the
existing path for misses, unloaded textures, super-transparency, and forced
two-pass diagnostics. This can eliminate source binds and unit switches on
warm cache hits without changing the draw count

Measure hits alongside actual binds and active-unit calls in a decal/door-rich
fixed view. Check palette and texture invalidation, animation, runtime filters,
context loss, and transparency regressions. Retain per-texture-unit binding
caches; the old single-scalar cache was a correctness bug, not a shortcut to
restore. Do not enlarge the 32-entry merge cache without measured miss churn

### 5. Reuse transient blit textures and avoid redundant scaling/mipmaps

Evidence:

- `d2/arch/ogl/ogl.c:2071` / D1 line 2062: `ogl_ubitblt_i` creates a local
  texture, converts/uploads the bitmap, draws it, and deletes the texture on
  every call
- `ogl_loadtexture` at D2 line 2845 expands dimensions to powers of two and
  generates mipmaps when `texfilt` is enabled
- `android/app/src/main/cpp/shared/android_menu_scale.c:706` allocates and
  CPU-scales an indexed bitmap to its final display size, then calls the
  transient blitter with `texfilt=1`. This builds mipmaps for an already-sized
  image immediately before deleting it
- The region path at line 758 uploads and deletes each 1024-pixel tile; D2's
  movie callback (`d2/main/movie.c:249`) also uses the transient blitter

Proposed experiments, separately: keep bounded reusable upload textures by
format/size; update changed content; and use a non-mipmapped filter for the
already-scaled 1:1 menu case if visual comparison permits. A later experiment
can move the CPU scaling to the GPU while preserving current sampling

Measure allocations, converted/uploaded bytes, texture creates/deletes, mipmap
calls, and CPU time in scaled menus, scrolling lists, save previews, and movies.
This primarily targets those screens, not normal world rendering

Preserve current-palette conversion, transparency, crop/stride handling, tiled
oversized menus, keyboard offsets, and context loss. Content changes and palette
changes must invalidate any retained image; pointer identity alone is not enough

### 6. Batch adjacent compatible world/model/sprite draws

Evidence: `d2/3d/interp.c:500` still submits each visible legacy textured face
through `g3_draw_tmap`; `d2/arch/ogl/ogl.c:1259` builds and submits one face at
a time. `g3_draw_bitmap_full` at line 1984 submits one four-vertex sprite per
draw. Equivalent paths exist in D1

Proposed first step: count consecutive runs with identical texture, shader,
matrix, blend/depth, clip, and render-target state. If runs are substantial,
triangulate and batch only those runs, retaining their original sequence

This is the larger potential submission win, but it has substantially more
scope than the first five. Respect classic painter ordering, transparency,
recursive model/submodel sorting, morphs, and cockpit subviews. Do not globally
sort translucent faces by texture. High-resolution xmodels already use retained
VBOs and per-material ranges; do not treat them as per-face uploads

### 7. Extend text batching across compatible adjacent strings

Evidence: `d2/2d/font.c:713` begins and ends a batch per string. Menus, kill
lists, and objective overlays with many short strings still make many draws
using the same font atlas

Proposed experiment: measure compatible consecutive string runs, then use an
explicit text-pass batch with flushes at atlas, scissor, canvas, matrix, blend,
and intervening primitive changes. Preserve per-glyph colors and ordering

This is a follow-up to existing glyph batching, not a claim that per-character
draws remain. Its likely value depends on the number of separate strings and
the cost of their layout relative to submission

## Small follow-ups

- `g3_draw_bitmap_full` recomputes the same view-space center inside its
  four-corner loop. Compute it once and copy it for each corner. This removes
  three vector subtractions/rotations per sprite but probably has modest value
  compared with removing draws
- D1, D2, and D1-in-D2 full cockpit code call `draw_numerical_display` twice in
  one pass. Check what the intervening ship/shield drawing covers before
  removing either call. Text blending and draw order can make duplication
  visually significant
- `ogl_end_frame` drains GL errors and reads the clock every pass, and MSAA
  begin/resolve has additional error polling. Attribute this cost before
  changing diagnostics; do not assume it is large or remove useful error
  handling indiscriminately
- Broad uniform/attribute-state caching is lower priority: uniforms already
  had a negative experiment, and external GL owners complicate coherence

## Measurement and implementation order

Start with gauge batching, inactive diagnostic collection, and shim query
attribution. Then test the merged-cache hit path and transient menu uploads.
Only expand to world/model batching after counting compatible runs

Use repeated fixed-view A/B windows on the target phone, with warmup and the
same assets, resolution, cockpit, MSAA, filtering, VSync, and FPS limit. Report
render CPU time, GPU time, swap/resolve time, and median/p95 frame time; FPS
alone hides improvements under a frame cap

Existing counters cover textured faces, binds, and merge-cache hits/misses, but
are reset by `ogl_start_frame`, which can execute more than once per presented
frame. Add any experiment counters at the actual submission owner and aggregate
across all passes until `gr_flip`. Count HUD/text draws and uploads explicitly;
world polygon count is not total GL draw count. GPU timing currently ends before
MSAA resolve/swap, so report those separately

Useful maintained scenarios include `test_d2_level7_reactor_water_profile.jsonc`,
`test_launch_to_automap.jsonc`, `test_gles3_shim_vbo_arrays.jsonc`,
`test_ogl_runtime_texture_options_unified.jsonc`, and
`test_merged_wall_snapshot_regression.jsonc`. Add a focused cockpit energy/
afterburner scene and menu/movie cases for the relevant experiments

No new performance measurements, runtime changes, or device tests were made in
the initial survey

## Implementation follow-up

Authorized on 2026-09-25, with concurrent edits/builds/emulator use expected

- [x] Establish isolated build/test artifacts and preserve a baseline
- [x] Implement and compare cockpit gauge batching in D1, D2, and D1-in-D2
- [x] Gate inactive merged-wall collection and validate requested diagnostics
- [x] Remove shim binding queries through coherent bind/delete tracking
- Follow-up: test early merged-texture cache hits and transient-blit improvements
- Follow-up: measure compatible world/model/sprite and text runs before expanding batching
- [x] Run scoped formatting, relevant builds, integration and visual comparisons
- [x] Record retained changes, deferred experiments, measured limits, and review diff

Do not touch another task's emulator or build outputs. Keep compilation parallelism
bounded and build from a stable source snapshot if active edits would contaminate
comparisons. Hardware timing and emulator timing must be identified separately

### First implementation pass

The first pass is limited to the three opportunities with narrow state/lifetime
boundaries. The larger cache/upload/submission experiments remain follow-ups

- Shared bounded 2D batching wraps only consecutive energy-gauge scanlines in
  D1, D2 and D1-in-D2, plus D2 afterburner masks. It retains the original line
  coordinates and colors, expands rectangle fans into the same two triangles,
  flushes on primitive changes/capacity, and ends before canvas/state changes
- The GLES shim now tracks ARRAY_BUFFER through bind/delete wrappers, including
  its own upload path and model VBO calls. Pointer setters and draw submission
  no longer query the driver for that binding. Initialization and explicit probes
  still query it. Other buffer targets do not update this cache
- Merged-wall face and cover geometry collection returns early when neither a
  requested snapshot/tap probe nor texture logging needs it. Actual drawing and
  the merged-texture cache remain independent of that diagnostic gate

The maintained GPU probe compares all pixels for empty batches, 800 lines,
800 rectangles, and 800 alternating primitives, including gradients and alpha
blending. It also checks VBO offsets, a different buffer target, deletion of the
bound buffer, and restoration of the prior copy-read binding

Measured submission counts from the actual batching helper, with a temporary
host-only draw counter around its normal GLES shim calls:

| Probe workload | Immediate draws | Batched draws | Pixels |
| --- | ---: | ---: | --- |
| Empty | 0 | 0 | Identical |
| 800 lines | 800 | 2 | Identical |
| 800 rectangles | 800 | 4 | Identical |
| 800 alternating lines/rectangles | 800 | 800 | Identical |

Both SwiftShader GLES 3.0 and ANGLE GLES 3.0 on the Intel Arc 140T passed this
probe. The host harness compiles the production shim and batching sources;
only Android logging and the engine include surface are stubbed. These are
submission counts for the synthetic overflow case, not gameplay FPS claims

### Build isolation and validation evidence

Artifacts are in `temp/opengl-efficiency-20260925/`. The source snapshot and
baseline libraries are preserved; the candidate manifest records the changed
rendering files. Other tasks' source edits and build directories were not
reverted or used as output destinations

The frozen snapshot caught three unrelated guidebot changes in progress. Only
the snapshot received a missing function declaration, a PHYSFS write-helper
correction, and a missing return value. Both baseline and candidate use those
same fixes. Generated native build metadata and pinned asset dependencies were
also copied into the private build/test tree

- Android CMake/Ninja x86_64 Debug: both engines link successfully
- MSVC x86: affected GL sources compile for both engines; both gauge sources
  also compile with OGL disabled; D1-in-D2 cockpit compiles
- Scoped mixed-language code quality: pass
- Host SwiftShader and ANGLE GPU probes: pass, with identical framebuffer bytes
- D2 baseline gauge continuation: 59/59 steps pass
- Candidate D2 cockpit/GPU scenario: 83/83 steps pass
- Candidate native D1 cockpit/GPU scenario: 84/84 steps pass
- Candidate D1-in-D2 cockpit/GPU variant: 84/84 steps pass, including explicit
  imported-mode and cockpit-active assertions
- Merged-wall snapshot regression: 45/45 steps pass, including cached-merge and
  door-cover GPU captures while ordinary texture logging is disabled
- Runtime texture-option scenarios: D2 66/66 and D1 42/42 steps pass
- Native D1 and D1-in-D2 half-energy gauge screenshots: both 265x35-pixel bar
  regions match exactly (zero different pixels)

The reusable cockpit test is `test_ogl_gauge_batch_unified.jsonc`. Run it through
`android/helpers/run_test.ps1` with `-Game d1` or `-Game d2`. The imported cockpit
variant uses `-Game d2 -Params @{ ASSETS = 'd1_in_d2' }`

Final review found no whitespace errors in the touched tracked code. All 14
rendering files in the candidate manifest match the validated snapshot. The
test-only `player_energy` setter was applied to both baseline and candidate
without copying other concurrent automation edits into the snapshot

The private AVD uses port 5580. Global emulator restarts from concurrent work
terminated it twice. It was subsequently restarted with host GPU acceleration
to finish correctness testing. Early SwiftShader timing was highly unstable,
and the renderer changed after the restart, so those timings are not a valid
A/B performance result. No physical phone was available. The baseline screenshot
also contains Android's first-use immersive-mode overlay and is not used as a
pixel-comparison reference

### Remaining experiments

Early merged-cache hits remain worthwhile, but the current route deliberately
pages in source bitmaps before checking super-transparency and generated masks.
A pre-bind fast path must preserve those first-use and invalidation rules

Transient blit reuse needs separate menu/movie lifetime and filtering coverage.
World/model/sprite/text batching needs counts of consecutive compatible runs
before changing draw order or extending state ownership. The previous phone
regression from buffer streaming remains a reason to require phone measurements
for those broader experiments
