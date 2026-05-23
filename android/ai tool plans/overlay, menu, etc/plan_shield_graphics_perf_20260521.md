# Shield graphics performance plan

## Status

- [x] Research current defaults, runtime stats, and likely Android-only hotspots
- [ ] Capture one Shield baseline with the existing video overlay
- [ ] Add focused runtime instrumentation for the most likely hot paths
- [ ] Confirm the dominant cost on Shield
- [ ] Apply the smallest mirrored D1 and D2 fix if the probe confirms a root cause
- [ ] Build and validate on Android

## User report

1. Graphics performance on Shield is very low
2. The game is unplayable at times
3. If there is no obvious code problem, add instrumentation to find the bottleneck

## Findings

### 1. First-launch defaults are not aggressively expensive

Relevant anchors:

- `android/app/src/main/java/com/dxxredux/app/SetupActivity.kt`
- `android/app/src/main/java/com/dxxredux/app/AdvancedSettingsPage.kt` `computeResolutionOptions()`
- `d1/main/config.c`
- `d2/main/config.c`
- `android/app/src/main/java/com/dxxredux/app/VideoInfoOverlay.kt`
- `android/app/src/main/cpp/jni_main.c` `nativeGetVideoStats()`

What the code does now:

- First launch writes render resolution to one half of the real display size, rounded to even dimensions
- Config defaults are conservative: `TexFilt=0`, `AnisoLevel=0`, `MsaaLevel=0`
- The existing video overlay already exposes render resolution, display resolution, frame time, GPU time, texture binds, polygon count, filter mode, AF, and MSAA

Implication:

- The Shield slowdown is probably not caused by an overly aggressive default configuration
- User-changed settings can still hurt, but the stock path does not look like an obvious default-resolution or default-MSAA mistake

Cheap discriminating check:

- On Shield, open `VideoInfoOverlay` in a bad scene and record `Render`, `FPS`, `CPU`, `GPU`, `Binds`, `Polys`, `TexFilt`, `AF`, and `MSAA`
- If the render resolution is at or near full native size or filtering is enabled, lower to `1/3` or `1/4` for a quick control test before changing engine code

### 2. The merged-wall debug harness is doing real per-draw work in live rendering

Relevant anchors:

- `d2/arch/ogl/ogl.c` `g3_draw_tmap_2()` and related merged-wall draw paths
- `d1/arch/ogl/ogl.c` same Android merged-wall paths
- `d2/main/render.c` and `d1/main/render.c` texmerge and wall-routing call sites
- `android/app/src/main/cpp/shared/merged_wall_debug.c`
- `android/app/src/main/cpp/shared/android_log.c`

What the code does now:

- On Android merged-wall paths, `android_merged_wall_track_face()` is called for cached-merge, old-texmerge, and GPU two-pass routes
- `android_merged_wall_log_cover()` also runs on the two-pass overlay path when the cover draw is not skipped
- Those helpers do nontrivial work before any `debug_log()` call:
  - compute projected points and screen bounds
  - copy per-face geometry and UV data
  - walk previously tracked faces looking for cover matches
  - maintain per-frame merged-wall bookkeeping structures
- `debug_log()` itself is gated by `debug_log_enabled[]`, but that gate is too late to avoid the geometry and bookkeeping cost

Local hypothesis:

- This is the strongest current match for the Shield report because the cost is scene-dependent rather than constant
- Rooms with more transparent walls, cloaked walls, or `tmap2` overlays would pay more of this work, which fits the user report that performance is bad only at some times

Cheap discriminating check:

- Add timers and counters around `android_merged_wall_track_face()` and `android_merged_wall_log_cover()`
- Surface those numbers in the existing video overlay or an aggregate debug log summary
- Compare a known bad Shield scene against a calm scene
- If merged-wall time spikes with the bad scene, gate or short-circuit the tracking path when no merged-wall debug or snapshot feature is active

Likely fix direction if confirmed:

- Keep the current merged-wall debugging and snapshot features intact
- Move the heavy tracking behind an explicit need gate such as:
  - merged-wall snapshot pending
  - merged-wall debug mode active
  - target-texture logging active
  - explicit performance probe enabled
- Do not leave always-on geometry capture in the normal Shield render path if the instrumentation confirms it is material

