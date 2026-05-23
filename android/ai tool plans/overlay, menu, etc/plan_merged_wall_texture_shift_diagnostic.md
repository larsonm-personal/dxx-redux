# Plan: Merged-Wall Texture Right-Shift Diagnostic (D1 primary, D2 secondary)

## Problem statement (from user observation)

- In Descent 1 (both D1 engine and D2 engine running D1 content), virtually every
  level-1 wall that has an overlay (merged wall) renders with its texture
  horizontally shifted right by roughly 20% of the face width. The left part of
  the face shows what wraps from the right edge of the texture, so the face is
  still fully textured (no solid color gap); it is just mis-aligned
- The effect is visible on the pure solid-wall merges such as "light panel on
  top of rock" and "sign on top of rock" where the overlay sprite should sit in
  a specific spot on the rock but is clearly off-center horizontally
- The effect does NOT obviously reproduce in D2 on level 1. D2 looks correct
  most of the time, and when toggling `m154 exp` it goes from correct to a
  mild mis-alignment. D1 starts wrong and cycles between wrong positions as
  `m154 exp` changes mode
- The shift is independent of: `TexFilt` (0/1/2), AF level, MSAA, and hires
  texture pack choice (stock piggy, 128px, 512px ports)
- Non-merged faces (single tmap1, no overlay) and animated door strips like
  the `door45#*` family do NOT exhibit this shift. The shift is specific to
  the merged-wall code path

This mirrors the separation that already exists in the repo notes:
`metl154` for merge-path bugs and `door45` for animated-door base-texture
corruption. Both plan families already exist; this is a distinct third bug
class (merged-wall UV alignment) that has not yet been isolated.

## Goals

1. Build up enough investigation evidence to either identify the root cause
   in source or produce a short list of strong suspects. NO code fixes in
   this plan file; the output is a design plus captured on-device evidence
2. Add a reusable "crosshair tap" on-device diagnostic that logs, for the
   face under the crosshair, enough per-vertex UV, face-size, and bitmap
   metadata to document the shift quantitatively across many faces without
   needing screenshots or manual measurement. This is an extension of the
   existing Video Info overlay `mwall snap: Tap` workflow
3. Capture a first batch of tap logs from the emulator and phone and decide
   whether the evidence already points at a single source file or still
   requires additional instrumentation

## Prior-art summary (why the existing plans are not enough)

The existing related plan files already established:

- `metl154_*.md`: narrow, per-bitmap instrumentation of the `metl154`
  transparent-wall portal. Focused on clip, cover, split, and shader state
  on one specific merged bitmap. It uses `[metl154face]`, `[metl154clip]`,
  `[metl154cover]`, and `[metl154coverbox]` logs keyed to one bitmap
- `plan_door45_base_texture_pose_repro.md`: narrow, per-bitmap
  instrumentation of the `door45` animated-door base texture and its hidden
  cover behind a transparent-wall portal. Uses `mwall_snapshot_*`,
  `mwall_cover_live`, `mwall_cover_src`, `mwall_cover_src_row`,
  `mwall_mip_upload`, `mwall_cover_lod` log families
- `tap_now_pose_logging_and_pose_warp.md`: added `mwall snap: Tap` on the
  Video Info overlay. It posts a snapshot request from the Java UI thread
  through `android_merged_wall_request_snapshot()` which the next rendered
  frame consumes on the GL thread and writes a single
  `[mwall_snapshot] stage=frame ...` block plus per-face partial and
  coverbox lines
- `metl154-face-id-and-transparent-wall-semantics.md` (repo memory):
  `g_android_draw_face_ctx` already carries a deterministic per-face
  `seg/side/face/child/wid/tmap1/tmap2` tag into both render and texmerge
  code, so any new logger can key cleanly off that context

None of these logs record what the user actually needs here: for the face
currently centered under the crosshair, what are the per-vertex UVs, the
screen-space projected positions, the base-bitmap and overlay-bitmap
dimensions (both piggy and hires), the orient bits, the active merge path
(force_two_pass vs merge_cached vs legacy texmerge), and how the UVs map
back onto the face edges. Those fields are what lets a human compare "where
the light panel should sit" vs. "where it actually sits" across many faces.

## Phase 1: Targeted code study, no instrumentation yet

Deliverable: short list of specific hypotheses, each tied to a source
location, that the new on-tap diagnostic should be able to distinguish
between.

### 1A. Trace the UV flow end-to-end for a merged wall on D1 Android

Walk the exact call chain on Android with the default flags
(`GameArg.DbgAltTexMerge = 1`, `OGL_MERGE` defined):

- `render_side()` / `render_face()` in `d1/main/render.c` builds `uvl_copy[]`
  from the side's stored `uvls[]`. Note whether those stored UVs are ever
  offset or scaled by anything Android-specific versus the stock game
- `g3_check_and_draw_tmap()` at `d1/main/render.c:395` calls
  `g3_draw_tmap_2(nv, pointlist, uvl_copy, dyn_light, bm, bm2, orient)` where
  `orient = ((tmap2 & 0xC000) >> 14) & 3`
- `g3_draw_tmap_2()` in `d1/arch/ogl/ogl.c:3432` routes to
  `ogl_draw_tmap_2_internal()` which at `d1/arch/ogl/ogl.c:2946` decides
  between three branches when `super == 0`:
  - `force_two_pass` (when `g_merged_wall_force_two_pass` is set by the
    `m154 exp` experiment): emits `[metl154clip] ... route=force_two_pass`,
    falls through to the GPU two-pass path that draws `bmbot` then `bmovl`
    with their own UVs
  - `merge_cached`: calls `ogl_android_get_cached_plain_texmerge_bitmap()`
    to pre-merge the two bitmaps into a single FBO texture, then draws via
    `g3_draw_tmap(nv, pointlist, uvl_list, light_rgb, merged)` with the
    OVERLAY-RELATIVE `uvl_list` (the bot UVs)
  - legacy CPU `texmerge_get_cached_bitmap()`: reached only when
    `DbgAltTexMerge == 0` or Android's `oldmerge` experiment is active,
    returns a CPU-merged 64x64 palette bitmap that then goes through the
    normal single-texture path

Action items:

- [x] Read `d1/main/render.c` for the `uvls[]` population path. Confirm that
      `side->uvls[0..3].u/v` are set at level load from
      `Segment2s[]`/`Segments[]` side data and never mutated per-frame
- [x] Read `d1/arch/ogl/ogl.c:2843..3460` end-to-end and write a one-page
      summary (in this plan file, under "Findings") of: which shader program
      runs per branch, which texture unit carries the bottom vs overlay,
      which UV array is fed to each texture unit, and what the `orient`
      rotation is applied to in each branch
- [x] Read `d1/arch/ogl/ogl.c:1958..1994`
      (`ogl_android_texmerge_build_uvs`) and the FBO quad draw at
      `d1/arch/ogl/ogl.c:2094..2205`. Record on paper the exact mapping from
      FBO quad corners `(-1, +/-1, +/-1, -1)` to `bot_uv` and `ovl_uv`
      samples. Pay attention to the Y-axis flip that FBO-as-texture usually
      introduces. The cached merged bitmap is subsequently resampled with
      the face's `uvl_list`, which assumes "no flip, no offset" relative to
      the piggy bitmap
- [x] Read `d1/main/texmerge.c:302..400` for `merge_textures_new` and
      `merge_textures_super_xparent`. Record the exact orient rotation table
      (case 0/1/2/3) used on the CPU path, and compare it to the GPU table
      in `ogl_android_texmerge_build_uvs`. The two tables should be
      mathematically identical. If not, that alone might explain a
      per-orient shift between cached (GPU) and legacy (CPU) merge paths
