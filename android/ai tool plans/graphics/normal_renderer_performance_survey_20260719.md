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