### 3. `ogl_bindbmtex()` still reissues filter state on every bind when filtering is enabled

Relevant anchors:

- `d2/arch/ogl/ogl.c` `ogl_bindbmtex()`
- `d1/arch/ogl/ogl.c` `ogl_bindbmtex()`

What the code does now:

- On Android, when `GameCfg.TexFilt > 0`, the bind path re-sends `glTexParameteri()` for min and mag filtering on every bind
- The video overlay already shows `Binds` and bind reuse, so this can be correlated with scene activity

Local hypothesis:

- This is a plausible secondary hotspot if the Shield run has filtering enabled
- It is less likely to explain stock performance alone because the default filter setting is off

Cheap discriminating check:

- Count and time the filter-state `glTexParameteri()` calls
- Compare that time against `r_texbinds` and against the same scene with `TexFilt=0`

### 4. The Android end-of-frame `glGetError()` drain is worth timing, but it is probably secondary

Relevant anchors:

- `d2/arch/ogl/ogl.c` `ogl_end_frame()`
- `d1/arch/ogl/ogl.c` `ogl_end_frame()`

What the code does now:

- Each Android frame drains GL errors with `while (glGetError() != GL_NO_ERROR) {}`

Local hypothesis:

- On some drivers this can stall more than expected, but it is a constant per-frame cost and does not explain the scene-dependent nature of the report as well as the merged-wall path does

Cheap discriminating check:

- Add a microtimer around the error-drain block and expose average and max time
- Keep this as a secondary probe unless the merged-wall timing comes back clean

## Proposed work order

### Phase 1. Baseline on Shield with existing stats

- Reproduce one clearly bad scene on Shield
- Record the existing overlay rows:
  - render and display resolution
  - FPS
  - CPU frame avg and max
  - GPU time
  - binds and polygons
  - TexFilt, AF, and MSAA
- Use this to separate settings-driven cost from engine-path cost before adding new probes

### Phase 2. Add merged-wall instrumentation first

- Add lightweight per-frame counters and timers for:
  - `android_merged_wall_track_face()` calls and total time
  - `android_merged_wall_log_cover()` calls and total time
  - tracked-face count and cover-comparison count
- Export the data through `nativeGetVideoStats()` and show it in `VideoInfoOverlay`, or emit one aggregate summary line per frame bucket under `DLOG_GRAPHICS`
- Prefer overlay stats for rapid on-device iteration because they are easier to compare in real time than log files

### Phase 3. Add two narrow secondary probes

- Time the `glTexParameteri()` filter-state churn in `ogl_bindbmtex()`
- Time the `glGetError()` drain in `ogl_end_frame()`
- Only widen instrumentation beyond these if the merged-wall probe is not material

### Phase 4. Fix the dominant confirmed cost

- If merged-wall tracking is dominant, gate it behind an explicit debug or snapshot need
- If filter-state churn is dominant, add a small Android-only cache so repeated binds do not resend unchanged filter state
- If error-drain time is unexpectedly high, narrow when it runs or gate the expensive path behind a debug flag
- Mirror any D1 and D2 engine changes

### Phase 5. Validation

- Focused Android native build after the first instrumentation edit
- Re-check the same Shield scene with the overlay before and after
- Keep the first fix small enough that the same baseline scene can confirm whether the hypothesis was right

## Expected file touch list

- `android/app/src/main/cpp/shared/merged_wall_debug.c`
- `android/app/src/main/cpp/shared/merged_wall_debug.h`
- `android/app/src/main/cpp/jni_main.c`
- `android/app/src/main/java/com/dxxredux/app/VideoInfoOverlay.kt`
- `d2/arch/ogl/ogl.c`
- `d1/arch/ogl/ogl.c`
- optional: a new small shared Android perf helper if the counters do not fit cleanly in existing files

## Risks and guardrails

- Do not remove the merged-wall debug harness outright until the timer proves it is material
- Do not widen this into a broad renderer rewrite before checking the merged-wall path
- Keep any new instrumentation lightweight and bounded so it does not itself distort Shield behavior
- Mirror D1 and D2 changes where the engine path is shared by behavior, but prefer new shared Android helper code when it keeps duplicated edits small