- [x] Diff the equivalent D2 paths in `d2/main/render.c`, `d2/arch/ogl/ogl.c`,
      and `d2/main/texmerge.c`. Note any D1-only or D2-only conditional
      that alters UV generation, since the user reports a D1-only problem

### 1B. Hypotheses this study must rank

Each hypothesis below names a mechanism that could plausibly produce a
horizontal right-shift on merged walls but leave single-texture faces
unaffected. Rank them Strong / Medium / Weak after phase 1A:

- H1 (Medium): `ogl_android_get_cached_plain_texmerge_bitmap` uses
  `gltexture->u/v` (bot and ovl independently) to build the FBO quad UVs.
  If the overlay bitmap has a non-1.0 `u` from hires NPOT padding and the
  bot has a different `u`, the resulting FBO composite can have the overlay
  shifted horizontally relative to where the original CPU merge would put
  it. At runtime the face's `uvl_list` is reused with `u_max=1.0`, which
  assumes the merged output occupies `[0..1]` in both dimensions uniformly.
  A stock D1 piggy wall is always 64x64 with `u=v=1.0`, so this hypothesis
  should be testable by logging both bitmaps' `u/v` pairs on the tap
- H2 (Medium): the FBO draw in
  `ogl_android_get_cached_plain_texmerge_bitmap` sets the FBO size to
  `max(bot_visible, ovl_visible)` in each axis, but reads back as a texture
  whose `u/v` are forced to `1.0` and whose `w/h` are set to
  `width, height`. If the vertex array `(-1..+1)` quad is used unchanged
  when bot and ovl have different visible dims, one of them can sample past
  its visible region into padded pixels (or below its visible region),
  producing a half-texel-plus shift on only merged faces
- H3 (Medium): orient table mismatch between
  `ogl_android_texmerge_build_uvs` (GPU FBO) and
  `merge_textures_new` / `merge_textures_super_xparent` (CPU legacy). A
  wrong rotation for a given orient would look like a shift for overlays
  whose content is not rotationally symmetric. D1 uses orient more often
  than D2 on level 1 (light panels, signs), which would explain the D1-only
  severity
- H4 (Weak): the per-frame `uvl_copy[]` that `g3_check_and_draw_tmap()`
  passes is different when tmap2 != 0 vs tmap2 == 0. Read the code between
  `render_face()` and the draw call to see if anything adjusts UVs only on
  the merged path. This is weak because the stock game has been stable on
  desktop for years on the same code
- H5 (Medium-Weak): the "20% right-shift" observation may not be a true
  pixel-space shift but rather the hires 128x128 overlay being drawn at
  64x64 worth of UV span because the cached FBO is created at the bot
  visible dim (64) but the overlay hires content is 128. That would scale
  the overlay down by 2x and leave it at the upper-left 50% of the merged
  texture, which from some viewing angles can read as "shifted right by
  ~20%" because the visible face is wider than it is tall. Rank this after
  the phase-2 logs show actual UV and dim numbers
- H6 (Weak): the `m154 exp` cycling changes both which merge branch runs
  AND where `glPolygonOffset`, the CPU clip helper, and state-reset code
  intervene. The user's observation that cycling shifts D2 from correct to
  slightly incorrect is consistent with at least one of those branches
  feeding a different UV. Rank this after phase 1A

### 1C. Phase 1 exit criteria

- [x] This plan file has a new "Findings" section with a concise summary of
      the three merge branches, the exact orient tables for each, the per-
      branch UV source, and a ranked hypothesis list (Strong / Medium /
      Weak) tied to specific source lines

## Findings (Phase 1 output)

### Stored UV data is not mutated per-frame

- `render_face()` at `d1/main/render.c:224` copies `uvlp[i].u/v` into a
  local `uvl_copy[]` with no scaling or offset
  (`d1/main/render.c:238..240`), then only adjusts the lighting `l` field.
  The UVs passed downstream are exactly `sidep->uvls[]` as authored in the
  level
- `render_side()` at `d1/main/render.c:543` dispatches to `render_face()`.
  For 4-vertex (quad) merged walls, which is the common case for level 1
  light-panel-on-rock faces, it calls `render_face(..., sidep->uvls, ...)`
  directly with the full 4 uvls. For face-split cases it builds a
  `temp_uvls[0,2,3]` or `[0,1,3]` reordering (`d1/main/render.c:642` and
  `:670`), or passes `&sidep->uvls[1]` for the second triangle
  (`d1/main/render.c:663`). Either way, it is a re-ordering of the stored
  authored UVs, not a new computation
- D2 has the same call structure in `d2/main/render.c`. No D1-only offset
  or scale is applied before the draw call
- Conclusion: the authored UVs and their orient bits (`(tmap2 & 0xC000) >>
  14`) are identical between the merge-cached path and the two-pass path.
  Any observed mis-alignment must come from how those UVs are consumed
  downstream, not from the data itself

### Three merge branches on Android, summarised

Branch 1: `force_two_pass` (`d1/arch/ogl/ogl.c:2928..2953`, enabled by
`g_merged_wall_force_two_pass`, which is what `m154 exp` cycles):
- TU0 bound to `bmbot`, TU1 bound to `bmovl`, TU2 bound to
  `bmovl->gltexture_mask` when `super`
- `texcoordbot_array` = face `uvl_list` straight:
  `(uvl_list[c].u, uvl_list[c].v)` at `d1/arch/ogl/ogl.c:2992..2993`
- `texcoordovl_array` = face `uvl_list` with orient rotation applied:
  - orient 0: `(u, v)`
  - orient 1: `(1 - v, u)`
  - orient 2: `(1 - u, 1 - v)`
  - orient 3: `(v, 1 - u)`
- Both UV arrays are bound to their respective texture units and the face
  is drawn once with the tex2 shader

Branch 2: `merge_cached` (`d1/arch/ogl/ogl.c:2955..2972`, default when
`!force_two_pass && (bmovl->bm_flags & BM_FLAG_TRANSPARENT)`):
- `ogl_android_get_cached_plain_texmerge_bitmap()` at
  `d1/arch/ogl/ogl.c:1994..2210` pre-composites `bmbot` and `bmovl` into an
  FBO texture using a full-screen quad with fixed NDC corners
  `{(-1,+1), (+1,+1), (+1,-1), (-1,-1)}` and `base_u/base_v =
  {(0,0),(1,0),(1,1),(0,1)}`
- Inside `ogl_android_texmerge_build_uvs()` at
  `d1/arch/ogl/ogl.c:1958..1992` the bot UVs are `base_u * bot_u_max,
  base_v * bot_v_max`; the ovl UVs apply the same orient table as Branch 1
  but to the fixed `base_u/base_v` coords, not to the face UVs
- Cached texture is written with `entry->texture->u = 1.0f; v = 1.0f`
  regardless of the source bitmaps' `u/v`
  (`d1/arch/ogl/ogl.c:2088..2089`)
- Face is then drawn by `g3_draw_tmap(nv, pointlist, uvl_list, ...,
  merged)` at `d1/arch/ogl/ogl.c:2972`. That helper at
  `d1/arch/ogl/ogl.c:2674` uses the face UV directly as the texcoord, no
  further scaling: `texcoord_array[i] = f2glf(uvl_list[c].u/v)`
  (`d1/arch/ogl/ogl.c:2780..2781`)

