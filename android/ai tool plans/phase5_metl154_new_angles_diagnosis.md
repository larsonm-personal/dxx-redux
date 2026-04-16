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

## Literal Next Steps

### A. Do the geometry-path checklist literally

1. Generalize the software clip helper from metl154-only to a debug-gated
   Android path for all `tmap2` walls
   - Likely code: `d1/arch/ogl/ogl.c`, `d2/arch/ogl/ogl.c`
   - Keep interpolated `u/v` and per-vertex lighting, even if the first pass
     uses the current mono-light approximation only for diagnosis
   - Goal: stop relying on the metl154-only gate when testing the merged-wall
     path
2. Add pre-GPU triangulation diagnostics before `glDrawArrays`
   - Log clipped `nv`
   - Log repeated projected vertices
   - Log any vertex already collapsed to `0,0`
   - Log both possible quad diagonals and signed triangle areas
   - Log whether any surviving point still carries `CC_BEHIND`,
     `PF_OVERFLOW`, or other near-plane warnings
   - Goal: prove exactly what fan GLES receives on bad frames
3. Add a deterministic CPU triangulation experiment
   - For quads, choose one diagonal on the CPU and submit `GL_TRIANGLES`
   - For clipped polygons with `nv > 4`, either tessellate on the CPU or at
     minimum log the exact fan triangle order that would be submitted
   - Goal: remove driver fan choice from the equation
4. Add a near-plane stabilization experiment
   - Do not assume `clip_polygon()` by itself closes this branch
   - Add logs that explicitly say whether the polygon was clipped against the
     near plane and what vertices were created
   - If needed, split this into `generic clip path` and `extra near-plane
     diagnostics` so the evidence stays clear
5. Extend the existing metl debug mode instead of inventing a second one-off
   debug renderer
   - Add a bottom-only mode
   - Add an overlay-only tinted mode
   - Add a split visualization mode if needed
   - Goal: visually separate underlay fill from overlay sampling

### B. Do the texture and cache control checklist literally

6. Run the old-texmerge control branch on purpose
   - Force `-gl_oldtexmerge` or `DbgAltTexMerge = 0` on Android for a
     controlled run
   - If the bug disappears, the default GPU merge path stays primary
   - If the bug survives, texmerge cache contamination jumps back up
7. Add texmerge slot-owner logging anyway
   - Likely code: `d1/main/texmerge.c`, `d2/main/texmerge.c`
   - For each cache entry, log bottom bitmap, top bitmap, orient, first owner,
     last owner, and creation or use frame
   - Goal: be able to answer `who first filled this generated merge`
8. Add explicit creation-time versus use-time logs
   - Log when a texmerge entry is created
   - Log when it is reused
   - Log who uses it when the bad portal draw happens
   - Goal: turn `stale slot?` from a guess into yes-or-no evidence

### C. Do the upload and buffer control checklist literally

9. Add per-draw upload IDs around Android `merge_vbo` submission
   - Likely code: `d1/arch/ogl/ogl.c`, `d2/arch/ogl/ogl.c`
   - Log upload ID, seg/side/face, `nv`, byte sizes, offsets, and GL handles
   - Goal: rule out stale streamed-array or attribute-pointer reuse with one
     capture
10. Separate face identity from sampled texture identity everywhere
    - Any bad overlap line should print face identity plus texture identity
    - Minimum fields: `seg/side/face`, `tmap1`, `tmap2`, merge path, bottom
      name, overlay name, base handle, overlay handle
    - Goal: stop conflating `where the pixels came from` with `which face
      submitted the draw`

### D. Comparison discipline for the next round

11. Run the above steps as a rote sequence instead of hopping between theories
    - Add the instrumentation batch
    - Capture the same bad views
    - Compare default GPU merge, generic clipped merge, CPU-triangulated merge,
      and old-texmerge control
    - Only after that decide which branch deserves a real fix

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
   - Carry `seg/side/face`, `tmap1`, `tmap2`, `orient`, `wid`, `child`, and a
     monotonic draw provenance ID into OGL
   - Use this as the stable join key across logs
2. Add texmerge cache provenance
   - Likely code: `d1/main/texmerge.c`, `d2/main/texmerge.c`
   - Extend cache entries with debug-only first-owner and last-owner metadata
   - Record creation frame, use frame, and owner face
3. Add GL texture provenance
   - Likely code: `d1/arch/ogl/ogl.c`, `d2/arch/ogl/ogl.c`
   - Track when a `gltexture->handle` is created or rebound, plus which
     bitmap or upload path produced it
   - For metl154 work, base and overlay handles matter more than a generic
     global system
4. Add draw provenance ring buffer
   - Likely code: Android OGL path only
   - Store the last N draw submissions with face provenance, texture
     provenance, projected box, and draw order
   - This is the closest realizable version of `ask the misplaced rock where
     it came from`
5. Expose provenance for interrogation
   - First version can be log-only
   - Better version later can be dumped via introspection or a debug overlay
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