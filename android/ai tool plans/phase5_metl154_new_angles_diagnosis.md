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
- Implemented in D1 and D2: new `[metl154clip]` logging on the Android plain
  metl154 merge-clip path plus screen-space `[metl154coverbox]` overlap
  logging for later draws that do not hit the exact-vertex `[metl154cover]`
  path
- New stock log `android\temp_game_logs\debuglog_20260414_185644.txt` keeps
  the no-mod stock path and still exercises `alpha_raw`; the first repeated
  overlap signal is `cover_bot=rock296` on `metl_seq=1`, while later camera
  positions also produce `rock346`, `rock331`, and even later `metl154`
  overlaps
- Implemented in D1 and D2: a shared Android wall-draw context now flows from
  `render_side()` into OGL metl154 tracking so `[metl154cover]` and
  `[metl154coverbox]` can log both the tracked metl154 face and the later
  covering face by segment, side, and face instead of only by bitmap name
- Revalidated after the context-rich overlap follow-up with
  `android\run-code-quality.ps1 -Fix`, `android\gradlew.bat bundleDebug`, and
  `android\gradlew.bat testDebugUnitTest`
- Launch smoke validation is still blocked in this session because
  `C:\local\android-sdk\platform-tools\adb.exe devices` returned no attached
  emulator or device
- New stock log `android\temp_game_logs\debuglog_20260414_193658.txt` keeps
  the no-mod stock path, still exercises stock `alpha_raw`, and shows the
  same clip pattern as the prior capture: `seq=1` and `seq=3` survive
  `stage=clip` while `seq=2`, `seq=4`, and `seq=9` repeatedly die as
  `stage=culled` with `behind=1`
- That same `193658` log is the first capture where the context-rich
  `[metl154coverbox]` lines isolate stable later cover faces instead of only
  cover bitmap names. The repeated early pair is metl `seg=32 side=0 face=0`
  covered by `seg=30 side=2 face=0` with `cover_bot=rock296`, and the later
  repeated pair is metl `seg=83 side=1 face=0` covered by
  `seg=83 side=2 face=0` with `cover_bot=rock346`
- Implemented in D1 and D2: a new Android-only `cover_skip` experiment that
  suppresses only those two later cover faces, and only when the matching
  metl154 face was tracked earlier in the same frame. The experiment is wired
  through JNI and the Video Info overlay as `m154 exp: CoverSkip`
- Revalidated after the `cover_skip` follow-up with
  `android\run-code-quality.ps1 -Fix`, `android\gradlew.bat bundleDebug`, and
  `android\gradlew.bat testDebugUnitTest`
- New stock log `android\temp_game_logs\debuglog_20260414_203139.txt` keeps
  the no-mod stock path, exercises the full experiment cycle through
  `cover_skip`, and shows the same clip pattern as `193658`: `seq=1` and
  `seq=3` survive `stage=clip` while `seq=2`, `seq=4`, and `seq=9` repeatedly
  die as `stage=culled` with `behind=1`
- That same `203139` log confirms that `cover_skip` is active and working for
  the original two pairs. The repeated `[metl154exp] cover_skip ...` hits are
  still `metl 32/0/0 -> cover 30/2/0` (`rock296`) and
  `metl 83/1/0 -> cover 83/2/0` (`rock346`), and those exact later faces no
  longer reappear as `[metl154coverbox]` overlaps after the toggle
- The same `203139` capture also shows a second stable later-cover family that
  survives beyond the original pair list: `metl 32/2/0 -> cover 82/1/0`
  (`rock331`), `metl 28/2/0 -> cover 82/3/0` (`rock331`),
  `metl 29/2/0 -> cover 82/4/0` (`metl154`), and
  `metl 28/0/0 -> cover 28/1/0` (`rock346`)
- Implemented in D1 and D2: a second Android-only `cover_skip2` experiment
  that keeps the original `cover_skip` pair list intact and adds that broader
  stable later-cover family. The experiment is wired through JNI and the Video
  Info overlay as `m154 exp: CoverSkip2`
- Revalidated after the `cover_skip2` follow-up with
  `android\run-code-quality.ps1 -Fix`, `android\gradlew.bat bundleDebug`, and
  `android\gradlew.bat testDebugUnitTest`

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

### Transparent Wall Semantics Check

- `wall_is_doorway()` in both D1 and D2 already classifies wall sides as
  `WID_TRANSPARENT_WALL` or `WID_TRANSILLUSORY_WALL` based on bitmap
  transparency and wall type, so the engine does have an explicit
  transparent/pass-through wall semantic instead of relying on visual guesses