Branch 3: legacy CPU `texmerge_get_cached_bitmap` (reached earlier in
`render_face()` when `DbgAltTexMerge == 0` and not the Android-default
path, or when the "old texmerge" experiment is active):
- `merge_textures_new()` at `d1/main/texmerge.c:302` walks
  `dest_data[wh*y + x]` and fills it from `top_data[...]` with an orient
  table:
  - orient 0: `top[wh*y + x]`
  - orient 1: `top[wh*x + ((wh-1)-y)]`
  - orient 2: `top[wh*((wh-1)-y) + ((wh-1)-x)]`
  - orient 3: `top[wh*((wh-1)-x) + y]`
- The merged bitmap is then uploaded as a normal piggy texture and drawn
  via `g3_draw_tmap(nv, pointlist, uvl_list, ..., merged)`. The face UVs
  are used unchanged

### Orient tables are consistent between GPU and CPU

Parameterising each table with `(u, v)` in [0,1]^2 and writing the sampled
top coordinate `(tu, tv)` in the same normalised frame:

| orient | CPU `merge_textures_new` | GPU `ogl_android_texmerge_build_uvs` |
| ------ | ------------------------ | ------------------------------------ |
| 0      | `(u, v)`                 | `(u, v)`                             |
| 1      | `(1 - v, u)`             | `(1 - v, u)`                         |
| 2      | `(1 - u, 1 - v)`         | `(1 - u, 1 - v)`                     |
| 3      | `(v, 1 - u)`             | `(v, 1 - u)`                         |

The two orient tables are mathematically identical, so **H3 (orient table
mismatch) is rejected** as a primary cause.

### Branch 2 (GPU FBO composite) has a memory-layout Y-flip vs. Branches 1 and 3

Tracing the FBO draw at `d1/arch/ogl/ogl.c:1994..2100`:

- Vertex 0 is placed at NDC `(-1, +1)` (screen top-left) and carries
  `base=(0, 0)`, so `bot_uv = (0, 0)` and the fragment at FB pixel
  `(0, height-1)` (top-left of FBO memory) gets bot sampled at `(0, 0)`
- Vertex 3 is placed at NDC `(-1, -1)` (screen bottom-left) and carries
  `base=(0, 1)`, so `bot_uv = (0, bot_v_max)` and FB pixel `(0, 0)`
  (bottom-left of FBO memory) gets bot sampled at `(0, bot_v_max)`
- For a fragment at FB pixel `(px, py)` the interpolated bot UV is
  `(px / W, 1 - py / H)` (because `base_v` varies linearly from 0 at the
  `+1` NDC row to 1 at the `-1` NDC row)
- When the FBO texture is later sampled as an input with face UV
  `(fu, fv)`, GL reads FBO pixel `(fu * W, fv * H)`, which was filled with
  bot sampled at `(fu, 1 - fv)`

So when this cached texture is drawn with the face's stored UVs, the
resulting sampling is:

- bot sampled at `(face_u, 1 - face_v)`

compared to Branch 1 two-pass, which samples bot at `(face_u, face_v)`.
**That is a V-flip of the bottom texture between Branch 1 and Branch 2.**

Applying the same derivation to the overlay per orient:

| orient | Branch 1 two-pass ovl sample at face (u,v) | Branch 2 merge_cached ovl sample at face (u,v) |
| ------ | ------------------------------------------ | ---------------------------------------------- |
| 0      | `(u, v)`                                   | `(u, 1 - v)`                                   |
| 1      | `(1 - v, u)`                               | `(v, u)`                                       |
| 2      | `(1 - u, 1 - v)`                           | `(1 - u, v)`                                   |
| 3      | `(v, 1 - u)`                               | `(1 - v, 1 - u)`                               |

For every orient the two paths disagree: orient 0 and 2 are V-flipped,
orient 1 and 3 are U-flipped. The bot is V-flipped in all four cases.

### Ranked hypotheses (after Phase 1 study)

- **H7 (Strong, new)**: Branch 2 `merge_cached` builds the FBO with a quad
  whose vertex layout and UV layout disagree in Y between the "image
  origin" convention and the GL FB origin convention, causing a V-flip on
  the bot and a V-flip-or-U-flip (depending on orient) on the overlay
  relative to Branches 1 and 3. Because stored face UVs are reused as-is
  to read the merged FBO, the flip appears as a displacement along
  whichever face axis happens to be perpendicular to the face's up vector
  in world space. For a vertically-placed light panel on a wall, that can
  manifest as an apparent horizontal shift because the face's "v" runs
  along the wall's horizontal when the face is oriented sideways. The
  `m154 exp` switch flips between Branch 1 (correct) and Branch 2 (flipped)
  and therefore necessarily changes the apparent alignment, which matches
  the user's observation. See
  `d1/arch/ogl/ogl.c:1958..2100` and `d2/arch/ogl/ogl.c:1960..2100`;
  the D1 and D2 implementations are byte-identical
- **H1 (Medium, unchanged)**: hires NPOT padding leaks. Only triggers when
  `tex->u < 1.0`, and `ogl_loadtexture` currently sets `tex->u = w / tw`
  with `tw = pow2ize(w)`, so stock 64x64 and 128/256/512 hires all stay at
  `u = 1.0`. Keep the phase 2 probe logging of both bitmaps' `u/v` to
  confirm this
- **H2 (Medium, refined)**: `ogl_android_texmerge_visible_dim` returns
  `floor(size * scale + 0.5)` when `scale > 0 && scale < 1`. For a
  hypothetical NPOT-padded hires overlay this would under-size the FBO
  along the overlay axis and leave the merged texture half-empty, which
  would look like a shift. Same gate as H1: only matters if `tex->u` is
  ever set below 1.0. Leave the probe fields in for later confirmation
- H3 (rejected): orient tables are mathematically identical between CPU
  and GPU (see the table above)
- H4 (rejected): `uvl_copy[]` is a straight copy of the authored
  `sidep->uvls[]` with no offset or scale applied per-frame
  (`d1/main/render.c:238..240`). The same is true in D2
- H5 (unlikely but still covered by the probe): hires overlay being drawn
  at bot's visible dim is structurally the same as H2. Rank weak unless
  phase 2 logs show bot/ovl `w/h` differing
- H6 (weak, partially explained): the `m154 exp` toggle cycles
  `g_merged_wall_force_two_pass`. Because Branch 1 is correct and Branch 2
  is flipped (H7), toggling the switch necessarily moves the alignment.
  No separate mechanism needed. The D2-vs-D1 severity difference is still
  not fully explained; the most likely reason is which level-1 merged
  faces happen to use orient 1 or 3 (U-flip on overlay) vs 0 or 2 (V-flip
  on overlay). Phase 3 probe data will confirm

### Phase 2 probe field priorities, updated from findings

Given H7 is now Strong, the probe must record (in addition to the fields
already listed in phase 2A):

- For each branch the face actually took this frame, a one-character tag
  indicating which of the four derived sample formulas above was used
  (`ovl_sample_path=B1_orient0` etc.), so the log itself encodes whether
  the flip is expected
- The face's world-space "up" axis projected onto its UV basis, so the
  flip axis can be correlated to a visible-direction axis (is the flip
  reading as horizontal or vertical on screen?). This can be computed
  from `sidep->normal` + `ConsoleObject->orient` without needing any
  extra engine hooks
- A diff-friendly summary field `ovl_flip_axis=U|V|none` computed by
  comparing the Branch 1 and Branch 2 sample formulas for this orient, so
  phase 3 can grep a single token per face

## Phase 2: On-tap crosshair texture-alignment diagnostic

