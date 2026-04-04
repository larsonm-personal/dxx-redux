# Overlay + Launcher Graphics Settings Improvements

## Status: PHASES 1, 3, 6, 7 DONE

## Summary
Seven areas of work related to graphics settings persistence, display, and usability.

---

## Phase 1: Save in-game AF/AA edits back to launcher defaults
**Effort: Small** -- DONE

### Persistence concern
The `settingsSaver` callback in VideoInfoOverlay already writes to SharedPreferences at
the moment of editing (when the user taps the AF/MSAA cycle button). However, it uses
`.apply()` which is async -- the write is enqueued but if the process is killed before
the async write completes (phone reboot, OOM kill, user force-stop), data can be lost.

**Fix**: Change `.apply()` to `.commit()` in the settingsSaver lambda. `.commit()` is
synchronous and guarantees the data is on disk before returning. Since this only fires on
user button taps (not per-frame), the UI thread cost is negligible.

### Launcher UI staleness
GraphicsSettingsPage reads prefs once via `remember { mutableIntStateOf(prefs.getInt(...)) }`.
This caches the value for the lifetime of the composable. However, since the user navigates
away from the page to launch the game and navigates back later, the composable is
recomposed fresh with a new `remember` initialization -- so it naturally reads the updated
SharedPreferences value. No additional fix needed for the launcher UI.

### Changes
- `MainActivity.kt`: Change `settingsSaver` from `.apply()` to `.commit()`

### Files
- `android/app/src/main/java/com/dxxredux/app/MainActivity.kt`

### Test
- Set AF=2, launch game, cycle AF to 8x in overlay, force-stop app, relaunch,
  open Graphics settings -- should show AF=8x.

---

## Phase 2: Texture filtering and color depth in overlay + feasibility of live editing

### Color depth: display-only
Color depth (RGB565 vs RGBA8888) is set at EGL surface creation time. Changing it requires
destroying the EGL context and recreating the surface. This is effectively a restart.
**Decision: display-only in overlay.** Already exposed as stats[26] and shown in the
"Render" line, but the text overflows. Will be fixed in Phase 6.

### Texture filtering: live editing is feasible with flush
TexFilt (0=nearest, 1=bilinear, 2=trilinear) is passed to `ogl_loadbmtexture_f()` on each
texture load. Already-loaded textures won't pick up a new value. However, the AF flush
mechanism (already in ogl_start_frame) provides a template:

- Add a `g_texfilt_pending_apply` volatile flag and a `g_texfilt_level` int
- Add JNI `nativeSetGraphicsOption("tex_filt", value)` support
- In `ogl_start_frame`, when flagged, flush ALL loaded textures so they reload with the new
  texfilt value (GameCfg.TexFilt must also be updated)
- Also update descent.cfg so the setting persists

The overlay already has AF and MSAA cycle buttons. Add a TexFilt cycle button: OFF -> Bilinear -> Trilinear -> OFF.

### Changes
- `d2/arch/ogl/ogl.c`: Add `g_texfilt_pending_apply` volatile, handle in `ogl_start_frame`
- `d1/arch/ogl/ogl.c`: Mirror
- `android/app/src/main/cpp/jni_main.c`: Handle "tex_filt" in `nativeSetGraphicsOption`,
  expose current TexFilt value in stats buffer (new index 27)
- `VideoInfoOverlay.kt`: Add TexFilt cycle button, add display of color depth
  (split from Render line)
- `GraphicsSettingsPage.kt`: Ensure TexFilt setting is reactive to in-game changes
  (TexFilt is written to descent.cfg; in-game changes need to also write back)

### Config persistence for in-game TexFilt changes
TexFilt lives in descent.cfg, not SharedPreferences. Options:
  a. Have JNI write descent.cfg from C (WriteConfigFile is called on quit anyway)
  b. Add a SharedPreferences mirror for TexFilt and sync on launch
  c. Simply rely on the engine's existing WriteConfigFile at clean quit

**Recommended: (c)** -- update `GameCfg.TexFilt` from JNI when the overlay changes it,
and the engine's existing config save on quit will persist it. The launcher already reads
descent.cfg to populate the UI. No extra sync needed.

