# Phase 5: metl154 rendering -- new diagnostic angles

## Status

Phase 4 findings: removing the GL_NEAREST force-override and alpha cutoff
produced identical results to the previous build. The overlay bars still
flicker on/off depending on perspective while the rock base stays 100%
opaque. Prior diagnostics (source pixel counts, vertical slices, filter
queries) all confirmed the SOURCE data and CPU-side texture are correct.
The issue is downstream -- something in the GL draw pipeline, draw order,
or state management.

This document proposes NEW investigation angles and specific logging items
to add in a single batch, covering hypotheses that were not explored in
phases 1-4.

### Current Implementation Status

- Implemented in D1 and D2: caller-side `[metl154face]` logging from
  `render_side()` with `frame`, `pass`, `seq`, `seg`, `side`, `child`,
  `side_type`, `wid`, `dot`, `tmap1`, `tmap2`, `orient`, and texture names
- Implemented in D1 and D2: expanded draw-time `[metl154diag]` logging with
  `frame/pass/seq`, live filter state, `GameCfg.TexFilt`, `ogl_aniso_level`,
  estimated mip-1 width, GL handle tracking, and texture size fields
- Implemented in D1 and D2: new `[metl154gl]` line logging expected versus
  active shader program and TEXTURE0/TEXTURE1/TEXTURE2 bindings
- Implemented in D1 and D2: new `[metl154uv]` line logging per-vertex raw and
  overlay UVs for up to four points plus a non-finite UV flag
- Implemented in D1 and D2: metl154-only shader debug modes in `ogl_prog_tex2`
  with `metl154_mode=1` for overlay alpha visualization and
  `metl154_mode=2` for overlay RGB visualization
- Implemented in Android native plumbing: `metl154_mode` can be set through
  JNI debug flags, automation `set_debug`, and now appears in introspection
- Implemented in Android UI: the in-game Video Info overlay now has a
  tappable `metl154: OFF/Alpha/RGB` button so the mode can be changed on a
  phone without adb
- Implemented in Android UI: the Video Info overlay now also has a second
  tappable `m154 exp` button that cycles metl154-specific KTX2 no-mip,
  decoded RGBA, decoded RGBA no-mip, and stock fallback experiments, and both
  metl154 buttons now poll their live native state from `nativeGetVideoStats`
- Implemented in Android native plumbing: `metl154_mode` transitions now log
  to both logcat and the exportable Texture debug log, and a new
  `metl154_experiment` debug flag now exists in JNI, automation `set_debug`,
  and introspection
- Implemented in D1 and D2: a GL-thread `g_metl154_experiment_pending_apply`
  path now invalidates only metl154 textures on demand so experiment changes
  force a live reload without flushing unrelated texture state
- Implemented in D1 and D2: new additive `[metl154exp]` logging now records
  experiment toggles, live apply events, load requests, KTX2 skips, and the
  actual metl154 upload path chosen among KTX2, decoded RGBA, and stock
  fallback, including whether mipmaps were disabled for the experiment
- Android validation passed after the implementation with
  `:app:assembleDebug`, `:app:testDebugUnitTest`, and
  `android\run-code-quality.ps1 -Fix`
- Desktop CMake validation is still blocked in this environment. The repo has
  no top-level desktop `CMakeLists.txt`, the cached desktop build trees are
  incomplete, and a fresh D2 configure hit missing desktop dependencies
  (`vcpkg` and SDL_mixer discovery)
- Implemented in D1 and D2: per-frame metl154 geometry tracking in the OGL
  draw path plus `[metl154cover]` logging for later draws that reuse the same
  face geometry after a metl154 merge draw
- Implemented in D1 and D2: a narrow Android-only polygon offset on plain
  metl154 merge draws so later equal-depth cover draws no longer win by
  default if the current overwrite theory is correct
- Revalidated after the follow-up with `android\run-code-quality.ps1 -Fix`,
  `android\gradlew.bat :app:assembleDebug :app:testDebugUnitTest`, and
  `android\run_test.ps1 -ScriptName test_launch_to_automap.json5 -Game d2`