Deliverable: a new Video Info overlay button (or extension of the existing
`mwall snap: Tap` handler) that logs, for the face currently under the
crosshair center, enough data to compute the texture-vs-face alignment from
the log alone. This is a debug-only path under `INTROSPECT_ON` /
`ANDROID` build guards, and must follow the existing conventions in
`android/app/src/main/cpp/shared/merged_wall_debug.c` so the logs show up
in the exportable `debuglogs/debuglog_*.txt` under `DLOG_TEXTURE`

### 2A. Extend the tap handler to emit crosshair-face data

- Reuse `android_merged_wall_request_snapshot()` as the entry point from
  Java. Add a second request type (e.g. an enum or a bit flag on the
  pending request) for "crosshair probe" so the existing `mwall snap: Tap`
  button and the new one share the snapshot flush, frame counter, and
  pose-log formatting. The goal is zero new JNI surface beyond one extra
  int argument
- On the next rendered frame after a crosshair-probe request, iterate the
  per-frame tracked-face list the merged-wall snapshot system already
  maintains (`g_mwall_tracked_faces[]` in
  `android/app/src/main/cpp/shared/merged_wall_debug.c`). For each tracked
  face, do a screen-space point-in-polygon test against the canvas-center
  pixel using the same projected-bbox helpers that
  `mwall_snapshot_focus_cover` already uses. Pick the nearest front-most
  face as the "crosshair face"
- Emit a single multi-line `[mwall_tap_probe]` block containing:
  - request type: `crosshair`
  - frame / request_frame / level num and name
  - pose: segment, x/y/z (fix-to-float), pitch/bank/heading (copy the
    format from `[mwall_snap_pose]`)
  - canvas and crosshair pixel (`canvas_center`)
  - face identity: seg, side, face, child, wid, tmap1, tmap2, orient
  - merge route taken this frame for this face: `force_two_pass`,
    `merge_cached`, or `legacy_texmerge`
  - bottom bitmap: name, `bm_w`, `bm_h`, `bm_flags`, `piggy_bitmap_flags`,
    `gltexture->w`, `gltexture->h`, `gltexture->u`, `gltexture->v`,
    `gltexture->is_png`, `has_mipmaps`, mask handle
  - overlay bitmap: same fields as bottom
  - cached merged bitmap (if `merge_cached`): its `w`, `h`, `u`, `v`, its
    source-bot and source-ovl pointers, and the FBO cache slot
  - per-vertex data for each of `nv` vertices (up to 4):
    - raw stored UV from `uvl_list[i].u/v` in fix and float
    - projected screen X/Y from `pointlist[i]->p3_sx/p3_sy`
    - which corner of the face the vertex is (heuristic: min/max X, min/max
      Y, compute the face's projected bbox and tag vertices as `LT/RT/RB/LB`
      based on their position in that bbox)
  - derived alignment fields:
    - horizontal UV span across the face: `u_max - u_min` (in units of the
      bottom texture), which for a correctly-authored wall should almost
      always be `1.0` or an integer
    - same for vertical
    - `u_shift_hint`: `u_min mod 1.0`, which tells whether the face starts
      on a texel boundary or is offset (e.g. 0.2 would be the suspected
      ~20% shift)
    - `v_shift_hint`: `v_min mod 1.0`
    - for merged faces: the expected overlay anchor in merged-bitmap
      pixel space, computed two ways:
      - (a) from the stored `uvl` + `orient` rotation applied to the
        overlay, which is what the cached FBO path produces
      - (b) from the CPU legacy merge table in `merge_textures_new`, which
        is what the stock game produced
      - log both and a pass/fail flag `orient_agree=1/0`
- The log format must be greppable and line-oriented. Prefer multiple
  lines with the shared `frame=... pass=... seq=...` prefix over one giant
  line, matching the existing convention in `merged_wall_debug.c`

### 2B. Add a "mwall tap: Probe" button to the Video Info overlay

- Mirror the existing `mwall snap: Tap` button in
  `android/app/src/main/java/com/dxxredux/app/VideoInfoOverlay.kt`
- The button posts the new request type via the existing JNI entry. It
  flashes `Sent` on press like the snapshot button. It must not replace the
  existing `mwall snap: Tap` button; it is an additional diagnostic
- The button should be next to the existing `mwall snap: Tap` so the user
  can walk through a room and probe every visible face quickly
- Follow the launcher/JNI naming convention already used by the tap-now
  pose logger and the `set_debug` automation entry, so automation scripts
  can drive the probe the same way they drive the snapshot

### 2C. Extend automation with a `probe_crosshair` action

- In `android/app/src/main/cpp/game_automate.cpp`, add a new action
  `probe_crosshair` that takes an optional step-label and calls the same
  JNI entry the Java button uses. This way a JSON5 script under
  `android/game_scripts/` can:
  1. `pose_view` to a known pose (reusing the existing action)
  2. wait one frame
  3. `probe_crosshair` to log the face under the crosshair at that pose
  4. move to the next pose and repeat
- This gives a reproducible corpus of probe logs across many faces on level
  1 without manual taps. It also means the first user-visible artifact of
  this plan is a committed regression script under
  `android/game_scripts/` even before any fix lands
- The script produces a repeatable diff of
  `[mwall_tap_probe]` blocks between D1 and D2 on the same pose, which is
  the single most useful direct comparison for isolating whether the shift
  is D1-specific or engine-specific

### 2D. Phase 2 exit criteria

- [ ] `mwall tap: Probe` button exists on the Video Info overlay in D1 and
      D2 debug builds and emits `[mwall_tap_probe]` blocks into the
      exportable texture debug log
- [ ] `probe_crosshair` automation action exists and is wired through the
      shared automation dispatcher
- [ ] A new script `android/game_scripts/probe_merged_wall_level1_d1.json5`
      walks several known-bad D1 level 1 poses (light-panel-on-rock and
      sign-on-rock faces) and emits probe lines for each. The pose list
      comes from `game_data_to_copy_to_emulator/` level 1 and the user's
      phone observations; start with at least three distinct merged faces
- [ ] A parallel script `probe_merged_wall_level1_d2.json5` drives D2 level
      1 through comparable merged faces so phase 3 can diff D1 vs D2
- [ ] `android/run-code-quality.ps1 -Fix` passes
- [ ] Both automation scripts pass on the emulator with
      `android\run_test.ps1 -ScriptName probe_merged_wall_level1_d1.json5
      -Game d1` and the D2 equivalent

## Phase 3: Collect evidence, classify the shift

Once the probe logs are available:

- [ ] Run both probe scripts on the emulator, export logs, and tabulate
      `u_shift_hint`, `v_shift_hint`, `orient_agree`, and the bot/ovl
      `gltexture->u/v` values across every probed merged face
- [ ] Run the same scripts on a real phone (via on-device Tap) for one
      D1 capture and one D2 capture to confirm emulator-vs-phone parity
- [ ] Compare across orient values 0/1/2/3 to see whether the shift is
      orient-specific
- [ ] Compare across merge routes (`force_two_pass`,
      `merge_cached`, `legacy_texmerge`) by cycling `m154 exp` between
      probes at a fixed pose. The user's observation that cycling shifts
      the texture is the strongest direct evidence that the per-route UV
      tables disagree, so this comparison should yield the fastest path to
      the offending branch
- [ ] Update the hypothesis ranking from phase 1B based on the numbers
- [ ] Stop. Do not attempt a fix in this plan file; hand the ranked
      evidence to the fix plan

## Phase 3a: create a failing test
- [ ] create a json test for a single d1 level instance that navigates to a location (provided in logs) that is facing a misaligned face
- [ ] using new introspection bits, test for the correct alignment
- [ ] ensure the test fails because of the existing misalignment

## Phase 4: Hand-off and follow-up plan seeding (no fixes here)

- [ ] Write a short findings summary at the bottom of this file with the
      top suspect(s) and the probe data that supports the ranking
