# Plan: Frame bar fix, supertransparency regression, graphics settings consolidation

## Task 1: Frame duration bar color/marker fix
**Problem**: At 25fps/40ms target, bar shows red/yellow instead of green. White marker is at wrong reference point (not useful).
**Root cause**: Bar color thresholds use 30ms as green cutoff (leftover from 30fps design). Marker is at the 40ms point in a 60ms range but user sees it as confusing.
**Fix**:
- [x] Update bar color thresholds: green <= 40000us (at budget), yellow <= 55000us, red > 55000us
- [x] Update frame time text paint thresholds to match: green <= 40000us, yellow <= 55000us, red > 55000us
- [x] Remove the white marker line entirely

## Task 2: Supertransparency regression (no hires textures)
**Problem**: First door opening shows rock flash instead of transparency; subsequent openings correct.
**Root cause**: In `g3_draw_tmap_2`, the `super` variable is computed BEFORE `ogl_bindbmtex(bmovl)`. On first encounter, `bm_flags` is BM_FLAG_PAGED_OUT and `gltexture_mask` is NULL, so super=false. After bindbmtex pages in the bitmap and generates the mask, the stale super=false causes tex2 shader (no mask) to be used.
**Fix**:
- [x] d2/arch/ogl/ogl.c: Move `super` computation to after both `ogl_bindbmtex` calls
- [x] d1/arch/ogl/ogl.c: Same fix

## Task 3: Move resolution settings into Graphics section
**Problem**: Resolution picker is a separate section at the top. User wants it consolidated into the Graphics section.
**Fix**:
- [x] Move ResolutionPickerAdvanced content into GraphicsSettingsSection (at the top, before tex filter)
- [x] Remove the standalone ResolutionPickerAdvanced call and its divider from the page layout
- [x] Fix updateDescentCfgResolution to write to all config files (like updateAllConfigFiles does), not just root descent.cfg
- [x] Build and verify