- Latest device log (`android\temp_game_logs\debuglog_20260414_000241.txt`)
  showed that `dbg=1` and `dbg=2` do reach metl154 draws, but the affected
  draws still remain `shader=plain` with `mask=0`, expected and active shader
  programs match, and no `[metl154cover]` or `shader=mask` lines appear
- That latest log also includes many metl154 walls on `child=-1` solid faces,
  which weakens the earlier exact shared-wall overwrite theory as the primary
  explanation for the missing bars
- Implemented in D1 and D2: new `[metl154state]` logging on Android plain
  metl154 draws with depth, depth write mask, depth func, blend, cull,
  front-face, cull-face, color mask, framebuffer binding, and projected
  screen-space signed area
- Implemented in D1 and D2: new `[metl154split]` logging on Android plain
  metl154 quad draws with projected `sx/sy` vertices, signed triangle areas
  for the current fan split versus the alternate diagonal, and a `pick=` hint
  when one split looks less degenerate than the other
- Implemented in D1 and D2: Android plain metl154 draws now temporarily
  disable culling, and metl154 shader debug modes also temporarily disable
  depth test and depth writes so Alpha/RGB debug output cannot be hidden by
  equal-depth or later-depth suppression during diagnosis
- Revalidated after the cull/depth-state follow-up with
  `android\run-code-quality.ps1 -Fix`,
  `android\gradlew.bat :app:assembleDebug :app:testDebugUnitTest`, and
  `android\run_test.ps1 -ScriptName test_launch_to_automap.json5 -Game d2`
- Revalidated after the split-diagnostics follow-up with
  `android\run-code-quality.ps1 -Fix`,
  `android\gradlew.bat :app:assembleDebug :app:testDebugUnitTest`, and
  `android\run_test.ps1 -ScriptName test_launch_to_automap.json5 -Game d2`
- Implemented in D1 and D2: Android plain `metl154` merge draws now take a
  narrow CPU clip path before the existing `g3_draw_tmap_2` body when the
  source polygon carries clip codes. The helper uses `clip_polygon(...)`,
  preserves interpolated `u/v`, and reuses mono `p3_l` for clipped lighting so
  the experiment stays scoped to the known-bad metl154 path
- Revalidated after the merge-clip follow-up with
  `android\run-code-quality.ps1 -Fix` and
  `android\gradlew.bat :app:assembleDebug :app:testDebugUnitTest`
- New highres-only log
  (`android\temp_game_logs\debuglog_20260414_102547.txt`) shows that the
  merge-clip helper is active but incomplete. Some recurring metl154 sequences
  now stop after `[metl154face]` with no later draw-time lines, which means
  they are getting culled before the OGL draw body, but the user still reports
  the same clipping-swapping behavior on the surviving draws
- That same log also shows `ETC2 upload: metl154 512x512 fmt=0x9278`, so the
  highres path is still uploading metl154 with an RGBA ETC2 internal format
  rather than an RGB-only format. Combined with the earlier stock 64x64 log,
  this weakens the texture-pack and compression-format theories further
- Implemented in D1 and D2: new `[metl154wrap]` logging for Android plain
  metl154 merge draws. The draw path now records the cached overlay
  `wrapstate`, reads the live `GL_TEXTURE_WRAP_S/T` values on the bound
  overlay texture, and force-restores both axes to `GL_REPEAT` if the actual
  state is not repeat
- Revalidated after the wrap-state follow-up with
  `android\run-code-quality.ps1 -Fix` and
  `android\gradlew.bat :app:assembleDebug :app:testDebugUnitTest`
- Emulator smoke validation is currently blocked in this session because
  `adb devices` returned no attached devices, so
  `android\run_test.ps1 -ScriptName test_launch_to_automap.json5 -Game d2`
  could not complete
- Revalidated after the metl154 experiment-control follow-up with
  `android\run-code-quality.ps1 -Fix` and
  `android\gradlew.bat :app:assembleDebug :app:testDebugUnitTest`
