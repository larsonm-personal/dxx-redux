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