- [ ] If the findings point at one clear path (for example, GPU FBO orient
      table mismatch with CPU), create a follow-up plan file
      `plan_merged_wall_texture_shift_fix.md` that references this plan's
      probe corpus and captures exactly the code locations to modify
- [ ] If the findings are ambiguous, add a second-pass instrumentation
      section to this plan (phase 2.5) listing the next probe fields
      needed and stop; do not start patching on incomplete evidence

## Cross-cutting constraints

- Do NOT rely on screenshots or pixel OCR. Every claim about the shift
  must be backed by log numbers from `[mwall_tap_probe]`
- All new C code lives under
  `android/app/src/main/cpp/shared/merged_wall_debug.c` and
  `android/app/src/main/cpp/` (or the corresponding header). D1/D2 source
  edits in `d1/` and `d2/` must stay minimal and be `#ifdef __ANDROID__`
  guarded, reusing the existing `g_android_draw_face_ctx` and the
  `piggy_game_bitmap_name` / `piggy_bitmap_get_flags` accessors
- All new kotlin is under
  `android/app/src/main/java/com/dxxredux/app/VideoInfoOverlay.kt` or a
  small helper; no new kotlin classes beyond what is strictly required to
  render the button and post the JNI request
- New logs go through `debug_log(DLOG_TEXTURE, ...)` and therefore require
  the launcher-side `dlog_texture_enabled` preference to persist to file.
  The probe button itself does NOT toggle that preference; document in the
  README that users need to enable Texture logging in the launcher before
  the probe output appears in `files/debuglogs/`
- Keep `printable ASCII`, no emoji, no em-dashes, short self-contained
  `printf` lines without trailing periods, per repo style

## Work item checklist

- [x] Phase 1A: trace UV flow end-to-end and write Findings section
- [x] Phase 1B: rank hypotheses H1..H6
- [x] Phase 2A: extend merged-wall snapshot request with a `crosshair` mode
      and implement `[mwall_tap_probe]` logging in
      `merged_wall_debug.c`
- [x] Phase 2B: add `mwall tap: Probe` button to `VideoInfoOverlay.kt`
- [x] Phase 2C: add `probe_crosshair` automation action
- [x] Phase 2D: commit two new JSON5 probe scripts under
      `android/game_scripts/`
- [x] Phase 2D: run `android/run-code-quality.ps1 -Fix`
- [x] Phase 2D: run both probe scripts on the emulator and confirm
      probe output appears in the exported debug log
- [x] Phase 3: tabulate the current D2 and provisional D1 emulator probe
  output, including route and shift fields
- [x] Phase 3: rerun D2 with a fixed pose using
  `probe_merged_wall_level1_d2_phase3_equal_pose.json5`
- [x] Phase 3: extract the 2026-04-20 phone D1 tap poses from the exported
  debug log and separate real probe taps from duplicate snapshot taps
- [ ] Phase 3: replace the provisional D1 `face_first_merged` probe target
  with the known-bad phone pose list
- [ ] Phase 3: unify the Video Info merged-wall tap UI to one end-user
  action that sends the full probe request
- [ ] Phase 3: collect emulator + phone logs, tabulate shift values,
      update hypothesis ranking

### Phase 2 verification note

- D2 emulator verification passed with all three labels completing through
  `probe_crosshair` and writing `[mwall_tap_probe]` blocks for
  `seg=83 side=3 face=1`
- D2 route observations from the durable automation log:
  `d2_default -> old_texmerge/auto_old_texmerge`,
  `d2_two_pass -> force_two_pass/gpu_two_pass`,
  `d2_forced_legacy -> old_texmerge/auto_old_texmerge`
- D2 probe logs include the new `kind=face`, `kind=derived`,
  `kind=vertex`, `kind=tex`, and `kind=merged` lines
- D1 emulator verification also passed and wrote `[mwall_tap_probe]`
  blocks, but the current script is only a provisional `face_first_merged`
  fallback. It landed on `seg=0 side=1 face=0` on level 1 and all three
  labels stayed on `merge_cached/gpu_cached_single`
- Keep the current D1 script as proof that the probe pipeline works on D1,
  but treat it as instrumentation scaffolding. Phase 3 still needs the
  intended known-bad D1 level-1 pose list from phone observations before
  the alignment data is representative of the reported bug

### Phase 3 groundwork: current dataset

| label | script | pose | route | merge_impl | orient | ovl path | flip axis | u_shift_hint | v_shift_hint | notes |
| ----- | ------ | ---- | ----- | ---------- | ------ | -------- | --------- | ------------ | ------------ | ----- |
| `d2_equal_default` | `probe_merged_wall_level1_d2_phase3_equal_pose.json5` | `seg=83 side=3 face=1 distance=12.0` | `old_texmerge` | `auto_old_texmerge` | `0` | `B3_orient0` | `none` | `0.170898` | `0.000000` | normalized D2 baseline |
| `d2_equal_two_pass` | `probe_merged_wall_level1_d2_phase3_equal_pose.json5` | `seg=83 side=3 face=1 distance=12.0` | `force_two_pass` | `gpu_two_pass` | `0` | `B1_orient0` | `none` | `0.171310` | `0.003510` | same pose as the other D2 rows |
| `d2_equal_forced_legacy` | `probe_merged_wall_level1_d2_phase3_equal_pose.json5` | `seg=83 side=3 face=1 distance=12.0` | `old_texmerge` | `auto_old_texmerge` | `0` | `B3_orient0` | `none` | `0.170898` | `0.000000` | matches `d2_equal_default` |
| `d1_default` | `probe_merged_wall_level1_d1.json5` | `seg=0 side=1 face=0 distance=6.0` | `merge_cached` | `gpu_cached_single` | `0` | `B2_orient0` | `V` | `0.000000` | `0.000000` | provisional first-merged face, not the reported bad pose |
| `d1_two_pass` | `probe_merged_wall_level1_d1.json5` | `seg=0 side=1 face=0 distance=6.0` | `merge_cached` | `gpu_cached_single` | `0` | `B2_orient0` | `V` | `0.000000` | `0.000000` | force-two-pass toggle did not change the route on this face |
| `d1_forced_legacy` | `probe_merged_wall_level1_d1.json5` | `seg=0 side=1 face=0 distance=6.0` | `merge_cached` | `gpu_cached_single` | `0` | `B2_orient0` | `V` | `0.000000` | `0.000000` | old-legacy toggle also stayed on cached merge |

- Current D2 data already weakens the hires-padding family H1/H2 for the
  probed metl154 face, because both the base and overlay textures logged
  `w=64 h=64 u=1.000 v=1.000`
- The normalized D2 rerun passed on the emulator and gave a clean same-pose
  comparison using request frames `15`, `23`, and `33` for
  `d2_equal_default`, `d2_equal_two_pass`, and `d2_equal_forced_legacy`
- The normalized D2 rerun shows that the large earlier D2 two-pass delta was
  mostly a pose artifact. At equal distance, `d2_equal_two_pass` moved to
  `u_shift_hint=0.171310` and `v_shift_hint=0.003510`, which is much closer
  to the old-texmerge result than the earlier distance-4 sample
- That weakens the current use of D2 as the primary evidence for a large
  route-only anchor shift. The route still changes the sample-path tag and
  the UV span values (`u_span=1.339508`, `v_span=0.635803` for the equal-pose
  two-pass sample versus `u_span=1.444336`, `v_span=1.048340` for the
  old-texmerge samples), but the simple shift-hint metric no longer shows a
  large D2 mismatch by itself