- A follow-up emulator smoke test is still blocked in this session because the
  Android SDK `adb.exe devices` check also returned no attached devices
- New stock log `android\temp_game_logs\debuglog_20260414_142906.txt`
  corrected a key flag-reading mistake: in this codebase
  `BM_FLAG_SUPER_TRANSPARENT` is `2`, not `8`, so metl154's `flags=0x9`
  means ordinary transparent plus RLE, not super-transparent
- That same log shows metl154 on the stock path with no active mod pack,
  `tex_handle=295`, `tex_wh=64x64`, `shader=plain`, `mask=0`, and
  `Stock mask check: metl154 ... super=0`, which means the missing-mask
  theory is no longer viable for metl154 itself
- The draw-time diagnostics in that log also show `src254=0` and large
  `src255` counts on metl154 samples, which means the texture is using
  ordinary transparent pixels (`255`) rather than super-transparent
  pixels (`254`)
- Implemented in D1 and D2: new `[metl154alpha]` logging that estimates a
  wrapped bilinear alpha sample from the original source bitmap at the
  representative overlay UV, including the four contributing texels and
  whether the sample crosses the repeat seam on U or V
- Implemented in D1 and D2: `stock-native` `[metl154src]` logging now also
  fires on the non-PNG stock bitmap path, and palette-source logs now include
  edge transparent counts plus left-right and top-bottom alpha seam mismatch
  counts
- Revalidated after the latest source/filter diagnostics with
  `android\run-code-quality.ps1 -Fix` and
  `android\gradlew.bat externalNativeBuildDebug`

### Latest Direction

- The current best theory is no longer "exact later draw reuses the same face
  geometry" by itself. The stronger lead is that the Android plain metl154
  merge draw may be getting neutralized by GL state, especially culling or
  depth behavior that was not previously logged
- The next device capture should focus on whether the new build makes the
  Alpha/RGB debug modes visibly obvious and whether `[metl154state]` shows a
  winding or depth-state condition that matches the bad angles
- New log `android\temp_game_logs\debuglog_20260414_075137.txt` weakens the
  cull-state theory further. In the first session the 512 pack is active and
  metl154 uploads as `512x512`, while a later session runs with no
  `.active_mod_paths` and metl154 falls back to stock `64x64`; both sessions
  still show the same plain-shader metl154 behavior
- In both the 512 and stock sessions, the metl154 draws remain
  `shader=plain`, `mask=0`, expected and active programs match, and there are
  still no `[metl154cover]` or `shader=mask` lines
- In both sessions, `[metl154state]` shows `force_cull_off=1` on the affected
  plain metl154 draws, which means the new Android cull disable is active but
  does not remove the visual problem by itself
- The newest likely root cause is shifting toward quad triangulation or
  projected winding rather than texture content or later overwrite. The stock
  session repeatedly logs some metl154 quads with `area=0.0` while the OGL
  path still renders the face as `GL_TRIANGLE_FAN`, which is consistent with a
  bow-tie or otherwise unstable projected quad split
- New log `android\temp_game_logs\debuglog_20260414_084106.txt` sharpens that
  result. It contains both a stock session (`No .active_mod_paths`,
  `tex_handle=295`, `tex_wh=64x64`) and a later 512 pack session (`Mounted mod
  ... d2-hires-512-textures-ktx2.dxa`, `tex_handle=1521`, `tex_wh=512x512`).
  The same early metl154 faces reproduce the same geometry signatures in both
  runs: `seq=2` and `seq=9` collapse to `area=0.0` with all `sx/sy=0`, while
  `seq=4` keeps two projected vertices at `0,0` and one flat triangle no
  matter which diagonal is tested
- The later non-`pick=same` cases in that log do not weaken the geometry
  theory. They still show one vertex already collapsed to `0,0` and huge
  off-screen coordinates, which means the diagonal heuristic only decides which
  broken split is less bad after clipping pressure has already distorted the
  quad
