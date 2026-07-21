# Normal renderer performance survey

## Scope

- Use the recent automap batching and font palette optimizations as search patterns
- Survey normal gameplay OpenGL, polygon-model, effect, HUD, robot, and guidebot paths
- Prioritize measured or strongly evidenced hot-path fixes with small upstream-friendly diffs
- Preserve unrelated worktree changes and D1/D2 parity where applicable

## Plan

- [x] Read repository instructions and identify the automap optimization commits
- [x] Establish likely normal-renderer and AI hot paths using existing profiling hooks and static call-frequency analysis
- [x] Audit redundant OpenGL state changes, immediate-mode draw fragmentation, repeated lookup/allocation, and per-object work
- [x] Implement only low-risk, compact optimizations supported by the survey
- [x] Add focused profiling or regression coverage where practical
- [x] Run scoped code quality, Android and Windows builds, and relevant tests
- [x] Record findings, rejected candidates, measurements, and validation here

## Initial hypotheses

- Repeated state setters may still issue redundant GL calls inside polygon, sprite, laser, or HUD loops
- Legacy polygon models may fragment draws by face or submodel, which is relevant to reactors and robots
- Robot and guidebot updates may repeat path, visibility, or collision queries more often than their state requires
- Existing Android frame/object profiling should be extended only if it cannot attribute the suspected cost

## Findings and changes

### Consolidate each legacy draw into one GPU upload

The GLES compatibility shim serviced every legacy `glDrawArrays` call by orphaning
its stream VBO and then issuing as many as four `glBufferSubData` calls, one for
each enabled client array. Walls and each visible polygon-model face pass through
this path, so a reactor or robot with many faces multiplied the driver calls.

The shim now packs the enabled arrays in reusable CPU staging memory and supplies
the packed data in the existing `glBufferData` call. This changes up to five
buffer API calls per primitive to one without changing draw order or vertex data.

In the fixed D2 Counterstrike level 7 emulator scene, four profiling samples gave:

| Build | Frames | Average render time | Sample render times |
| --- | ---: | ---: | --- |
| Baseline | 83 | 45,527 us | 47,351; 40,842; 44,601; 50,204 us |
| Single upload | 90 | 23,209 us | 24,636; 18,004; 20,033; 30,637 us |

That is a 49.0 percent reduction in average render time for this emulator scene.
The emulator uses a software-backed GPU, so the absolute timing is not a phone
prediction, but the fixed-scene before/after result strongly confirms that the
per-primitive upload fragmentation was expensive.

### Skip redundant texture filter state

`ogl_bindbmtex` reapplied both `GL_TEXTURE_MIN_FILTER` and
`GL_TEXTURE_MAG_FILTER` on every textured face, even when the texture's requested
filter had not changed. The Android texture record now caches those two values,
and the shared helper only calls `glTexParameteri` on an actual transition. Live
menu, HUD, mipmap, and runtime filter changes use the same helper so the cache
stays coherent. This is D1/D2-parity code and remains Android-gated in the common
headers.

### Remove per-face model heap traffic

Both D1 and D2 allocated and freed an `lrgb_list` for every visible textured face
of a legacy polygon model, including robots and reactors. Both normal and
morphing model interpreters now use their already-bounded
`MAX_POINTS_PER_POLY` stack storage. This removes allocator traffic from the
model face loop with a four-line semantic diff per game.

## AI and deferred candidates

- In the same level 7 samples, simulation averaged about 0.28 to 0.43 ms while
  rendering took about 40 to 50 ms before the upload fix. Ordinary robot and
  guidebot work was therefore not the primary bottleneck in that scene.
- Guidebot route refreshes are already dirty/event gated. Route-completion
  monitoring is limited to the active local goal, rate-limited to 250 ms, and
  only rescans after a pending event.
- D2 missile-camera robot waking can scan the object list when it lacks a cached
  viewer and can revisit the same quarter of objects on multiple frames within
  one 20 Hz tick. It is a plausible missile-camera-specific candidate, but not
  active in the profiled normal view. Changing its cadence could alter robot wake
  latency, so it was left for a focused missile-camera profile.
- A shader-uniform cache was prototyped and profiled. It showed no meaningful
  incremental improvement over the upload consolidation, so it was reverted.
- Cross-face world/model batching could remove more draw calls, but texture and
  shader changes, transparency order, clipping, and recursive model sort order
  make it substantially less suitable for a small, easily reconciled patch.
