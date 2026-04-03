# Texture Visual Issues Investigation

## Issue 1: door37#7 appears as white (should be transparent)

### Root cause: Art asset issue in d2x-xl texture pack

Investigation findings:
- Source TGA `door37#0.tga` is a 512x4096 strip (8 frames, 512x512 each), 32bpp with alpha_bits=8
- Frame 7 (last frame, door fully open) contains: RGB mean ~252/255 (nearly white), alpha=255 (fully opaque)
- In the original game, when a door is fully open, palette color 255 means "transparent" -- you see through the door
- The hires artist drew frame 7 as opaque near-white pixels instead of using alpha=0 for transparent areas
- Result: The hires texture renders as a white rectangle where you should see through the door
- The KTX2 is correctly ETC2_RGB8 (no alpha needed since source alpha is all 255)

### Additional finding: ImageMagick strips alpha during strip splitting

Even for frames that DO have meaningful alpha in the source strip:
- IM reads the 32bpp TGA but interprets it as `TrueColor` (not `TrueColorAlpha`)
- IM `identify` shows "Type: TrueColor, Channels: 4.0" -- it sees 4 channels but doesn't treat the 4th as alpha
- When cropping and saving back to TGA, IM writes 24bpp (3 channels), dropping the alpha
- This affects all 123 strip textures in the D2-512 pack (42 are doors)

Fix (two-part):
1. `-alpha set` on the IM input to force alpha interpretation before crop:
   ```
   & $MagickPath $safePath -alpha set -crop "..." +repage $safeFramePath
   ```
   Then the output PNG/TGA will have alpha preserved
2. For door37 specifically (and likely other doors), the hires art itself has wrong alpha for "open" frames.
   This is a d2x-xl art deficiency -- not all transparency in the original game was replicated in the hires art.

### Experiment to verify
- Pick a strip texture where alpha IS meaningful (not all 0 or all 255 per frame)
- Add `-alpha set` to the IM crop command in `Split-StripTextures`
- Rebuild the DXA and verify the alpha-bearing frames render correctly
- For door37 specifically: check in-game whether the original piggy texture (without hires) shows proper transparency at frame 7

### Scope
- Low urgency for door37 specifically (art asset issue)
- Higher urgency for the IM alpha-stripping bug (could affect any strip texture with alpha)
- Count of strip textures with actual per-frame alpha variation: unknown (needs batch analysis)

---

## Issue 2: Shootable light overlays appear lower resolution

### Root cause: texmerge creates 64x64 merged bitmap, discarding hires data

Investigation findings:
- On Android (non-OGL_MERGE, GLES 1.1), when a tmap2 overlay has `BM_FLAG_SUPER_TRANSPARENT`:
  ```c
  // render_face(), d2/main/render.c line 251
  if (bm2 && (bm2->bm_flags&BM_FLAG_SUPER_TRANSPARENT)){
      bm = texmerge_get_cached_bitmap( tmap1, tmap2 );
      bm2 = NULL;
  }
  ```
- `texmerge_get_cached_bitmap()` creates a 64x64 merged bitmap from `bm_data` (the original piggy data)
  - It uses `bitmap_bottom->bm_w` (always 64) for the output size
  - It has zero awareness of hires PNG/KTX2 replacements
  - The merge iterates pixel-by-pixel over 64x64 `bm_data` arrays
- The merged 64x64 bitmap then goes to `g3_draw_tmap()` which calls `ogl_loadbmtexture()`
  - `ogl_loadbmtexture` checks `piggy_game_bitmap_name()` -- returns NULL for texmerge bitmaps (not in GameBitmaps[])
  - So the PNG/KTX2 lookup is skipped entirely
  - Result: the 64x64 palette-indexed data is uploaded to the GPU, replacing both hires textures with lores merge

- Virtually ALL shootable lights use `BM_FLAG_SUPER_TRANSPARENT`
  - This means ceil028b overlays on rock356, etc. all render at 64x64
  - Both the wall AND the overlay appear low-res because they're merged into one 64x64 bitmap
  - The debug label system (just fixed) still shows green (hires) because it looks up the original bitmaps

### Possible fixes (ordered by complexity)

#### Option A: Skip texmerge for hires overlays (render two-pass instead)
When both textures have hires replacements, bypass the texmerge merge and render
using the non-OGL_MERGE two-pass path in `g3_draw_tmap_2`:
```c
// In render_face:
#ifndef OGL_MERGE
if (bm2 && (bm2->bm_flags&BM_FLAG_SUPER_TRANSPARENT)){
    // Check if both have hires replacements
    int bottom_hires = (bm->gltexture && bm->gltexture->is_png);
    int top_hires = (bm2->gltexture && bm2->gltexture->is_png);
    if (bottom_hires || top_hires) {
        // Keep bm2 non-NULL so g3_draw_tmap_2 handles them separately
        // The super_transparent flag needs special handling in g3_draw_tmap_2
    } else {
        bm = texmerge_get_cached_bitmap( tmap1, tmap2 );
        bm2 = NULL;
    }
}
#endif
```
Complexity: Medium.
Risk: The super-transparent merge exists for a reason -- the 2-pass rendering
might not handle super-transparency correctly (color 254 = super-transparent needs
special compositing). Would need to verify the visual output matches.

#### Option B: Hires-aware texmerge
Modify texmerge to work at hires resolution when both inputs have hires data.
Instead of merging bm_data (64x64), merge the gltexture data (256x256 or 512x512).
Complexity: High -- would need to read back GPU textures or cache the original PNG data.
Not recommended. Would be a large change to the engine.