- The stronger root-cause candidate is now the merged-wall draw path itself.
  `render.c` calls `g3_draw_tmap_2(...)` directly for `bm2` texmerge overlays,
  bypassing the normal `g3_draw_tmap` clipping path in `3d/draw.c` that routes
  polygons through `clip_polygon(...)` before drawing. `g3_draw_tmap_2` then
  submits the raw 3D polygon as `GL_TRIANGLE_FAN`, so Android/GLES ends up
  clipping and triangulating these quads on the GPU instead of receiving an
  already clipped polygon from the software path
- The next step should be a targeted fix experiment rather than another broad
  logging round: clip/split texmerge quads before `g3_draw_tmap_2` submits
  them, or otherwise route merged walls through the same software clipping path
  as normal tmap draws so the GPU does not have to resolve unstable fan
  triangulation at the near plane
- That first fix experiment is now implemented for Android plain `metl154`
  merge draws. The next useful data point is a fresh device or emulator retest
  from the formerly bad door angles. If the bars still disappear, the follow-up
  should focus on either preserving full RGB lighting across clipped temp
  vertices or broadening the clipped merge path beyond the metl154-specific
  gate
- The newest log shifts the lead again: clipping alone is not enough. The more
  specific remaining candidate is stale live wrap state on the overlay texture.
  Many of the bad metl154 draws use overlay UVs outside `[0,1]`, so they rely
  on repeat wrap. If the engine cache still says repeat while GL is actually
  clamped, the overlay can appear to swap or disappear as the camera moves
- The next device capture should look first at `[metl154wrap]`. If the bad
  draws show `forced=1` or `actual` values other than `0x2901`, then a real
  wrap-state mismatch has been caught in the act. If all of those lines stay
  at repeat and the visual bug is unchanged, the wrap theory weakens quickly
  and the next step should move toward shader-side sampling or precision
  diagnostics instead of more clip-path work
- The newest stock log weakens both the mask theory and the broader GL-state
  theory. `[metl154gl]`, `[metl154state]`, and representative `[metl154split]`
  lines remain stable, while the corrected flag interpretation now shows that
  metl154 was never entering the super-transparent mask path in the first
  place
- The stronger shared candidate across the last few weeks of changes is now
  Android filtering on a sparse transparent overlay: explicit mipmap
  generation, anisotropy-driven mip use, linear minification, and repeated UVs
  on metl154 all line up with the stock run's `mips=1`, `filt=9985/9729`, and
  UV ranges that regularly cross outside `[0,1]`
- A particularly relevant analogy already exists in the code: Android now
  avoids mipmapping font atlases because mipmaps average thin opaque strokes
  with surrounding transparency and destroy glyph alpha at lower mip levels.
  metl154 has the same sparse-alpha shape, just in a wall overlay instead of a
  font texture
- The next useful capture should focus on the new `[metl154src]` and
  `[metl154alpha]` lines. If seam mismatch counts are high and the filtered
  alpha sample drops around wrap crossings, the next experiment should be a
  narrow filter or mipmap exception for metl154-like transparent overlays
  rather than more geometry surgery
- Logging should stay additive for the rest of this diagnosis phase. The goal
  now is to keep the current metl154 instrumentation set and build on it until
  the root cause is confirmed

### Current Tranche Plan

- [x] Add a second in-game Video Info overlay button for metl154 experiment
  cycling and keep both metl154 controls synced from native state
- [x] Add transition logging for `metl154_mode` and the new
  `metl154_experiment` setting in JNI and expose the new value through
  introspection and automation `set_debug`
- [x] Add D1 and D2 GL-thread metl154-only cache invalidation so experiment
  changes force a live reload without flushing unrelated textures
- [x] Add additive metl154 experiment-path logging covering KTX2,
  decoded-RGBA, no-mipmap, and stock-fallback paths
- [ ] Revalidate Android code quality, debug build, unit tests, and an
  existing launch smoke test after the patch