### Files
- `d2/arch/ogl/ogl.c`, `d1/arch/ogl/ogl.c`
- `android/app/src/main/cpp/jni_main.c`
- `android/app/src/main/java/com/dxxredux/app/VideoInfoOverlay.kt`

### Test
- Cycle TexFilt in overlay from nearest -> bilinear -> trilinear, visually confirm textures
  change smoothness. Check framebuffer_avg changes. Exit game, re-enter, confirm setting persisted.

---

## Phase 3: Launcher graphics page -- reduce text size for more visible controls

**Effort: Small** -- DONE

Currently uses `fontSize = 14.sp` for headers, `13.sp` for options, `11.sp` for notes,
with `12.dp` spacers and `2.dp` row padding. Reduce:
- Headers: 14sp -> 11sp
- Options: 13sp -> 10sp
- Notes: 11sp -> 9sp
- Inter-section spacers: 12dp -> 6dp
- Row padding: 2dp -> 1dp
- RadioButton sizes: scale down via Modifier.size

### Files
- `android/app/src/main/java/com/dxxredux/app/GraphicsSettingsPage.kt`

### Test
- Open Graphics page, verify all sections visible without scrolling on a typical phone.

---

## Phase 4: All settings saved when edited (in-game -> launcher)

This is mostly covered by Phases 1 and 2:
- AF/MSAA: already saved to SharedPreferences by `settingsSaver` callback. Phase 1 fixes
  the launcher reading stale values.
- TexFilt: Phase 2's approach (c) -- update GameCfg.TexFilt, engine's WriteConfigFile
  persists on quit. Launcher reads descent.cfg.
- ColorDepth: display-only in overlay, not editable in-game.

No additional work needed beyond Phases 1-2.

---

## Phase 5: GPU timer showing 0.0ms on real device (S25)

### Root cause analysis
The implementation uses `GL_EXT_disjoint_timer_query` with double-buffered queries:
1. Frame N: `glBeginQuery(GL_TIME_ELAPSED_EXT, queries[idx])`
2. End of frame N (gr_flip): `glEndQuery(GL_TIME_ELAPSED_EXT)`
3. Frame N+1 (ogl_start_frame): check `GL_QUERY_RESULT_AVAILABLE` on queries[prev]

**Problem on real GPUs**: The query result from the previous frame may not be immediately
available because the GPU runs asynchronously. With double-buffering (only 2 query objects),
if GPU latency is >1 frame, the result from `queries[prev]` isn't ready yet and
`g_gpu_time_us` stays 0.

**Also**: `GL_GPU_DISJOINT_EXT` checking is too aggressive. On mobile GPUs, thermal
throttling or DVFS changes set the disjoint flag frequently, discarding valid results.

### Possible fixes (ordered by preference)

**Option A: Triple-buffer queries + poll previous-previous frame**
Use 3 query objects instead of 2. Poll queries[N-2] which has had a full frame to complete.
Most GPU drivers complete within 2 frames.

**Option B: Blocking read with GL_QUERY_RESULT (no AVAILABLE check)**
When `GL_QUERY_RESULT_AVAILABLE` is false, just read `GL_QUERY_RESULT` directly -- this
blocks the CPU until the GPU finishes the query. This adds a sync point but gives accurate
timing. Acceptable for a debug overlay.

**Option C: Use EGL_ANDROID_get_frame_timestamps if available**
This is a newer Android API that gives per-frame GPU composition timestamps without
query objects. Only available on Android 8+ with supporting drivers. More complex to
integrate and doesn't give fine-grained shader/raster time.

**Option D: Frame delta heuristic**
Compare wall-clock frame time with CPU-measured frame time. The difference is approximately
GPU time. Very rough but works everywhere.

**Recommendation: Option A (triple-buffer)** -- minimal code change, no sync point, reliable
on most hardware. Fall back to Option B if result still isn't available after 2 frames.
Also: relax the disjoint check -- only discard if disjoint is set, but don't accumulate
zero; instead keep the last valid reading.

### Changes
- `d2/arch/ogl/ogl.c`: Change query array from [2] to [3], adjust index cycling, poll N-2
  with blocking fallback. Keep last valid g_gpu_time_us on disjoint.