- `find_vector_intersection()` only traverses those sides when callers set
  `FQ_TRANSWALL` or `FQ_TRANSPOINT`, and `FQ_TRANSPOINT` samples the actual hit
  texel via `check_trans_wall()` using the merged wall bitmap UVs. This fits
  the user report that shots already pass through the bar holes even while the
  Android renderer still shows rock behind them
- The D1 and D2 renderers also treat those same sides specially in the OGL
  three-pass wall draw, which confirms that the transparent-wall
  classification is not limited to collision logic
- The Android diagnostics already have a stable face identity. `render.c`
  populates `g_android_draw_face_ctx.seg/side/face` for every rendered face,
  including split triangles, and the existing metl154 logs already print that
  triple. The missing piece is user-visible surfacing, so the overlay should
  display that same `seg/side/face` identifier for direct correlation with the
  logs
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
- New log `android\temp_game_logs\debuglog_20260414_185644.txt` makes the
  broader overlap signal concrete. `[metl154clip]` shows visible `seq=1` and
  `seq=3` draws surviving clip while `seq=2`, `seq=4`, and `seq=9` often die
  as `stage=culled` with `behind=1`, so the Android metl154 clip helper is
  active and also identifies which recurring candidates never reach the draw
  body
- The repeated early `[metl154coverbox]` hits are not one-off noise. In the
  stock session the same `metl_seq=1` face is overlapped for hundreds of
  frames by a later `cover_shader=single cover_bot=rock296` draw, and later
  views repeat the pattern with other later covers such as `rock346`,
  `rock331`, and `metl154`
- That still does not isolate the root cause by itself because the old
  overlap log only names the covering bitmap. It cannot distinguish whether
  one bitmap comes from one wall face or several different later faces
- The next capture should use the new context-rich overlap logging now in the
  tree. `[metl154cover]` and `[metl154coverbox]` will now emit both the
  metl154 face identity and the later covering face identity, which should let
  the next stock run answer whether a specific segment, side, and face is
  consistently drawing over the bad metl154 wall
- New log `android\temp_game_logs\debuglog_20260414_193658.txt` answers that
  question. The repeated early overlap is not just another `rock296` draw in
  the abstract; it is specifically `metl seg=32 side=0 face=0` being covered
  by `cover seg=30 side=2 face=0`. The later repeated overlap is likewise
  stable: `metl seg=83 side=1 face=0` is being covered by
  `cover seg=83 side=2 face=0` with `rock346`
- Those stable pairs are specific enough to move from logging to a narrow
  behavior experiment. The new `cover_skip` mode now suppresses only those
  identified later cover draws, and only when the matching metl154 face was
  tracked earlier in the same frame, so the next device run can answer the
  practical question directly: do the bars return when those exact later cover
  faces are skipped
- New log `android\temp_game_logs\debuglog_20260414_203139.txt` shows that
  the answer is "partly". `cover_skip` definitely runs and suppresses the two
  originally targeted later faces, but the broader view still accumulates a
  second stable cover family around `seg=82` plus the local
  `28/0/0 -> 28/1/0` `rock346` overlap
- The next useful comparison is no longer default versus `cover_skip`; it is
  `cover_skip` versus a broader same-frame suppression list. The new
  `cover_skip2` mode now keeps the original two pairs and also suppresses the
  `82/1/0`, `82/3/0`, `82/4/0`, and `28/1/0` later covers so the next capture
  can answer whether those remaining stable faces account for the rest of the
  visual failure
- The `203139` runtime evidence also exposes a more direct same-draw question
  than the later-cover branch. The recurring bad faces are all solid-wall
  faces with `child=-1`, `wid=2`, `tmap1=rock313`, `tmap2=metl154`, and
  draw-time `super=0`, which means `rock313` is the same face's bottom
  texture in the ordinary plain-alpha merge path rather than a child-segment
  render-past surface
- The next logging tranche should therefore pivot from later-cover suppression
  to wall classification and merge semantics: log exactly why those metl154
  faces are staying on the non-super path and whether the representative
  overlay alpha sample would intentionally expose the same-face bottom texture
  inside the plain `tex2` shader
- New log `android\temp_game_logs\debuglog_20260414_233303.txt` corrects one
  process assumption from the previous tranche: it is not a stock-only capture.
  The run starts in `default`, then cycles through the full experiment ladder
  up to `overlay_only`, so the same file now contains an in-log before/after
  comparison instead of only another stock snapshot