- Current D1 data is intentionally provisional. It proves the D1 probe
  pipeline and the new log fields work, but it is not representative of the
  reported user bug yet because it used `face_first_merged` instead of a
  known-bad face from the phone corpus
- The provisional D1 capture is still useful because the chosen face stayed
  on `merge_cached/gpu_cached_single` even after both route toggles. The
  eventual known-bad D1 pose list therefore needs exact face identities,
  not just any merged wall on level 1
- Keep the original `probe_merged_wall_level1_d2.json5` numbers only as a
  historical note for the first route-diff capture. Use the equal-pose D2
  script for any future route-only comparisons

### Phase 3 phone tap intake: 2026-04-20 exported D1 log

Source log: `android/temp_game_logs/debuglog_20260420_130009.txt`

- The exported phone log contains three real `request=probe` taps and three
  separate `request=snapshot` taps at nearly the same poses. The duplicate
  snapshot requests explain why the user could not tell which button they had
  used. For bug-report intake, the user-facing overlay should expose only one
  merged-wall tap action
- Probe tap 1, light-plus-rock face, default route:
  `request_frame=522`, pose replay
  `segment=80 x=-136.005890 y=-135.121704 z=185.405533 pitch=1204 bank=671 heading=-17523`
  landed on `seg=83 side=4 face=0`, `tmap2=0x411f`, `orient=1`,
  `route=merge_cached`, `merge_impl=gpu_cached_single`, `ovl=ceil030`,
  `ovl_sample_path=B2_orient1`, `ovl_flip_axis=U`,
  `flip_screen_axis=diagonal`
- Snapshot-only duplicate for the same light face:
  `request_frame=565`, pose replay
  `segment=80 x=-136.057434 y=-134.645432 z=185.368805 pitch=1204 bank=671 heading=-17523`
  This is not a fourth face capture, just the separate snapshot button on the
  old UI
- Probe tap 2, broken-panel-plus-rock face, forced legacy route:
  `request_frame=949`, pose replay
  `segment=94 x=-210.219879 y=-141.744644 z=238.903458 pitch=2161 bank=1207 heading=3694`
  landed on `seg=94 side=3 face=0`, `tmap2=0x415c`, `orient=1`,
  `route=old_texmerge`, `merge_impl=old_texmerge`, `ovl=blown03`,
  `ovl_sample_path=B3_orient1`, `ovl_flip_axis=none`,
  `flip_screen_axis=none`
- Snapshot-only duplicate for the legacy panel pose:
  `request_frame=982`, pose replay
  `segment=94 x=-210.244751 y=-141.186951 z=239.038391 pitch=2161 bank=1207 heading=3694`
- Probe tap 3, broken-panel-plus-rock face, default cached route:
  `request_frame=1265`, pose replay
  `segment=94 x=-210.244385 y=-141.217422 z=239.031479 pitch=2161 bank=1207 heading=3694`
  landed on the same `seg=94 side=3 face=0`, `tmap2=0x415c`, `orient=1`, but
  switched to `route=merge_cached`, `merge_impl=gpu_cached_single`,
  `ovl_sample_path=B2_orient1`, `ovl_flip_axis=U`,
  `flip_screen_axis=horizontal`
- Snapshot-only duplicate for the cached panel pose:
  `request_frame=1232`, pose replay
  `segment=94 x=-210.247971 y=-141.122604 z=239.054062 pitch=2161 bank=1207 heading=3694`
- The practical Phase 3 follow-up is therefore a four-sample D1 automation
  script that replays two exact phone poses under two experiment states:
  `light_default`, `light_forced_legacy`, `panel_default`,
  `panel_forced_legacy`
- 2026-04-20 emulator replay note: the first scripted rerun accidentally used
  `merged_wall_experiment=1`, which does not select forced legacy. After
  correcting the scripts to `merged_wall_experiment=10`
  (`force_legacy_texmerge`), the D1 phone-pose replay switched the forced rows
  from `route=merge_cached` / `merge_impl=gpu_cached_single` to
  `route=old_texmerge` / `merge_impl=old_texmerge` as intended. For the two
  current phone poses, `orient`, `u_shift`, and `v_shift` still matched across
  both routes: light face `seg=83 side=4 face=0` stayed `orient=1`,
  `u_shift=0`, `v_shift=0`, `u_span=1.0`, `v_span=0.935059`; panel face
  `seg=94 side=3 face=0` stayed `orient=1`, `u_shift=0`, `v_shift=0`,
  `u_span=1.0`, `v_span=0.999512`. This means the route toggle is verified,
  but these two D1 phone poses are not yet a shift-separating repro
- 2026-04-20 newest-build phone log note: `debuglog_20260420_142105.txt`
  captured a better D1 level-1 repro on `seg=92 side=1 face=0`
  (`rock001` + `blown07`). The default tap hit
  `route=merge_cached` / `merge_impl=gpu_cached_single` with
  `ovl_sample_path=B2_orient3`, `ovl_flip_axis=U`, and
  `flip_screen_axis=vertical`. The forced-legacy tap hit the same face with
  `route=old_texmerge` / `merge_impl=old_texmerge`,
  `ovl_sample_path=B3_orient3`, `ovl_flip_axis=none`, and
  `flip_screen_axis=none`
- For that face, the current derived probe fields still matched between the two
  taps: `u_min=0`, `u_max=1`, `v_min=0.034668`, `v_max=0.659668`,
  `u_shift=0`, `v_shift=0.034668`, `u_span=1.0`, `v_span=0.625`. That rules
  out a face-vertex UV change for this repro. The remaining difference is in
  route-specific overlay sampling, not in the face UVs themselves
- Follow-up for this tranche: make the probe log route-aware cached-vs-legacy
  overlay anchor values, because the old `gpu_anchor` vs `cpu_anchor`
  instrumentation was mistakenly calling the same orient-only helper twice and
  could not expose the large visual shift seen on the phone
- Workflow follow-up for this tranche: widen `Run-TestMenu.ps1` JSON5
  discovery beyond `test_*.json5` so the new `probe_*.json5` scripts are
  directly runnable from the menu without renaming them
- 2026-04-20 harness note: the first `probe_*.json5` files were written as
  launcher scripts (`enter_launcher` + launcher `tap_button` launch). On the
  current emulator this stalled after `Game surface created, gameStarted=true`
  and never wrote `automation_log.jsonl`. Rewriting the probe files as direct
  game-launch scripts fixed the issue immediately: `run_test.ps1`
  `probe_merged_wall_level1_d1_phone_poses.json5 -Game d1` passed, and
  `probe_merged_wall_level1_d2_phase3_equal_pose.json5 -Game d2` also passed.
- For the launcher-pref tranche, the probe files were moved back onto the
  launcher path, but with `setup_command write_probe_debug_prefs` plus
  `enter_game` instead of launcher `tap_button` launch. That path now verifies
  cleanly: `probe_merged_wall_level1_d1_phone_poses.json5 -Game d1` and
  `probe_merged_wall_level1_d2_phase3_equal_pose.json5 -Game d2` both pass,
  `setup_introspect.json` shows all requested debug prefs enabled, and
  `adb logcat -d -s DXX-Setup:* DXX-LauncherScript:*` confirms
  `write_probe_debug_prefs: enabled=true` plus the launcher `ASSERT_PASS`
  checks before `enter_game`
- 2026-04-20 runner note: `Watch-AutomationResult` originally passed launcher
  tags to `adb logcat -s` as one combined string and only echoed
  `DXX-Automate` lines, which hid successful launcher-pref assertions in the
  normal `run_test.ps1` output. `android/test_helpers.ps1` now passes
  `DXX-Automate:*`, `DXX-LauncherScript:*`, and `DXX-Setup:*` as separate tag
  args and echoes launcher/setup lines for launcher scripts