- [x] Add metl154 projected quad-split diagnostics in D1 and D2 OGL helpers
- [x] Log fan-vs-alternate triangle signed areas beside the existing state log
- [x] Revalidate Android code quality, build, unit tests, and emulator smoke
  test after the patch
- [x] Analyze `android\temp_game_logs\debuglog_20260414_084106.txt` across
  stock and 512 sessions
- [x] Confirm whether the new split logs isolate texture content vs geometry
  instability
- [x] Update the phase note with the new texmerge-bypasses-clipping hypothesis
- [x] Analyze `android\temp_game_logs\debuglog_20260414_142906.txt` and
  correct the bitmap-flag interpretation for metl154
- [x] Add stock-path source logging and filtered-alpha sampling diagnostics in
  D1 and D2
- [ ] Capture a fresh stock run with the new `[metl154src]` and
  `[metl154alpha]` lines enabled

---

## Hypothesis 1: Back-face double rendering at shared walls (HIGH priority)

### Theory

Descent renders walls via portal-based segment traversal. A wall between
two segments has TWO sides (one in each segment) with OPPOSITE normals.
`render_side` culls back-faces by checking `v_dot_n0 >= 0`. At oblique
viewing angles, the dot product approaches zero for BOTH sides, and the
`>= 0` check lets BOTH pass. Both sides are drawn to the SAME Z depth
(shared vertices). With `glDepthFunc(GL_LEQUAL)`, the LAST drawn side
overwrites the first. If the back side has a different tmap2 (no overlay,
or a different overlay), the front side's overlay disappears.

This directly explains:
- Perspective-dependent flickering (dot product crosses zero as camera
  moves)
- Rock always 100% opaque (both sides have the same or similar tmap1)
- "Bars appear and disappear" (front side has metl154, back side does not)

### How to verify

Need to know: Is metl154 drawn on an OUTER wall (`children[side] == -1`,
only one side exists) or on a SHARED wall between segments? Outer walls
cannot have back-face overlap.

### Logging to add

**In `render_face()` -- guarded by `tmap2` matching metl154:**
```c
// Log every render_face call for metl154 overlay within a frame
// Need: segnum, sidenum, tmap1, tmap2, wid_flags, children[sidenum]
// Also need a per-frame counter to see if the same physical wall is drawn
// multiple times per frame
```

Specific fields:
- `segnum`, `sidenum`
- `tmap1`, `tmap2` (including orientation bits)
- `wid_flags` from WALL_IS_DOORWAY
- `Segments[segnum].children[sidenum]` -- -1 means outer wall (no overlap
  possible), >= 0 means shared wall (overlap possible)
- `v_dot_n0` -- the face normal dot product (how close to zero = how
  oblique)
- A frame counter to detect if the same (segnum,sidenum) pair is drawn
  more than once per frame

**Add to existing metl154diag line or as a separate `[metl154face]` line.**

### Why this is new

Prior diagnostics all focused on the texture data and shader parameters
within `g3_draw_tmap_2`. This hypothesis is about the CALLER
(`render_face` / `render_side`) and the scene-level draw order. The issue
may not be in how a single face is drawn, but in which faces are drawn
and in what order.

---

## Hypothesis 2: Three-pass rendering and WID classification (MEDIUM priority)

### Theory

When `ClassicDepth` is OFF (depth test enabled), the renderer uses 3
passes:
1. All geometry, with `glAlphaFunc(GL_GEQUAL, 0.8)` for transparent walls
2. Objects
3. Transparent walls again, with normal alpha

If metl154 walls are classified as `WID_TRANSPARENT_WALL` or
`WID_TRANSILLUSORY_WALL`, they are drawn TWICE (passes 1 and 3). Pass 1
uses high alpha test that on GLES is only relevant to the shim shader
(g3_draw_tmap path), not the external merge shader (g3_draw_tmap_2 path).
But if the gles3_shim's alpha emulation somehow bleeds into the external
shader, or if the wall is draw with g3_draw_tmap (texmerge fallback), this
could cause issues.

### Logging to add