- The user also clarified the scene semantics for that run: most visible
  `metl154` overlays sitting on rocks are correct and should continue showing
  rock beneath them. Only one visible case is wrong and should instead show a
  transparent no-rock opening, which makes any global "remove same-face bottom
  underlay everywhere" fix too risky
- The strongest new discriminator in `233303` is not wall metadata by itself.
  On the stock path, `seg=32 side=0 face=0` and `seg=28 side=0 face=0` both
  repeatedly log `sample_alpha=0.000` with `bottom_mix=1.000`, while
  `seg=83 side=1 face=0/1` stay opaque and `seg=83 side=3 face=0/1` only drop
  into the transparent-underlay path at some viewpoints. Those repeated faces
  still share the same broad `child=-1`, `wid=2`, ordinary-transparent wall
  classification, so the bad case is not isolated by those fields alone
- The useful distinction shows up only when the log is read in frame-matched
  windows with `coverbox` context. Around overlay-only frames `400-403`,
  `seg=28 side=0 face=0` is later re-covered by `cover seg=24 side=0 face=0`
  with `cover_bot=rock313`, while `seg=32 side=0 face=0` is not. In the later
  overlay-only window around frames `437-441`, the repeated coverbox hits move
  to `seg=83 side=1 face=0 -> cover_bot=rock346` and
  `seg=83 side=1 face=1 -> cover_bot=rock296`, while neither
  `seg=28 side=0 face=0` nor `seg=32 side=0 face=0` receives a later coverbox
  hit in that view
- The current best lead is therefore scene coverage context rather than wall
  flags: some faces that look identical in the stock plain-alpha logs can still
  end up visually backed by other scene draws, while others cannot. The next
  safe step is to map the user's one semantically wrong scene element to a
  specific logged face before adding another experiment or making a code fix

### Software Renderer Semantics Check

- A classic-renderer pass through `wall.c`, `fvi.c`, `texmerge.c`,
  `ntmap.c`, `scanline.c`, `render.c`, `laser.c`, and `physics.c` narrows the
  engine-intent question further than the Android OGL logs alone
- In the software path, walls with `tmap2 != 0` do not render as two live
  layers. `render.c` calls `texmerge_get_cached_bitmap(tmap1, tmap2)`, so the
  visible result comes from a premerged bitmap rather than a separate overlay
  pass
- `merge_textures_new()` handles ordinary transparent overlay pixels by
  copying the bottom texture pixel into the merged bitmap. In other words,
  normal transparent `metl154` texels are explicitly defined to show the same
  face's `tmap1` rock underlay in the classic renderer
- `merge_textures_super_xparent()` is the distinct hole path. It preserves
  true `TRANSPARENCY_COLOR` pixels in the merged bitmap, and the software
  scanline code only skips draw on those surviving transparent pixels
- `wall.c` lines up with that split: for `tmap2` overlays, the wall is only
  classified as `WID_TRANSPARENT_WALL` when the overlay bitmap is
  `BM_FLAG_SUPER_TRANSPARENT`. `fvi.c` then uses the merged bitmap through
  `check_trans_wall()`, and moving weapon objects include `FQ_TRANSPOINT`, so
  projectile pass-through behavior follows the same true-hole semantics rather
  than the ordinary transparent-underlay semantics
- That means the broad guess "maybe this face is just marked to hide the
  rock" is only plausible in the narrow super-transparent sense. The classic
  renderer does have a built-in way to make a wall overlay behave like a real
  hole, but ordinary transparent overlay pixels are not that mechanism
- This leaves one important contradiction to reconcile with the earlier
  Android captures: the runtime metl154 logs reported `ovl_super=0` and
  ordinary transparent behavior, while the user reports that shots pass
  through the visible opening. Either the semantically wrong visible case is
  not the exact face those earlier samples represented, or there is still a
  remaining identification or asset-flag detail missing from the current logs

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
- [x] Revalidate Android code quality, debug build, unit tests, and an
  existing launch smoke test after the patch
- [x] Add metl154 projected quad-split diagnostics in D1 and D2 OGL helpers
- [x] Log fan-vs-alternate triangle signed areas beside the existing state log
- [x] Revalidate Android code quality, build, unit tests, and emulator smoke
- [x] Review `debuglog_20260414_160646.txt` across both highres and stock
  sessions and compare the actual metl154 load path chosen for each
  experiment mode