- 2026-04-20 live-view follow-up: the next evidence gap is no longer route
  selection. The user confirmed the launcher prefs are correct and the active
  emulator already shows two visually obvious problems that the current probe
  metrics do not yet express well enough: (1) toggling `default` vs `legacy`
  visibly moves the currently viewed merged-wall face, and (2) nearby strip
  lights in the same room are visibly misaligned on every setting
- Current direction for the next tranche:
  - capture the exact live crosshair face from the current emulator session and
    turn it into a reproducible `face_view` or `pose_view` automation target
  - extend the merged-wall probe with a face-normalized final-frame sample so
    the logs reflect the rendered placement on the selected face instead of
    only route-mapped overlay-anchor math
  - use that same face-normalized sample path to log the always-wrong strip
    light pose in the same room, so both the route-sensitive shift and the
    route-insensitive misalignment can be seen in automation output

### 2026-04-20 rendered-sample tranche: current-room emulator repro

- The new face-normalized render sampler is now live in the probe path. It
  records an `8x8` framebuffer-luma grid for the selected face plus summary
  metrics: `valid_cells`, `render_hash`, `luma_min/max`, `hot_cells`, and
  `hot_xy`. The room-focused automation script is now
  `android/game_scripts/probe_merged_wall_level1_d1_current_room.json5`
- Validation run: `run_test.ps1 -ScriptName
  probe_merged_wall_level1_d1_current_room.json5 -Game d1 -Install` passed in
  the emulator. The authoritative evidence for this tranche came from
  `temp/probe_current_room_debuglog_161257.txt`, which was pulled from app file
  `files/debuglogs/debuglog_20260420_161257.txt`
- Post-run harness note: `automation_log.jsonl` did not survive long enough to
  use as the post-run source of truth here, so the native debug log is the
  reliable artifact for the render-sample evidence
- Request-frame order in that script/debug log is now stable:
  - `request_frame=18`: `room_s1_f0_default`
  - `request_frame=26`: `room_s1_f0_legacy`
  - `request_frame=36`: `room_s4_f0_default`
  - `request_frame=46`: `room_s4_f0_legacy`
  - `request_frame=53`: `room_s4_f1_default`
  - `request_frame=62`: `room_s4_f1_legacy`
- Current live face under the user's emulator view is confirmed and reproducible:
  `seg=83 side=1 face=0`
  - default / cached merge (`request_frame=18`):
    `hash=0x9956bd8f hot_xy=0.452/0.312 hot_cells=3 luma=0..244`
    rows: `1aeef!..` / `72226...` / `1000....`
  - legacy / old texmerge (`request_frame=26`):
    `hash=0x991529b0 hot_xy=0.454/0.312 hot_cells=3 luma=17..244`
    rows: `5aeef!..` / `98662...` / `1131....`
  - Interpretation: the face identity, orient, and UV-span math stayed the
    same, but the final rendered sample changed. This is the missing proof that
    the probe can now see the same default-vs-legacy placement difference that
    is visually obvious in the emulator
- Nearby strip-light candidate `seg=83 side=4 face=0` also shows a strong
  rendered change between routes:
  - default (`request_frame=36`):
    `hash=0x8cdb5b6e hot_xy=0.683/0.303 hot_cells=11 luma=0..226`
    rows include `.!aaaaaa`, `..2eeeee`, `...22211`, `....2300`
  - legacy (`request_frame=46`):
    `hash=0x130520cf hot_xy=0.620/0.303 hot_cells=9 luma=0..226`
    rows include `.!aaaaa4`, `..2eeee4`, `...26781`, `....2500`
- Nearby strip-light candidate `seg=83 side=4 face=1` also shows a strong
  rendered change between routes:
  - default (`request_frame=53`):
    `hash=0x6b158e30 hot_xy=0.391/0.750 hot_cells=8 luma=0..226`
    rows include `87777...`, `7eeee2..`, `8eee2e!.`, `001401!!`
  - legacy (`request_frame=62`):
    `hash=0xc298d7ec hot_xy=0.438/0.688 hot_cells=5 luma=24..226`
    rows include `88877...`, `2eeeee..`, `223121!.`, `222212!!`
- Practical conclusion from this tranche:
  - the debugger is no longer limited to route/anchor metadata; it now records
    face-local rendered evidence that changes when the emulator image changes
  - `83/1/0` is the correct current-face repro target for the user's live view
  - `83/4/0` and `83/4/1` are good room-local strip-light follow-up targets for
    the next isolation pass because they produce large, stable render-grid
    signatures and visibly different hot-region placement

## Phase 5: Root-cause synthesis (2026-04-20)

### Integrated evidence

Three facts must be explained at once:

1. Toggling the `m154 exp` experiment between default and force-legacy or
   force-two-pass visibly moves the overlay on `seg=83 side=1 face=0` in D1
   level 1. Probe hashes and hot-cell centroids agree with the eye:
   `default hash=0x9956bd8f hot_xy=0.452/0.312` vs
   `legacy hash=0x991529b0 hot_xy=0.454/0.312`. Row grids differ in interior
   cells only; the face outline stays fixed.
2. Neighbor faces in the same room show the same style of route
   disagreement: `83/4/0` goes `0x8cdb5b6e` -> `0x130520cf`; `83/4/1` goes
   `0x6b158e30` -> `0xc298d7ec`. All three faces are orient=1, cached vs
   legacy.
3. The user also reports merged faces and strip-light poses that look wrong
   on every setting, not just with an experiment toggled. Those cases are
   not route-sensitive in the current probe; the experiments are only a
   live toggle that animates part of the bug.

### Cross-route anchor numbers match a clean mirror, not a random shift

For every probed orient=1 face the derived anchors are related by an exact
axis flip, cached vs legacy:

| face     | cached_anchor_uv           | legacy_anchor_uv           |
|----------|----------------------------|----------------------------|
| `83/1/0` | `0.916504 / 0.880371`      | `0.083496 / 0.880371`      |
| `83/4/0` | `0.000000 / -0.000000`     | `1.000000 / -0.000000`     |
| `83/4/1` | `0.999512 / -0.000000`     | `0.000488 / -0.000000`     |

Cached_u + legacy_u is 1.0 exactly or one texel off, while the v anchors
agree. The probe's `route_agree=0` is the boolean form of the same
observation. That is a U-axis mirror of the overlay between the two routes,
not an arbitrary shift, which rules out filtering, precision, and hires NPOT
sizing as the primary cause.

### Why a clean U mirror appears on orient=1 faces

Walk the three merge branches using the actual numbers:

- Branch 1, force_two_pass. The face is drawn once per layer with its own
  `uvl_list`. Overlay UVs are rotated by the orient table at
  `d1/arch/ogl/ogl.c:2992..2993`: `{(u,v), (1-v,u), (1-u,1-v), (v,1-u)}`.
  That matches the CPU `merge_textures_new` orient table at
  `d1/main/texmerge.c:302..360` pixel-for-pixel. No FBO involved.
- Branch 3, legacy CPU via `texmerge_get_cached_bitmap`. Same orient table
  in the pixel loop. Output is a 64x64 palette bitmap that flows through
  the normal single-texture path with no UV surprises.