- Track which render pass (1/2/3) is active via a global, and include it
  in the metl154diag log line
- Log the `wid_flags` for metl154 walls in render_face -- particularly
  check if it's WID_WALL (2) or WID_TRANSPARENT_WALL (6) or another value
- Note: this is partially covered by Hypothesis 1 logging (which also
  logs wid_flags)

---

## Hypothesis 3: GL texture binding contamination (MEDIUM priority)

### Theory

Between the texture binding in g3_draw_tmap_2 and the glDrawArrays call,
some GL state might be corrupted. For example, a prior draw call's
texture might still be bound to TEXTURE1, or the shader program might not
be the correct one.

### Logging to add

**Immediately before `glDrawArrays` in g3_draw_tmap_2 OGL_MERGE path:**
```c
// Query actual GL state (expensive, only for metl154 diagnostic)
GLint active_prog = 0, bound_tex0 = 0, bound_tex1 = 0;
glGetIntegerv(GL_CURRENT_PROGRAM, &active_prog);
glActiveTexture(GL_TEXTURE0);
glGetIntegerv(GL_TEXTURE_BINDING_2D, &bound_tex0);
glActiveTexture(GL_TEXTURE1);
glGetIntegerv(GL_TEXTURE_BINDING_2D, &bound_tex1);
glActiveTexture(GL_TEXTURE0); // restore
```

Fields to log:
- `active_prog` -- should match `ogl_prog_tex2` (or `ogl_prog_tex2m`)
- `bound_tex0` -- should match `bmbot->gltexture->handle`
- `bound_tex1` -- should match `bmovl->gltexture->handle`
- If ANY mismatch, the bug is GL state corruption

### Why this is new

Prior diagnostics queried the overlay texture's filter state but not the
actual binding state or active program at draw time.

---

## Hypothesis 4: Mipmap state inconsistency (MEDIUM priority)

### Theory

Prior diagnostic showed `mips=0` and `filt=9729/9729` (GL_LINEAR). This
is inconsistent with both upload paths:
- texfilt > 0 upload: would produce mipmap filter + has_mipmaps=1
- texfilt == 0 upload: would produce GL_NEAREST + has_mipmaps=0

GL_LINEAR with has_mipmaps=0 suggests something modified the filter after
upload. If the texture has mipmaps auto-generated (e.g., by the
aniso/texfilt flush loop in ogl_start_frame) but has_mipmaps wasn't
updated, the filter might be wrong.

More importantly: if the texture DOES have mipmaps with GL_LINEAR min
filter (not GL_LINEAR_MIPMAP_LINEAR), then base-level-only sampling is
used. mipmap levels exist but are ignored. However, on some GLES drivers,
having mipmaps with a non-mipmap min filter can cause undefined behavior.

### Logging to add

- Log `GameCfg.TexFilt` at the time of metl154 diagnostic
- Log the overlay texture's OpenGL handle AND its `bytes` and `lw` fields
  to verify it's the expected texture
- Log `ogl_aniso_level` to check if AF triggered mipmap generation
- On the FIRST metl154 draw per level load, do a `glGetTexLevelParameteriv`
  query to check if mip level 1 exists on the overlay texture:
  ```c
  GLint mip1_w = 0;
  glActiveTexture(GL_TEXTURE1);
  glGetTexLevelParameteriv(GL_TEXTURE_2D, 1, GL_TEXTURE_WIDTH, &mip1_w);
  glActiveTexture(GL_TEXTURE0);
  // mip1_w > 0 means mip level 1 exists
  ```

---

## Hypothesis 5: Fragment-level alpha diagnostic via shader (HIGH priority)

### Theory

All prior diagnostics sampled the CPU-side bitmap data. We have never
verified what the shader ACTUALLY RECEIVES at the fragment level. If the
GPU texture upload, mipmap generation, or filtering produces different
alpha values than expected, we would only see it from the shader side.

### Implementation approach

**Option A: Debug color output (visual diagnostic)**

