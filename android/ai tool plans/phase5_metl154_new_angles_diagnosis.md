# Phase 5: metl154 rendering -- active plan

This file is intentionally short and forward-looking. Tranche-by-tranche
history, older hypotheses, and detailed log-by-log interpretations were moved
to `android\ai tool plans\metl154_prior_work.md`.

## Foundational Knowledge

**THIS IS NOT LEVEL CONTENT. THE BASE REDUX GAME ENGINE NEVER SHOWS THIS PROBLEM.**

**Treat this as an Android-specific rendering defect unless desktop reproduction is explicitly demonstrated.**

- Wrong scene element: a rock-textured sliver or face appears through, under,
  or mixed with the transparent `metl154` grate near the `82/4/0 <-> 83/4/0`
  portal area
- The grate's transparency semantics are otherwise correct. The failure is
  which pixels contribute in one visible case
- Only one visible case is semantically wrong. Most rock-plus-metl walls in
  the level are legitimate and must keep working
- Current tracked leaked source face is `83/3/1`. Treat that as the current
  source of leaked pixels in Android logs, not as proof that the final image
  is supposed to contain visible rock there

Observed problem: on Android, an extra rock-textured sliver or face appears underneath, on top of, or mixed with the transparent `metl154` grate near the `82/83` portal area. The grate works from both directions (there's no problem with transparency, or supertransparency - the grate is transparent, and that works), but it fails in terms of the rock texture blocking what would be a transparency, or being drawn on top of the rock. The extra rock strip disappears when the camera pans far enough that its right edge leaves view.

The task is to figure out why this happens: there were recently a number of opengl changes (such as a fairly involved gles shim), msaa/anisotropic filtering/texture filtering paths, GL shader changes.  The problem happens with both high res texture packs and the base textures, so it isn't that, although some rework was done *to support* high res textures, and that code could be suspect.

Some theories are that it's a corrupted 2nd texture entry for that face; that it's a corrupted coordinate for a different texture; that it's a problem with shaders holding stale data.  There could be other reasons.

Your task is to reason about why the game engine or graphics changes might have caused this, and to track it down.  So far, an enormous amount of logging has been added with little result, often because the AI tool was confused about which face was being drawn, or going down rabbit holes trying to blame some of the things I just listed.  It's ok to continue adding logging, but the most important thing is original reasoning about the cause in code.  Some of the below analysis shows a series of rabbit holes that, if this header text were in place before it was done, would have been skipped.

*the rock texture is not part of level geometry. I've verified this in a level editor and in the base unmodified redux game, and in the base game engine as well (the original, unmodified descent engine from interplay). do not go down rabbit holes with the assumption that the rock texture is supposed to be there*

Another constraint in your analysis - the rock texture appears to be correctly drawn, not stretched, not projected from some far away point or partial triangle, nothing like that. it's a simple square face with a simple square texture, which is drawn edge to edge, showing no odd effects that would be expected if it was being projected in some kind of transparency draw-behind problem.

## Prior Work Moved

- All earlier tranche plans, historical hypotheses, and detailed log analysis
  now live in `android\ai tool plans\metl154_prior_work.md`
- Keep this file for active constraints, failed branches, and next literal
  debugging steps only

## What Has Not Worked

- Texture source checks did not resolve the bug
  - Stock textures and highres packs both reproduce it
  - ETC2, decoded RGBA, and stock fallback experiments did not produce a
    decisive visual change
- Transparency and mask theories did not resolve it
  - The paged-out transparency-flag fix was real, but it did not remove the
    persistent Android artifact
  - metl154 in this case is ordinary transparent, not a super-transparent mask
- Simple GL state theories did not resolve it
  - Expected program and active program matched at draw time
  - Culling disable, depth-debug modes, wrap-state enforcement, and fragment
    precision upgrade did not remove the bug
- Simple later-overdraw theory did not resolve it
  - `cover_skip` and `cover_skip2` removed specific later faces but did not
    explain or remove the surviving bad scene
  - Portal traversal itself is working
- Narrow metl154-only clip work did not resolve it
  - Android-only software clipping for metl154 changed diagnostics but the
    user still saw the same defect
  - The clipped helper still ends in `GL_TRIANGLE_FAN`, so the main Android
    fan-triangulation asymmetry remains
- Content-level explanations did not resolve it
  - Editor checks, base Redux, and original Descent do not show the rock strip
    as intended scene content

## Current Picture

- Android defaults `GameArg.DbgAltTexMerge = 1`, so the reproduced default
  path is `render.c -> g3_draw_tmap_2()` for `tmap2` walls
- The old cached texmerge path still matters as a control branch, but it is
  not the default path under reproduction
- The current best fit is still an Android geometry-path defect around clipped
  `tmap2` walls, shared-edge behavior, near-plane behavior, and/or GPU fan
  triangulation
- The next round should still run the external AI tool's branches literally
  instead of pruning them early. That means doing the geometry-path work first,
  but also doing the texmerge/cache and upload provenance control branches

## Tranche Update (2026-04-16)

- Implemented in D1 and D2: new `METL154_EXPERIMENT_CLIP_ALL` branch for
  generic `tmap2` clip-path testing in `g3_draw_tmap_2()`, with Android UI and
  JNI wiring (`m154 exp: ClipAll`)
- Implemented in D1 and D2: merged-wall clip helper generalized to
  route-aware `tmap2` diagnostics instead of metl154-only logging
- Implemented in D1 and D2: new pre-GPU submission diagnostics
  `[metl154submit]` including route, pre/post clip code summary, repeated
  projected vertices, collapsed `0,0` counts, and per-vertex `sx/sy/codes/flags`
- Implemented in D1 and D2: deterministic fan-order summary in submit logs
  (`fan_tris` and `fan_head`) so clipped `nv > 4` captures show exact fan
  triangle submission order before any CPU triangulation rewrite
- Implemented in D1 and D2: new Android merge VBO upload diagnostics
  `[metl154upload]` with monotonic `upload_id`, buffer sizes/offsets, face
  identity, texture identity, and GL handles
- Implemented in D1 and D2: `g_metl154_draw_seq` now advances from
  `render_set_android_draw_face_context()` for metl154 faces and clip-all
  experiment faces so route and submit logs have stable `frame/pass/seq`
  identity
- Implemented in D1 and D2: new `METL154_EXPERIMENT_OLD_MERGE` control branch
  forces the old cached texmerge render path from Android experiment controls
  instead of relying on startup args only
- Implemented in D1 and D2: explicit old-path provenance line
  `[metl154exp] ... merge_impl=old_texmerge ...` on metl154 faces while
  `METL154_EXPERIMENT_OLD_MERGE` is active
- Implemented in Android shared controls: JNI clamp/name wiring and Video Info
  overlay cycling now include `m154 exp: OldMerge`
- Implemented in D1 and D2: texmerge cache slot-owner provenance in
  `texmerge.c` with Android-only metadata for first-owner, last-owner,
  creation frame, and last-use frame
- Implemented in D1 and D2: explicit texmerge cache lifecycle logs
  `[metl154texmerge] event=create|reuse ...` including slot index, face
  identity, texture identity, orient, owner lineage, and frame markers
- Validation: `android\run-code-quality.ps1 -Fix` passed
- Validation: `android\gradlew.bat :app:assembleDebug :app:testDebugUnitTest`
  passed
- Validation detail: Android unit test suites currently report zero failures
  and zero errors
- Validation note: Gradle requires JDK 17+ in-shell. One attempted run failed
  under JVM 8 until `JAVA_HOME` and `PATH` were pointed to
  `c:\local\jdk-21`; subsequent runs passed
- Integration smoke test status: blocked in this session because `adb devices`
  returned no connected or authorized emulator/device

## Current Best-Fit Root Cause (2026-04-16, post-log reanalysis)

The earlier April 14 polygon-offset hypothesis was incorrect. This defect
predates the debug instrumentation and the logs point back to the older
Android-only shader texmerge path.

### What old_merge actually changes

`METL154_EXPERIMENT_OLD_MERGE` moves the bad face from the Android
shader-based `g3_draw_tmap_2()` path back to the legacy CPU
`texmerge_get_cached_bitmap()` path followed by single-texture
`g3_draw_tmap()`. That is the key pre-debug branch difference.

### Why this now fits better than the April 14 theory

- The tracked leaking face is `83/3/1`, which is already a triangle with
  `clip=0`, so the bug is not explained by later quad fan-order diagnostics
  or near-plane clip rewriting
- The failing draw in the captured log is `texfilt=0`, `mips=0`, and
  `aniso=0`, so the current capture does not point to AF or trilinear mip
  behavior as the primary cause
- The bad face draws as `shader=plain` on the custom texmerge program while
  `portal83` draws as `shader=single` on the normal single-texture GLES3 shim
  path
- old_merge fixes the defect by moving only the rock-plus-metl face back to
  the legacy single-texture path. That strongly implicates the older shader
  texmerge path, not the later debug-only code

### Strongest pre-debug regression chain

- `1abf956` introduced the Android GLES3 shim and the separate external
  shader path used by `g3_draw_tmap_2()` on Android
- `2a83533` then forced `GameArg.DbgAltTexMerge = 1` on Android in
  `d1/d2 misc/physfsx.c`, making the shader texmerge path the reproduced
  default instead of the legacy CPU texmerge path
- `00c37b5` further reworked this Android texmerge path for hires and mask
  handling, but the important regression is that Android stopped using the
  legacy texmerge path by default

### Current best-fit mechanism

The best fit is that Android shader texmerge is not texel-identical to the
legacy CPU texmerge on this metl154 shared-edge case.

- `texmerge_get_cached_bitmap()` resolves the overlay in software texel space
  first, using exact palette-index transparency rules in `merge_textures_new()`
- The shader path instead samples base and overlay textures separately in
  hardware and resolves transparency after those lookups
- On the tracked pair, the shared-edge metl154 UVs are close but not identical
  modulo repeat: the portal edge is about `u=1.183` and the leaking rock face
  edge is about `u=0.171`, which is roughly a 0.012 wrapped offset
- On a 64x64 stock texture, that is about 0.7 texel, enough for nearest-sample
  hardware lookup to choose a different grate texel than the legacy CPU
  texmerge resolves on the rock face edge
- That produces the visible rock strip: the shader texmerge path resolves one
  or more edge texels to bottom rock where the legacy texmerge path resolves a
  metl bar or otherwise does not expose the rock in the same way

### Practical fix direction

- Treat the root cause as the Android-only forced shader texmerge default, not
  the April 14 debug code
- Short-term safe fix: stop forcing `DbgAltTexMerge = 1` on Android, or add an
  Android-only fallback to the old texmerge path for plain transparent stock
  `tmap2` walls such as metl154 until the shader path is made texel-identical
- Longer-term fix: audit the shader texmerge path for exact texel-center and
  wrap behavior versus `merge_textures_new()`, especially on repeated UVs near
  0/1 boundaries on side_type=3 faces

## Tranche Update (2026-04-17, permanent-fix implementation)

- Implemented in Android shared GLES3 shim: external-program draws can now
  reuse the shim streaming VBO path instead of maintaining a separate merge-only
  VBO upload path
- Implemented in D1 and D2: Android `g3_draw_tmap_2()` merged-wall draws now
  feed position, color, base UV, and overlay UV through the shared shim stream
  upload path, with `[metl154upload]` logging `upload_impl=shim_stream`
- Implemented in D1 and D2: overlap snapshot logging now includes the sampled
  rock base UV, nearest rock base palette index, nearest overlay palette index,
  computed legacy CPU texmerge index, and nearest portal palette index so a
  fresh Android capture can directly compare shader sampling against legacy
  texmerge semantics at the exact shared overlap point
- Rationale for this tranche: old_merge changes both texmerge semantics and the
  Android draw/upload path. Unifying the upload path removes one Android-only
  divergence before any broader texmerge fallback is considered
- Validation: `android\run-code-quality.ps1 -Fix` passed
- Validation: `android\gradlew.bat assembleDebug` passed with `JAVA_HOME`
  set to `c:\local\jdk-21`
- Validation: `android\gradlew.bat testDebugUnitTest` passed with 22 tests
  across 5 suites and zero failures
- Desktop validation status: blocked by machine environment, not by the code in
  this tranche. Fresh Windows CMake configure attempts for both D1 and D2 fail
  before build due to missing desktop dependency setup:
  - `vcpkg` is not installed or configured (`VCPKG_ROOT` and
    `VCPKG_INSTALLATION_ROOT` unset, no standard install path present)
  - Visual Studio generator configure with `SDLMIXER=OFF` still fails on
    missing `PhysFS`
- Remaining proof step for root-cause closure: run a fresh Android capture on
  the bad scene and inspect whether the visible leak is gone and whether the new
  `[metl154snapoverlap]` fields show shader-picked texels diverging from or now
  matching `legacy_merge_idx`

## Literal Next Steps

### Execution Rules For This Tranche

- Apply D1 and D2 in lockstep. D2 is a fine lead copy, but the final tranche
  must mirror into D1 before calling the plan phase done. The only relevant
  structural difference found so far is the `const` qualifier on the D2
  `g3_draw_tmap_2()` point-list signature
- Reuse the existing `frame/pass/seq` identity and `g_android_draw_face_ctx`
  join key. Do not invent a second face numbering system for this tranche
- Use `metl154_experiment` for render-path control branches and use
  `metl154_mode` only for visual debug presentation. Do not mix those roles
- Keep any new log line machine-greppable with stable field names:
  `seg`, `side`, `face`, `child`, `wid`, `tmap1`, `tmap2`, `orient`,
  `merge_impl`, `upload_id`, `base_handle`, `overlay_handle`

### A. Do the geometry-path checklist literally

1. Generalize the software clip helper from metl154-only to a debug-gated
   Android path for all `tmap2` walls
   - Likely code: `d1/arch/ogl/ogl.c`, `d2/arch/ogl/ogl.c`
   - Primary seam: `g3_draw_tmap_2()` currently routes only plain metl154 to
     `ogl_clip_and_draw_metl154_merge()`. Use that as the template and add a
     generic `tmap2`-wide debug branch instead of replacing the metl154-only
     helper outright
   - Prefer a new Android-only experiment toggle for this branch instead of
     changing the always-on default path immediately. The existing
     `metl154_experiment` plumbing already reaches JNI, automation,
     introspection, and the Video Info overlay
   - Keep interpolated `u/v` and per-vertex lighting, even if the first pass
     uses the current mono-light approximation only for diagnosis
   - Preserve the current `p3_u`, `p3_v`, `p3_l`, `PF_UVS`, and `PF_LS`
     propagation exactly so the comparison stays about geometry path, not a
     new attribute bug
   - Emit one path-selection line that says whether the draw took the raw
     merged path, the legacy metl154-only clip path, or the new generic clip
     path
   - Goal: stop relying on the metl154-only gate when testing the merged-wall
     path
   - Success signal: the same bad camera view can be captured with generic
     clip on and off while all face-identity lines still refer to the same
     runtime faces
2. Add pre-GPU triangulation diagnostics before `glDrawArrays`
   - Primary seam: `ogl_draw_tmap_2_internal()` right before the final merged
     draw submission
   - Extend the existing split diagnostics rather than creating a parallel log
     family unless the line becomes unreadable. A new dedicated tag is only
     worth it if the current `[metl154split]` format becomes too cramped
   - Log clipped `nv`
   - Log repeated projected vertices
   - Log any vertex already collapsed to `0,0`
   - Log both possible quad diagonals and signed triangle areas
   - Log whether any surviving point still carries `CC_BEHIND`,
     `PF_OVERFLOW`, or other near-plane warnings
   - Include the projected `sx/sy` for every submitted vertex plus its raw
     `p3_codes` and `p3_flags`, because `area=0` by itself does not say which
     vertex actually collapsed
   - Run this on the polygon that is actually about to reach GLES. That means
     post-clip output for clipped draws, not only the original input polygon
   - Gate the full-vertex version if needed. It is acceptable to emit the
     detailed form only for focus faces or only while the comparison
     experiment is enabled
   - Goal: prove exactly what fan GLES receives on bad frames
   - Success signal: a single bad frame explains the fan input without having
     to infer it from earlier clip logs
3. Add a deterministic CPU triangulation experiment
   - First pass should stay narrow: handle the `nv == 4` case only and submit
     duplicated CPU-built arrays as `GL_TRIANGLES` instead of introducing a
     new index-buffer path
   - For quads, choose one diagonal on the CPU and submit `GL_TRIANGLES`
   - For clipped polygons with `nv > 4`, either tessellate on the CPU or at
     minimum log the exact fan triangle order that would be submitted
   - Log `tri_mode=gpu_fan` versus `tri_mode=cpu_triangles` and log the chosen
     diagonal explicitly such as `diag=02` or `diag=13`
   - If a diagonal heuristic is used, tie it to the area diagnostics from step
     2 so the chosen split can be explained after the fact
   - Do not block this tranche on a full generic tessellator. If the quad-only
     experiment moves the bug, that is already a strong answer
   - Goal: remove driver fan choice from the equation
   - Success signal: the artifact changes or stays fixed in a way that cleanly
     separates `GPU fan choice` from `bad source polygon`
4. Add a near-plane stabilization experiment
   - Do not assume `clip_polygon()` by itself closes this branch
   - Primary seam: the existing `clip_polygon()` callers in the metl154 clip
     helpers, plus the proposed generic helper from step 1
   - Add logs that explicitly say whether the polygon was clipped against the
     near plane and what vertices were created
   - Record pre-clip `uor/uand`, the number of input vertices carrying
     `CC_BEHIND`, the number of temporary vertices created by clipping, and
     whether any post-clip vertex still overflows during projection
   - If practical, identify which source edges created temp points. Even a
     compact `new_edges=1-2,2-3` style field is enough to make later analysis
     tractable
   - Re-run the per-vertex diagnostics from step 2 on the clipped output so
     the logs can answer whether the near plane created the degeneracy or the
     polygon was already bad before clipping
   - If needed, split this into `generic clip path` and `extra near-plane
     diagnostics` so the evidence stays clear
   - Success signal: each clipped bad draw can be classified as either
     `near-plane involved` or `no near-plane involvement`
5. Extend the existing metl debug mode instead of inventing a second one-off
   debug renderer
   - Primary shared definitions already live in
     `android/app/src/main/cpp/shared/debug_tex_overlay.h` and are clamped and
     named in `android/app/src/main/cpp/jni_main.c`. The Video Info overlay in
     `android/app/src/main/java/com/dxxredux/app/VideoInfoOverlay.kt` already
     cycles and labels this mode
   - Add a bottom-only mode
   - Add an overlay-only tinted mode
   - Add a split visualization mode if needed
   - Keep `metl154_mode` as pure presentation. Any branch that changes clip,
     triangulation, or old-texmerge behavior belongs under `metl154_experiment`
   - The first two useful additions are `bottom_only` and `overlay_tint`.
     Leave `split_viz` until after step 3 if necessary, because triangle-split
     coloring gets easier once CPU triangulation exists
   - `bottom_only` should keep the same geometry and depth behavior while
     suppressing overlay contribution. `overlay_tint` should ignore the overlay
     RGB and display overlay alpha as a bright fixed tint so underlay fill and
     overlay sampling are visually separable
   - Goal: visually separate underlay fill from overlay sampling
   - Success signal: the user can tell at a glance whether a wrong pixel came
     from underlay fill, overlay sample choice, or triangle split behavior

### B. Do the texture and cache control checklist literally

6. Run the old-texmerge control branch on purpose
   - Primary seam: `render_face()` in `d1/main/render.c` and `d2/main/render.c`
     where `GameArg.DbgAltTexMerge` selects `g3_draw_tmap_2()` versus
     `texmerge_get_cached_bitmap()`
   - On Android, do not depend on startup args alone. Add a narrow Android
     override or experiment-controlled branch so the same install can switch
     between GPU two-pass merge and old cached texmerge during capture
   - Make the path choice explicit in logs with a stable field such as
     `merge_impl=gpu_two_pass` or `merge_impl=old_texmerge`
   - If the bug disappears, the default GPU merge path stays primary
   - If the bug survives, texmerge cache contamination jumps back up
   - Success signal: one controlled run answers whether the artifact requires
     the Android default merged-wall draw path at all
7. Add texmerge slot-owner logging anyway
   - Likely code: `d1/main/texmerge.c`, `d2/main/texmerge.c`
   - Primary seam: `TEXTURE_CACHE` and `texmerge_get_cached_bitmap()`
   - Extend cache entries with debug-only owner metadata so the runtime key
     remains unchanged. Suggested fields: first-owner seg/side/face,
     last-owner seg/side/face, creation frame, last-use frame
   - For each cache entry, log bottom bitmap, top bitmap, orient, first owner,
     last owner, and creation or use frame
   - Pull owner identity from `g_android_draw_face_ctx` when valid. If the
     cache is hit from a non-render caller such as editor or FVI, log a clear
     `ctx=none` or `seg=-1 side=-1 face=-1` rather than pretending the owner
     is known
   - Goal: be able to answer `who first filled this generated merge`
   - Success signal: every suspect texmerge slot can be traced back to the
     first face that created it and the most recent face that reused it
8. Add explicit creation-time versus use-time logs
   - Use one dedicated tag family such as `[metl154texmerge]` so cache lines
     do not get mixed into face, clip, or GL state logs
   - Log when a texmerge entry is created
   - Log when it is reused
   - Log who uses it when the bad portal draw happens
   - Emit `event=create` on cache miss and `event=hit` or `event=reuse` on
     cache hit, with slot index, bottom name, top name, orient, frame, and
     current owner face
   - If the bad portal draw uses the old texmerge path, log whether its slot
     was created in the same frame or long before. That is the shortest path
     to a yes-or-no answer on stale-cache contamination
   - Goal: turn `stale slot?` from a guess into yes-or-no evidence
   - Success signal: the cache branch either produces a fresh correct slot or
     it produces a reused slot with traceable ownership history

### C. Do the upload and buffer control checklist literally

9. Add per-draw upload IDs around Android `merge_vbo` submission
   - Likely code: `d1/arch/ogl/ogl.c`, `d2/arch/ogl/ogl.c`
   - Primary seam: the Android-only `merge_vbo` block in
     `ogl_draw_tmap_2_internal()`
   - Add a monotonic `upload_id` next to the static `merge_vbo` handle. The
     VBO handle is reused, so `upload_id` is the actual per-draw provenance key
   - Log upload ID, seg/side/face, `nv`, byte sizes, offsets, and GL handles
   - Minimum payload for the log line: `upload_id`, `frame/pass/seq`, draw
     order if available, `seg/side/face`, `tmap1`, `tmap2`, `vb/cb/tb/t2b`,
     total byte count, VBO handle, base handle, overlay handle, and program
   - If log volume gets out of hand, gate the full upload line to focus faces
     or to the active comparison experiment instead of making it global
   - Goal: rule out stale streamed-array or attribute-pointer reuse with one
     capture
   - Success signal: one bad draw can be tied to one specific upload with the
     expected buffer layout and texture handles
10. Separate face identity from sampled texture identity everywhere
    - Standardize the field split instead of letting each log call improvise
      its own naming
    - Any bad overlap line should print face identity plus texture identity
    - Minimum fields: `seg/side/face`, `tmap1`, `tmap2`, merge path, bottom
      name, overlay name, base handle, overlay handle
    - Face identity should always come from `g_android_draw_face_ctx` and use
      the same field names across `[metl154diag]`, `[metl154clip]`,
      `[metl154focus]`, upload logs, and texmerge logs
    - Texture identity should come from the actual bitmap and GL state:
      `bot`, `ovl`, `orient`, `merge_impl`, `base_handle`, `overlay_handle`,
      and cache slot when the old path is active
    - Do not treat bitmap name as a face identifier. A correct face can sample
      the wrong texture path, and a wrong face can legitimately sample a rock
      bitmap
    - Goal: stop conflating `where the pixels came from` with `which face
      submitted the draw`
    - Success signal: every suspicious line answers both questions directly

### D. Comparison discipline for the next round

11. Run the above steps as a rote sequence instead of hopping between theories
    - Add the instrumentation batch
    - Capture the same bad views
    - Compare default GPU merge, generic clipped merge, CPU-triangulated merge,
      and old-texmerge control
    - Suggested execution order: step 1, step 2, step 4, then step 3, then the
      old-texmerge control in steps 6 to 8, then the upload/provenance checks
      in steps 9 and 10
    - Use the existing `m154 snap` workflow and current focus faces
      `82/4/0`, `83/4/0`, `83/3/0`, `83/3/1`, and `29/2/0` so every branch is
      compared on the same visible setup instead of on different incidental
      views
    - For each comparison run, answer the same explicit yes-or-no questions:
      `artifact present?`, `source still 29/2/0 or something else?`,
      `degenerate fan input logged?`, `near-plane involved?`, `wrong upload?`,
      `cache reuse involved?`
    - Only after that decide which branch deserves a real fix
    - Success signal: the next implementation tranche ends with one dominant
      branch, not another tie between geometry, cache, and GL-state theories

## Planned Log Analysis Helper

This is future work only. Do not implement it in this tranche.

### Purpose

- The metl logs are now too large for ad hoc scrolling
- We need a repeatable way to slice by face, pair, frame, and log family
  without rereading 10MB from scratch every time

### Minimum useful future artifact

- Start as a notes file or crib sheet, not a parser
- Suggested future file: `android\ai tool plans\metl154_log_analysis_helper.md`
- If that becomes too manual, later add a script such as
  `android\tools\metl154_log_helper.ps1`

### Typical log families to group

- `\[metl154face\]`
- `\[metl154diag\]`
- `\[metl154gl\]`
- `\[metl154state\]`
- `\[metl154split\]`
- `\[metl154clip\]`
- `\[metl154cover\]`
- `\[metl154coverbox\]`
- `\[metl154portal\]`
- `\[metl154focus\]`
- `\[metl154side\]`
- `\[metl154sidegeom\]`
- `\[metl154facegeom\]`
- `\[metl154exp\]`
- `\[metl154wrap\]`
- `\[metl154alpha\]`
- `\[metl154src\]`
- `\[metl154list\]`

### Regex patterns that keep recurring

- All metl lines:
  - `\\[metl154[^\\]]+\\]`
- Frame, pass, and seq identity:
  - `frame=(\\d+)`
  - `pass=(\\d+)`
  - `seq=(\\d+)`
- Face identity:
  - `seg=(\\d+) side=(\\d+) face=(\\d+)`
- Cover-pair identity:
  - `metl_seg=(\\d+) side=(\\d+) face=(\\d+)`
  - `cover_seg=(\\d+) side=(\\d+) face=(\\d+)`
- Texture identity:
  - `tmap1=(0x[0-9a-fA-F]+|\\d+)`
  - `tmap2=(0x[0-9a-fA-F]+|\\d+)`
  - `bot=([^ ]+)`
  - `ovl=([^ ]+)`
- Geometry trouble signals:
  - `stage=clip|stage=culled|stage=overflow`
  - `behind=1`
  - `area=0\\.0`
  - `pick=`
  - `bottom_mix=1\\.000`
  - `alpha=0\\.000`
- Portal and suspect identifiers:
  - `82/4/0`
  - `83/4/0`
  - `83/3/1`
  - `29/2/0`

### Queries this helper should support later

- All lines for one face across a file
- All overlaps for one metl face and one cover face
- All lines from one frame
- All lines from one `seq`
- All places where the same face changes merge path or alpha behavior
- All bad samples where `alpha=0.000` and `bottom_mix=1.000`

## Planned Provenance Tagging

This is future work only. Do not implement it in this tranche.

### Goal

Be able to ask a bad pixel, texture, or draw `where did you originate?` and
get a useful answer instead of only a texture name.

### The provenance problem has three layers

1. Face provenance
   - Which runtime face submitted the draw
   - We already have part of this in `g_android_draw_face_ctx`
2. Texture provenance
   - Which bitmap, upload path, or generated texmerge entry produced the
     sampled texels
3. Draw provenance
   - Which exact GL submission wrote the pixels that ended up on screen

### Realizable code directions

1. Extend face provenance first
   - Likely code: `d1/main/render.c`, `d2/main/render.c`
   - The current shared struct already carries `seg`, `side`, `face`,
     `child`, `side_type`, `nv`, `wid_flags`, `tmap1`, and `tmap2`. Extend
     that existing struct instead of creating a parallel provenance object
   - Carry `seg/side/face`, `tmap1`, `tmap2`, `orient`, `wid`, `child`, and a
     monotonic draw provenance ID into OGL
   - Fill the added fields in `render_set_android_draw_face_context()` and
     clear them in `render_clear_android_draw_face_context()` so logs never see
     stale values between draws
   - Prefer a dedicated `draw_id` if later work needs a provenance key that is
     independent of metl154-only filtering. `g_metl154_draw_seq` is already a
     useful temporary join key, but it only increments on tracked metl154 face
     logs today
   - Use this as the stable join key across logs
2. Add texmerge cache provenance
   - Likely code: `d1/main/texmerge.c`, `d2/main/texmerge.c`
   - Reuse the slot-owner metadata from steps 7 and 8. The important rule is
     to keep provenance fields out of the cache key so the render behavior does
     not change while instrumentation is added
   - Extend cache entries with debug-only first-owner and last-owner metadata
   - Record creation frame, use frame, and owner face
   - If this grows beyond a few integers, move the debug fields behind an
     Android or debug-only guard rather than making every platform carry the
     cost
3. Add GL texture provenance
   - Likely code: `d1/arch/ogl/ogl.c`, `d2/arch/ogl/ogl.c`
   - Primary seams are `ogl_loadbmtexture_f()` and `ogl_bindbmtex()` where the
     GL handle is created, rebound, or lazily loaded
   - Track when a `gltexture->handle` is created or rebound, plus which
     bitmap or upload path produced it
   - Minimum useful fields: handle, bitmap name, source path
     (`ktx2`, `rgba`, `stock`, `texmerge_cache`), width and height,
     creation frame, and last bind frame
   - For metl154 work, base and overlay handles matter more than a generic
     global system
4. Add draw provenance ring buffer
   - Likely code: Android OGL path only
   - Start from the existing tracked-face and snapshot storage patterns in
     `ogl.c` instead of inventing a new subsystem from scratch. The current
     `metl154_tracked_faces`, `metl154_focus_draws`, and
     `metl154_snapshot_cover_events` already prove the codebase can maintain
     small fixed-size per-frame draw records safely
   - Store the last N draw submissions with face provenance, texture
     provenance, projected box, and draw order
   - Add `upload_id` and the chosen triangulation mode to each record so the
     ring answers both `what got drawn` and `how it got submitted`
   - Keep it Android-only and fixed-size. A simple overwrite-on-wrap ring of
     `128` or `256` entries is enough for targeted captures
   - This is the closest realizable version of `ask the misplaced rock where
     it came from`
5. Expose provenance for interrogation
   - First version can be log-only
   - Better version later can be dumped via introspection or a debug overlay
   - The lowest-risk first surface is the existing snapshot path. A snapshot
     request can dump the most recent provenance ring entries without adding a
     continuous per-frame JSON cost
   - If introspection grows this later, expose recent draw records on demand
     rather than appending them to every normal state dump
   - If point-picking is ever added, the query should return all candidate
     draws whose projected polygon contains the tapped or sampled point

### Working rule for future provenance work

- Do not collapse face provenance and texture provenance into one field
- A bad draw can have the correct face with the wrong sampled texture, or the
  correct texture sampled through the wrong face, or the right face and
  textures submitted through the wrong GL draw path
- The system has to preserve all three layers if it is going to answer
  `where did this come from?`

## Immediate Rule For The Next Tranche

- No new broad theory writeups in this file
- Add evidence or move on
- If a step fails, record it under `What Has Not Worked` and keep this active
  file short