- [x] If the current experiments are visually inert, add one narrower
  metl154-only experiment that changes runtime sampling behavior rather than
  only the texture source path, then log it alongside the existing toggles
  Current choice: add an `alpha_raw` mode that keeps the same source path but
  disables the metl154 plain-pass `0.5` alpha cutoff so device testing can
  isolate shader-side alpha interpretation from texture-source selection
- [x] Analyze `android\temp_game_logs\debuglog_20260414_181819.txt` and
  confirm that stock-path runs still exercised `alpha_raw`, while the
  metl154 state and quad-split traces stayed stable and no exact
  `[metl154cover]` overwrite hits appeared
- [x] If the `alpha_raw` stock run is still visually inert, zoom out to other
  parts of the draw path and add broader metl154 logging there
  Current choice: log clipped merge routing in `ogl_clip_and_draw_metl154_merge`
  and add a new `[metl154coverbox]` screen-overlap log so later draws that do
  not match the exact vertex set can still be correlated against a tracked
  metl154 face
- [x] Analyze `android\temp_game_logs\debuglog_20260414_185644.txt` and
  confirm whether the broader clip and overlap logs isolate a stable later
  cover candidate
  Findings: the run is still stock/no-mod and still exercises stock
  `alpha_raw`; early frames repeatedly log `cover_bot=rock296` over
  `metl_seq=1`, while later views also surface `rock346`, `rock331`, and
  `metl154` overlaps
- [x] If the broader overlap logs are still too coarse, carry exact wall draw
  context into the metl154 tracking records and cover logs
  Current choice: add a shared `android_draw_face_context` in the D1 and D2
  render and OGL code so the next `[metl154cover]` and `[metl154coverbox]`
  lines include both metl and cover segment, side, face, child, side type,
  and tmap identity
- [x] Analyze `android\temp_game_logs\debuglog_20260414_233303.txt` and
  compare the recurring metl faces across the stock/default and overlay-only
  windows in the same capture
  Findings: `233303` cycles through all experiment modes up to
  `overlay_only`; `seg=32 side=0 face=0` and `seg=28 side=0 face=0` both stay
  `sample_alpha=0.000 bottom_mix=1.000` on the stock path, but frame-matched
  `coverbox` lines show that `28/0/0` gains later rock cover in some windows
  while `32/0/0` does not, and later windows shift the stable cover hits to
  `83/1/0` and `83/1/1`
- [ ] Correlate the user's one semantically wrong visible scene element with a
  specific logged face before adding another experiment or attempting a code
  fix
- [x] Review the classic software renderer and collision paths for how
  transparent versus super-transparent wall overlays are meant to behave
  Findings: ordinary transparent texels in `texmerge` reveal the same face's
  bottom texture, while only super-transparent texels survive as true holes
  for software rendering and `FQ_TRANSPOINT` traversal
- [ ] Revalidate Android code quality, debug build, unit tests, and the
  launch smoke path after any follow-up experiment patch
  2026-04-14 follow-up status: `android\run-code-quality.ps1 -Fix`,
  `gradlew.bat bundleDebug`, and `gradlew.bat testDebugUnitTest` all passed;
  the launch smoke path is still pending because `adb devices` returned no
  connected emulator or device in this session
  2026-04-14 broader-logging status: `android\run-code-quality.ps1 -Fix`,
  `gradlew.bat bundleDebug`, and `gradlew.bat testDebugUnitTest` all passed
  again after the clip/coverbox logging patch; the launch smoke path remains
  pending because the SDK-configured `adb.exe devices` output showed no
  attached emulator or device in this session
  2026-04-14 context-rich overlap status: `android\run-code-quality.ps1 -Fix`,
  `gradlew.bat bundleDebug`, and `gradlew.bat testDebugUnitTest` all passed
  after the wall-context follow-up; the launch smoke path remains pending
  because `C:\local\android-sdk\platform-tools\adb.exe devices` showed no
  attached emulator or device in this session
  2026-04-14 cover-skip status: `android\run-code-quality.ps1 -Fix`,
  `gradlew.bat bundleDebug`, and `gradlew.bat testDebugUnitTest` all passed
  after the targeted cover suppression follow-up; the launch smoke path
  remains pending because `C:\local\android-sdk\platform-tools\adb.exe`
  `devices` again showed no attached emulator or device in this session
  2026-04-14 cover-skip2 status: `android\run-code-quality.ps1 -Fix`,
  `gradlew.bat bundleDebug`, and `gradlew.bat testDebugUnitTest` all passed
  after the broader stable-pair suppression follow-up; the launch smoke path
  remains pending because `C:\local\android-sdk\platform-tools\adb.exe`
  `devices` still showed no attached emulator or device in this session
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
- [x] Capture a fresh stock run with the new `[metl154clip]` and
  `[metl154coverbox]` lines enabled