- Caching all shim attribute enables, VBO bindings, or external program state was
  deferred because the xmodel and external-shader paths can mutate the same GL
  state outside the legacy shim.

## Validation

- Android debug APK assembled successfully for arm64-v8a, armeabi-v7a, and x86_64.
- Android debug unit tests passed.
- Windows D1 and D2 Release builds passed.
- D1 and D2 CTest discovery completed successfully; neither build currently
  defines CTest tests.
- D2 `test_ogl_runtime_texture_options_unified.json5` passed, 43/43 steps.
- D2 `test_door45_cover_gpu_regression.json5` passed, 35/35 steps, including GPU
  pixel-readback assertions.
- D1 `test_launch_to_automap.json5` passed, 53/53 steps, including background and
  resume coverage.
- The fixed D2 level 7 profiling scenario passed all 20 automation steps on each
  measurement run.
- Scoped code-quality checks and `git diff --check` passed.

## Animated water follow-up

### Plan

- [x] Map D1/D2 effect animation from frame advance through wall rendering and texture upload/bind
- [x] Establish repeatable water-heavy D2 views and collect baseline render, texture, and draw behavior
- [x] Test likely thrashing hypotheses with focused instrumentation or controlled variants
- [x] Implement only compact changes with measured benefit and D1/D2 parity where applicable
- [x] Run focused visual/runtime tests, Android and Windows builds, unit tests, and code quality
- [x] Record measurements, conclusions, and deferred candidates here

### Findings

- `do_special_effects` advances animated wall effects by changing the bitmap index
  in `Textures`; it does not upload the new frame. Level paging touches every
  effect frame before play.
- A fixed view from segment 490 of Counterstrike level 7 rendered about 285 to
  303 textured polygons, but only three visible faces normally carried
  `TMI_WATER`. Once the view was established, water animation produced no
  texture uploads and no merged-wall cache misses. The 32-entry Android merged
  texture cache therefore was not cycling animated water frames in this view.
- Robot effects could load new sprite frames during combat, but those loads were
  event-driven and distinct from the continuously animated water walls.
- The strong correlation with water came from room complexity. The reactor view
  issued about 134 to 141 actual texture binds and nearly 300 legacy polygon
  draws per frame.

### Per-frame streaming VBO

The first upload consolidation still orphaned the GLES stream VBO with
`glBufferData` once per primitive. The shim now reserves regions in a 1 MiB
streaming VBO, uploads each packed primitive with `glBufferSubData`, and orphans
the backing store once at the start of a 3D frame. Draw order and the existing
one-upload-per-primitive behavior are unchanged.

On the same 1280x720 SwiftShader AVD and fixed reactor pose, stable early samples
gave:

| Build | Average render time | Textured polygons | Time per polygon |
| --- | ---: | ---: | ---: |
| Per-primitive VBO orphan | 58,239 to 60,111 us | 284 to 303 | 192 to 212 us |
| Per-frame streaming VBO, run 1 | 26,215 to 29,697 us | 290 to 293 | 91 to 102 us |
| Per-frame streaming VBO, run 2 | 27,165 to 27,590 us | 288 to 293 | 94 us |

This is a reproducible 52 to 55 percent render-time reduction in the relevant
software-GPU profile. It should not be read as a phone FPS prediction, but it
directly removes a driver synchronization/allocation hazard amplified by large
rooms.

### Follow-up validation

- Android debug APK assembled for arm64-v8a, armeabi-v7a, and x86_64.
- Android debug unit tests passed.
- Windows D1 and D2 Release builds passed; CTest discovery found no registered
  tests in either build.
- The new level 7 water/reactor profile passed twice, 29/29 steps, including
  invulnerability and final segment assertions.
- D2 runtime texture option coverage passed, 43/43 steps.
- D2 door 45 GPU pixel-readback regression passed, 35/35 steps.
- D1 launch/automap renderer coverage passed, 53/53 steps.
- Scoped code quality and `git diff --check` passed.

## On-device VBO A/B investigation

- [x] Add a profiling-only automatic A/B controller for the prior per-draw orphan path and the per-frame streaming path
- [x] Accumulate VBO upload calls, bytes, sampled CPU call time, frame counts, and frame-time distribution without per-frame file writes
- [x] Emit one compact Profiling-category summary per completed interval, including warmup and mode identity
- [x] Verify alternating modes and bounded log volume on the emulator reactor scenario
- [x] Build Android all ABIs, run renderer regressions and unit tests, and document phone test instructions

### A/B implementation and overhead