Add a debug uniform `utex2_debug` to the `ogl_prog_tex2` shader. When
set to 1, output overlay alpha as visible color:
```glsl
if (utex2_debug == 1) {
    gl_FragColor = vec4(ovl.a, 0.0, 0.0, 1.0);
    return;
}
if (utex2_debug == 2) {
    gl_FragColor = vec4(ovl.rgb, 1.0);
    return;
}
```

Mode 1: overlay alpha as red intensity. Opaque bars = bright red,
transparent gaps = black. If bars flicker, the red would flicker too,
confirming the alpha changes at fragment level.

Mode 2: overlay RGB ignoring alpha. Shows what color the shader sees for
the overlay regardless of transparency. If this is stable, the issue is
alpha-only.

Toggle via a new JNI-accessible debug flag, similar to the existing
debug_tex_overlay system.

**Option B: Sample-and-log (more data, less visual)**

In the shader, if the fragment is at a specific screen-space position
(e.g., center of the face), write the sampled values to a small SSBO or
pixel readback. This is more complex but gives exact values.

Recommend starting with Option A -- it's simpler and gives immediate
visual feedback.

---

## Hypothesis 6: UV coordinate degeneracy (LOW priority)

### Theory

At certain camera angles, the projected vertex positions might produce
degenerate or very small UV ranges for the overlay. If the UV range
collapses to a single point, all fragments sample the same texel. If that
texel happens to be transparent, the whole face shows rock.

### Logging to add

Already partially covered by the existing metl154diag `ovl_uv` range. But
should verify:
- Log the actual per-vertex overlay UVs (not just min/max range)
- Check if any vertex has NaN or inf coordinates
- Log the UV range on EVERY metl154 draw (not just first) to see if it
  changes between "bars visible" and "bars invisible" states

---

## Hypothesis 7: Piggy bitmap paging invalidation (LOW priority)

### Theory

In `render_face`, the overlay bitmap is paged in BEFORE the base bitmap:
```c
PIGGY_PAGE_IN(Textures[tmap2&0x3FFF]);
bm2 = &GameBitmaps[Textures[tmap2&0x3FFF].index];
PIGGY_PAGE_IN(Textures[tmap1]); // re-page base in case flush
```
But tmap1's second page-in could flush tmap2. The bitmap pointer `bm2`
still points to the right struct, but the struct's data may have been
replaced with a different bitmap or placeholder.

### Logging to add

- In g3_draw_tmap_2: verify `bmovl->bm_flags` still has BM_FLAG_TRANSPARENT
  set (not BM_FLAG_PAGED_OUT)
- Log `bmovl->gltexture->handle` and compare with a cached "first seen"
  handle -- if the handle changes between frames, the texture is being
  recreated

---

## Recommended implementation order

1. **Hypothesis 1** (render_face logging) -- highest ROI, tests a
   completely new theory about draw order that prior phases never examined
2. **Hypothesis 5** (shader debug output) -- gives ground truth about what
   the GPU actually computes, eliminates all CPU-side sampling ambiguity
3. **Hypothesis 3** (GL state query) -- quick to add, immediate pass/fail
4. **Hypothesis 4** (mipmap state) -- extends existing diagnostic with
   a few extra fields
5. **Hypothesis 2** (pass tracking) -- easy to add alongside H1 logging
6. **Hypothesis 6/7** (UV and paging) -- extend existing diagnostics

Goal: add ALL of these in a single build so we get comprehensive data
from one device test session instead of iterating one hypothesis at a time.

---

## Key principle for this round

Every previous phase focused on the texture content and shader parameters
as seen from g3_draw_tmap_2. But the bug's perspective-dependent nature
strongly suggests the issue is UPSTREAM -- in which faces are drawn, how
many times they're drawn, and in what order. Hypothesis 1 (back-face
double rendering) is the most promising new direction.

Even if H1 turns out to be wrong, the render_face logging will tell us
whether the metl154 face is drawn exactly once per frame with consistent
parameters, which eliminates a large class of draw-order hypotheses.