- [x] Capture a fresh stock run with the new context-rich `[metl154cover]`
  and `[metl154coverbox]` lines enabled
- [x] Analyze `android\temp_game_logs\debuglog_20260414_193658.txt` and
  confirm whether the context-rich cover logs isolate stable later cover
  faces
  Findings: the run is still stock/no-mod and still exercises stock
  `alpha_raw`; the repeated early pair is `metl 32/0/0 -> cover 30/2/0`
  (`rock296`), while the later repeated pair is `metl 83/1/0 -> cover 83/2/0`
  (`rock346`)
- [x] If the new context-rich cover logs isolate stable later faces, add one
  narrow runtime experiment that suppresses only those later cover draws
  Current choice: add `cover_skip`, a same-frame face-pair experiment that
  skips only the identified `30/2/0` and `83/2/0` cover draws after their
  matching metl154 faces were tracked earlier in the frame
- [x] Capture a fresh stock run with the new `cover_skip` experiment enabled
  Findings: `android\temp_game_logs\debuglog_20260414_203139.txt` is still
  stock/no-mod, still exercises the full experiment cycle, and confirms that
  `cover_skip` suppresses the original `30/2/0` and `83/2/0` later cover
  faces while a broader stable cover family remains around `seg=82` and
  `cover 28/1/0`
- [x] If `cover_skip` removes only the original two later faces, add one
  broader same-frame suppression experiment without replacing the narrower
  mode
  Current choice: add `cover_skip2`, which keeps the original
  `32/0/0 -> 30/2/0` and `83/1/0 -> 83/2/0` pairs and also skips
  `32/2/0 -> 82/1/0`, `28/2/0 -> 82/3/0`, `29/2/0 -> 82/4/0`, and
  `28/0/0 -> 28/1/0`
- [ ] Capture a fresh stock run with the new `cover_skip2` experiment enabled
  2026-04-14 runtime status: blocked in this session because
  `C:\local\android-sdk\platform-tools\adb.exe devices` returned no
  attached emulator or device
- [x] Re-evaluate the repeated `203139` metl154 faces against wall
  classification and merge-path semantics instead of only later covers
  Findings: the bad faces are solid-wall `child=-1` / `wid=2` draws with
  `tmap1=rock313`, `tmap2=metl154`, and draw-time `super=0`, so the visible
  rock is coming from the same merged face rather than only from a later
  render-past cover draw
- [x] Add targeted D1/D2 logging for metl154 wall classification and
  plain-vs-super merge behavior
  Current choice: add caller-side `[metl154wall]` lines that explain why a
  face stayed on the ordinary wall path, plus draw-side `[metl154mix]` lines
  that interpret the representative overlay alpha sample as same-face bottom
  exposure versus masked final-alpha control
- [x] Analyze `android\temp_game_logs\debuglog_20260414_213343.txt` from a
  second phone against the new wall/mix logs
  Findings: the second phone reproduces the same stock-path behavior. The
  bad metl154 faces are still solid `wid=2` / `child=-1` wall draws with
  `ovl_real=0x9`, `ovl_super=0`, and `[metl154mix] path=plain_alpha_cutoff`
  showing `sample_alpha=0.000` and `bottom_mix=1.000`. The same log also
  cycled through `alpha_raw` and `cover_skip2`, and neither changes the core
  diagnosis because transparent metl pixels still expose the same-face
  `rock313` underlay whenever the sampled alpha lands at zero
- [x] Add one narrow runtime experiment that removes same-face bottom mixing
  for metl154 without changing the texture source path
  Current choice: add `overlay_only`, which keeps the stock metl154 source
  and alpha cutoff behavior but renders the overlay as scene-through-alpha
  instead of mixing `tmap1` underneath transparent metl pixels
- [ ] Capture a fresh stock run with the new `overlay_only` experiment enabled
  Goal: confirm whether removing same-face underlay contribution also removes
  the visible rock in the full-view bad case, which would make the remaining
  issue a semantics mismatch rather than a later-cover overlap problem

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