- `d1/arch/ogl/ogl.c`: Mirror
- Possibly add a "gpu_time_stale" flag to stats so the overlay can show an aged value
  differently

### Test
- Run on real S25 device, check GPU line shows non-zero values.
- Run on emulator (SwiftShader), verify it still works.

---

## Phase 6: FPS/frame line formatting cleanup -- DONE

### Current layout (lines 2-4 of overlay)
```
FPS: 25                    (green, 1.3x text size)
Frame:   40.5ms avg / 42.1ms max   (1.0x text, 6x offset)
[==============     ]       (load bar)
```

### Desired layout
```
VIDEO  25fps               (title stays white, fps on same line, color-coded, 1.0x text)
frame  40ms avg / 42ms max (integer ms, label lowercase, tighter spacing)
[==============     ]      (load bar, stays as-is)
```

### Changes
- `VideoInfoOverlay.kt` onDraw():
  - Remove the separate "FPS: N" line
  - Append fps text to the VIDEO title line: "VIDEO" in white, then "  25fps" in color-coded
    paint at same baseline
  - Change frame time formatting from `"%.1f".format(...)` to integer:
    `"${frameTimeAvg / 1000}ms"`
  - Reduce the label-to-value offset for "frame" from `baseTextSize * 6f` to `baseTextSize * 4.5f`
  - Make frame label lowercase ("frame" not "Frame")
  - numLines decreases by 1 (FPS line removed, merged into title)

### Files
- `android/app/src/main/java/com/dxxredux/app/VideoInfoOverlay.kt`

### Test
- Visual -- run game, check overlay, screenshot. Verify fps on VIDEO line, frame shows integer ms.

---

## Phase 7: Reorder overlay lines -- GPU and cache move up -- DONE

### Current order
```
 1. VIDEO  25fps         (after Phase 6 merge)
 2. frame  40ms avg ...
 3. [load bar]
 4. Tex mem: 32MB
 5. Hires: 100/405 (24%)
 6. Max res: 256x256
 7. GL cap: 4096px
 8. Render: 640x360 / 1280x720  RGB565
 9. Binds: 142 (85% cache)
10. Polys: 1200  shd:4  mask:0
11. Cache: 450ms
12. [AF button]
13. [MSAA button]
14. GPU: 0.0ms
15. [Labels button]
```

### Desired order
```
 1. VIDEO  25fps
 2. frame  40ms avg / 42ms max
 3. [load bar]
 4. GPU: 2.1ms             <-- moved up
 5. Cache: 450ms            <-- moved up
 6. Tex mem: 32MB
 7. Hires: 100/405 (24%)
 8. Max res: 256x256
 9. GL cap: 4096px
10. Render: 640x360 / 1280x720
11. Color: RGB565           <-- split from render line (Phase 2 display)
12. TexFilt: Trilinear      <-- new display + cycle button (Phase 2)
13. Binds: 142 (85% cache)
14. Polys: 1200  shd:4  mask:0
15. [AF button]
16. [MSAA button]
17. [Labels button]
```

Net line count: same or +1 (splitting render/color, adding texfilt, removing separate FPS line).
The "MSAA and AF take effect on next launch" footer text can be removed since they now
take effect immediately.

### Changes
- `VideoInfoOverlay.kt` onDraw(): Reorder drawing blocks

### Files
- `android/app/src/main/java/com/dxxredux/app/VideoInfoOverlay.kt`

---

## Implementation Order

1. **Phase 6** -- FPS/frame formatting (small, self-contained)
2. **Phase 7** -- Reorder overlay lines (depends on 6 for line count)
3. **Phase 1** -- Save AF/AA back to launcher (small, independent)
4. **Phase 3** -- Launcher text size reduction (small, independent)
5. **Phase 2** -- TexFilt live editing + display (medium, touches C + Kotlin + overlay)
6. **Phase 5** -- GPU timer fix (medium, C-side, needs real device testing)
7. **Phase 4** -- no additional work, covered by 1+2

## Open Questions
- Should TexFilt cycle button also affect MovieTexFilt? (probably not, separate concern)
- Should the "Takes effect on next launch" note on TexFilt/ColorDepth in launcher be
  updated to reflect that TexFilt CAN now be changed live? (yes, update to only say
  ColorDepth requires restart)