- Branch 2, merge_cached (`ogl_android_get_cached_plain_texmerge_bitmap`,
  `d1/arch/ogl/ogl.c:1994`). Draws a full-screen quad into an FBO whose
  color attachment is the cached texture. Quad vertices at
  `d1/arch/ogl/ogl.c:2005..2010` are `{(-1,+1), (+1,+1), (+1,-1), (-1,-1)}`
  and per-corner base at `d1/arch/ogl/ogl.c:1966..1968` is
  `base_u = {0,1,1,0}`, `base_v = {0,0,1,1}`. The same static arrays live
  at `d2/arch/ogl/ogl.c:1969`. `ogl_android_texmerge_build_uvs` multiplies
  those bases through the orient table to produce the overlay UVs and
  directly through `bot_u_max/bot_v_max` to produce the bottom UVs.

The FBO quad is drawn with `glViewport(0, 0, width, height)`. In GL ES, NDC
y=-1 lands at framebuffer pixel y=0 and NDC y=+1 lands at pixel y=height.
The same color attachment is later sampled as a texture where v=0 reads
pixel y=0 and v=1 reads pixel y=height-1. Therefore:

- corner `i=3`, NDC `(-1,-1)`, base `(0,1)` is written at framebuffer
  `(0,0)` and read back at texture UV `(0,0)`. Texture UV `(0,0)` reflects
  base `(0,1)`, not base `(0,0)`
- in general, sampling the cached texture at face UV `(u,v)` reads the
  pixel drawn with base `(u, 1-v)`

Plugging that into each orient column and comparing to CPU
`merge_textures_new`:

| orient | CPU overlay read at | GPU cached overlay at | delta in face UV |
|--------|---------------------|-----------------------|-------------------|
| 0      | `(u, v)`            | `(u, 1-v)`            | overlay V-flipped |
| 1      | `(1-v, u)`          | `(v, u)`              | overlay U-mirrored|
| 2      | `(1-u, 1-v)`        | `(1-u, v)`            | overlay V-flipped |
| 3      | `(v, 1-u)`          | `(1-v, 1-u)`          | overlay U-mirrored|

The bot layer is also sampled at base `(u, 1-v)` instead of `(u, v)`, so
the entire cached composite is the CPU-reference composite flipped
vertically. Bot is `rock001`, noise-dominant and near-symmetric, so the
flip is invisible on the bot. Overlay content (strip lights, light panels,
signs) carries the visible pose, so the flip shows up as a U-mirror on
orient 1 and 3 faces and a V-flip on orient 0 and 2 faces. That matches the
three orient=1 probe rows above.

### Why the toggle moves textures but many stock faces still look fine

Branch 2 is the Android default. Branches 1 and 3 agree with each other and
with the DOS/CPU reference. Toggling from default to either experiment
switches between GPU-flipped overlay and CPU-correct overlay, which is the
live animation the user described. Many merged faces look acceptable under
the default because the overlay is near-symmetric or sits on orient=0
faces where the flip is only vertical and small in visible extent. Orient=1
faces with strong horizontal asymmetry such as light panels on walls are
the obvious victims.

### Why some faces look wrong under every setting

The Y-flip explains every route-dependent case seen in the probe. It does
not by itself explain the class the user flagged as wrong on every setting.
Working hypotheses after the render-sample tranche:

- P1 (Medium-Strong): side UVs that intentionally span more than one texture
  tile. Example: `seg=83 side=1 face=0` has
  `u_min=-1.1196, u_max=0.0, v_min=-0.0835, v_max=1.1909`, span roughly
  1.12 x 1.27. GL_REPEAT wraps the 64x64 composite across the face, so any
  asymmetric overlay motif appears at partial offsets on both routes. That
  looks like a persistent mis-alignment even when default and legacy agree
  on placement, because the level geometry does not align the overlay
  bitmap to the face edge.
- P2 (Medium): hires overlays with NPOT padding where `gltexture->u/v` is
  below 1.0. Both branches 1 and 2 use `ovl_u_max, ovl_v_max` when sampling
  the overlay, but branch 1 applies them to face UVs directly, whereas
  branch 2 applies them to base UVs inside the cached FBO. On faces with
  span outside [0,1] the two branches can disagree at the repeat boundary.
  Likely a contributor on hires packs only; recheck once a wrong-on-every-
  setting face has been probed.
- P3 (Weak): orient interpretation on non-square hires replacements. CPU
  uses `wh = bottom_bmp->bm_w`; GPU uses `ovl_u_max`-scaled coords. Both
  degrade on non-square sources. Parked until a confirmed repro face shows
  non-square overlay dims in a probe.

P1 and P2 are separable by extending the current-room probe to a face the
user considers wrong under every setting. If cached and legacy hashes agree
but the hot region is still visibly off, P1 dominates. If the hashes still
disagree but less than the current trio, P2 contributes.

### Proposed fix for the Y-flip (primary root cause)

Minimal, localized change in `ogl_android_texmerge_build_uvs` in
`d1/arch/ogl/ogl.c:1962..1997` and `d2/arch/ogl/ogl.c:1964..1999`: invert
`base_v` so NDC y=+1 maps to base_v=1 and NDC y=-1 maps to base_v=0. That
brings texture-space v into agreement with base_v, so the cached FBO stores
exactly what CPU `merge_textures_new` would produce. Concretely:

```c
static const GLfloat base_v[4] = {1.0f, 1.0f, 0.0f, 0.0f};
```

No other call site needs to change. Both bot and ovl flow through base_v
identically, so the cached readback stays internally consistent and matches
the CPU reference modulo filter rounding. Downstream consumers continue to
read the cached texture with the face's native `uvl_list`.

Alternatives, for completeness only:

- Flip the NDC y in the quad `vertex_array`. Same effect, larger diff,
  touches both d1 and d2 copies.
- Flip tex_v on the downstream sampler. Rejected; touches the face draw
  path and risks regressing non-cached paths.

### Fix plan sequencing

- [ ] F1: invert `base_v` in `d1/arch/ogl/ogl.c` and `d2/arch/ogl/ogl.c`.
      No other edits.
- [ ] F2: rerun `probe_merged_wall_level1_d1_current_room.json5`. Assert
      that `room_s1_f0_default` and `room_s1_f0_legacy` now produce matching
      `render_hash` and `hot_xy` within noise, same for `s4_f0` and `s4_f1`.
      The probe `route_agree` boolean should flip to 1.
- [ ] F3: visually verify at the same pose under default, legacy, and
      force_two_pass. All three should look identical after F1.
- [ ] F4: capture one strip-light face the user says is wrong under every
      setting, add it to the current-room probe, and discriminate P1 vs P2
      with render-sample evidence post-fix.

### Pending D1 pose intake

The first phone-exported poses are now known. Record any additional targets in
this format before patching the D1 automation script again:

- `label`: short stable name such as `d1_l1_light_panel_rock_left`
- `game`: `d1`
- `level`: `1`
- `segment/side/face`: exact identity if known
- `distance`: preferred probe distance if known
- `description`: what should align and how it is visibly shifted
- `route notes`: what `m154 exp` or other route toggles changed on-device
- [ ] Phase 4: write findings, seed follow-up fix plan if justified

## Open questions to record while working

- Does `side->uvls[]` differ between D1 and D2 in-memory layout when the
  same level 1 mission runs on each engine? The user reports D1 is worse
  than D2 even for the same level, so a per-engine UV table difference is
  worth confirming in phase 1A
- Are there any merged faces on D1 level 1 that use stock piggy for BOTH
  bitmaps (no hires replacement)? If yes, probing those faces tests H1/H2
  cleanly, because both bitmaps will have `gltexture->u/v == 1.0` and
  equal `w/h`; any shift seen there rules out the hires-NPOT hypothesis
  family
- Does the `m154 exp: Compat` mode cycling actually alter the merge route
  the way the user assumes, or does it only alter a shader uniform? This
  should be answered in phase 1A from code, not from logs