#### Option C: OGL_MERGE on Android (shader-based)
Use GL ES 2.0 multi-texturing to blend both textures on the GPU, same as desktop.
Complexity: Very high -- the OGL_MERGE path uses GLSL shaders which require GL ES 2.0.
The Android build currently uses GL ES 1.1 fixed-function pipeline.
Not feasible without a major rendering rewrite.

#### Option D: Pre-merge hires textures in the DXA conversion pipeline
For texture pairs that are known to be merged (BM_FLAG_SUPER_TRANSPARENT overlays),
pre-merge the hires textures at full resolution and store the merge in the DXA.
This would require knowing the overlay combinations at build time.
Complexity: Medium-High. Would need to enumerate all possible overlay combinations.
Not recommended -- too many combinations.

### Recommended approach: Option A
Skip texmerge when hires replacements exist, and render the two textures in
two passes. The BM_FLAG_SUPER_TRANSPARENT handling needs investigation in the
two-pass path to ensure color 254 pixels are handled correctly.

### Experiment to verify Option A
1. In render_face, add a check: if both bm and bm2 have `gltexture->is_png`, skip
   the texmerge and keep bm2 non-NULL
2. The two-pass path in g3_draw_tmap_2 should handle alpha blending naturally
   because the hires textures already have alpha channels where transparency 
   is needed
3. Build and test in level 1: look at shootable lights on walls -- they should
   now appear at hires resolution
4. Verify no visual artifacts from the changed blending

---

## Issue 3: Ship lighting sometimes too bright

### Likely cause: ETC2 compression characteristics + lighting math interaction

Investigation findings:
- Ship/robot textures use the same hires replacement pipeline as wall textures
- Ships use `compute_object_light()` for 3D scene lighting with RGB light values
- The ETC2 encoder preserves colors faithfully but there's quantization inherent
  in 4bpp block compression. Subtle color differences vs the original palette-based
  textures may interact with the fixed-function lighting pipeline differently.
- Additional brightness sources: Robot `glow` field, Netgame.BrightPlayers,
  flare brightness (+F1_0*2)

### Possible causes
1. **Gamma/color space mismatch**: The original palette-based textures are in sRGB
   gamma space. When converted to ETC2, the colors are preserved in sRGB but the
   lighting math operates in linear space. This can cause over-brightening of
   already-bright areas. Not specific to ships though.
2. **ETC2 quantization**: Block compression can shift colors slightly. Bright textures
   may be quantized upward (toward white), making lit areas even brighter.
3. **Texture brightness differences**: The hires replacement textures may simply be
   brighter than the originals (art choice by d2x-xl creator).

### Experiments
1. Capture screenshots with hires and without (toggle tex_overlay to compare)
2. Use the introspection API to check lighting values on the ship being viewed
3. Try reducing global gamma or brightness to see if the effect lessens
4. Compare a specific ship texture's average brightness between original and hires

### Low priority
This may be inherent to the art pack and not fixable without per-texture correction.
A global gamma correction tuning option could help.

---

## Issue 4: HUD gauge textures don't match their backgrounds

### Likely cause: Separate hires textures with brightness mismatch

Investigation findings:
- The cockpit HUD is a single background bitmap (cockpit.ktx2, cockpitb.ktx2)
- Gauge elements (energy, shields, keys, targeting, lock indicator) are SEPARATE
  bitmaps drawn on top via 2D blit (hud_bitblt)
- All gauge elements go through the same hires replacement pipeline
- HUD elements are rendered WITHOUT 3D lighting -- pure 2D bitmap copy
- If the "lock indicator" hires replacement was made with a different brightness
  than the cockpit background hires replacement, they'll look mismatched

### Possible causes
1. **Different source artists**: The cockpit background and individual gauge overlays
   may have been created by different artists or at different times in d2x-xl
2. **Alpha blending issues**: If gauge overlays lack proper alpha matching the cockpit
   background, edges may show as brighter/darker than expected
3. **Color space mismatch**: The gauge texture and cockpit background may use slightly
   different color profiles

### Experiments
1. Extract the specific "lock" gauge bitmap from the DXA and compare its brightness
   with the corresponding area of the cockpit background
2. Check if the original (non-hires) gauge textures match better
3. Check whether the gauge overlay has alpha that should blend with the background
   but the ETC2 format lost the alpha (similar to issue 1)

### Fix options
- Most likely fix: adjust the brightness/alpha of the specific mismatched gauge textures
  in the DXA conversion pipeline (brightness correction filter)
- Or let the user tune cockpit brightness separately in settings

---

## Priority order

1. **Issue 2** (shootable light resolution) - Code bug with clear fix path; affects many 
   walls in every level. High visual impact.
2. **Issue 1 IM alpha fix** (strip splitting drops alpha) - Code bug in conversion pipeline;
   may affect multiple textures beyond doors. Medium visual impact.
3. **Issue 4** (HUD mismatch) - Art/pipeline issue; noticeable but only in cockpit view.
   Low-medium visual impact.
4. **Issue 3** (ship brightness) - Subtle visual quality issue; may be inherent to art pack.
   Low priority.
5. **Issue 1 door37 art** (d2x-xl art has wrong alpha) - Asset deficiency; would need
   manual fix or auto-alpha-from-piggy tool. Lowest priority.