Enabling only the launcher Advanced tab's `Profiling` log category activates the
experiment. It alternates the packed per-draw `glBufferData` orphan path and the
per-frame streaming `glBufferSubData` path. Each mode has a 3 second unmeasured
warmup followed by a 12 second measurement. Upload calls and bytes are exact,
while CPU time around upload and orphan calls is sampled once per 64 calls to
avoid hundreds of clock reads per frame. Frame and render timing reuse the
existing profiler clocks.

One `type=vbo_ab` summary is written after each measurement, or about four lines
per minute. Existing Profiling-category sampling remains batched. A 55 second
emulator reactor run produced a 79,031 byte exported debug log, so the file
volume is about 86 KiB per minute rather than an unbounded per-draw trace.

The emulator successfully alternated both modes over three complete windows.
It continued to favor `per_frame_stream`, while retaining exact calls and bytes
needed to normalize intervals with different visible effects. The phone run is
expected to determine whether its hardware driver reverses that result.

### A/B validation

- Android all-ABI debug build and debug unit tests passed.
- Windows D1 and D2 builds passed.
- D2 reactor A/B scenario passed, 29/29 steps, and emitted both mode summaries.
- D2 runtime texture options passed, 43/43 steps.
- D2 door 45 GPU pixel-readback regression passed, 35/35 steps.
- D1 launch/automap coverage passed, 53/53 steps.

## First hardware A/B log analysis

- [x] Parse and pair all complete per-draw and per-frame measurement windows
- [x] Normalize frame and render timing against upload calls and bytes per frame
- [x] Check scene stability, outliers, texture activity, and other profiler buckets
- [x] Record the hardware conclusion and choose the next renderer experiment
- [x] Remove the per-frame streaming path and A/B controller, restoring packed per-draw uploads
- [x] Rebuild and run renderer, unit, and code-quality validation

### Hardware result

The arm64 phone log contained three `per_draw_orphan` windows and two
`per_frame_stream` windows in a stable level 4 view with 98 textured polygons,
zero water faces, 123 texture binds, 230 VBO uploads, and about 52.56 KiB of
vertex data per frame.

| Mode | FPS | Frame avg | Render avg | Sampled upload call |
| --- | ---: | ---: | ---: | ---: |
| Packed per-draw orphan | 25.0 | 39,887 us | 572 us | 0.6 us |
| Per-frame stream | 10.5 | 94,520 us | 94,421 us | 408.1 us |

The streaming path also orphaned twice per game frame because this render route
starts two OGL frames. More importantly, 230 sampled `glBufferSubData` calls at
about 408 us each account for essentially the entire 94 ms render time. GPU work
remained about 1 ms, simulation stayed below 0.25 ms, and no texture load burst
overlapped the completed comparisons. This is a phone-driver synchronization
stall, not water animation, robot AI, geometry variation, or GPU shading load.

The phone-validated fix removes the ring allocation, per-frame orphan,
`glBufferSubData`, and temporary A/B timing code. It retains the earlier useful
consolidation that packs all enabled client arrays into one staging buffer and
performs one `glBufferData` upload per draw.

Validation after the removal passed Android `assembleDebug` for arm64-v8a,
armeabi-v7a, and x86_64; all Android debug unit tests; Windows D1 and D2 builds;
the D2 reactor profile (29/29), D2 door GPU readback (35/35), D2 runtime texture
options (43/43), and D1 launch/automap (53/53) scenarios; scoped code quality;
and `git diff --check`.

## 25 FPS wait attribution

- [x] Trace the Android profile `wait` bucket to the frame pacing implementation
- [x] Identify the configuration value, default, and user control producing 25 FPS
- [x] Check whether profiling, multiplayer, or Android-specific state overrides it
- [x] Record the conclusion without changing an intentional battery-saving policy

Finding: D2 wraps `calc_frame_time()` in the Android `wait` profiler bucket. With
VSync disabled, that function uses the pilot's `PlayerCfg.maxFps`; a saved value
of 25 therefore targets a 40 ms frame and calls `timer_delay()`, which yields via
`SDL_Delay`. The measured roughly 38-39 ms wait plus sub-millisecond rendering is
the intended limiter, not renderer load. `maxfps` is exposed in the in-game
Graphics Options menu, persisted in the pilot file, and clamped to 25-200. New
pilots default to the release `MAXIMUM_FPS` of 200, so 25 is a saved pilot choice,
not an Android, profiling, or multiplayer override. VSync is off by default and,
when enabled, bypasses this explicit limiter in favor of presentation pacing.
No runtime behavior was changed.
