## Metl154 Plain Alpha Fix

### Goal
- keep mask-backed doors unchanged
- harden the plain tmap2 merge path for binary transparent overlays like `metl154`
- preserve the existing merge path for non-binary alpha cases unless the source bitmap is flagged transparent

### Plan
1. Add a plain-merge shader uniform that can threshold overlay alpha before mixing.
2. Set that cutoff only when the secondary bitmap carries `BM_FLAG_TRANSPARENT` and is not using the mask path.
3. Mirror the change in both D1 and D2 OGL shader code.
4. Run validation and update follow-up notes with the result.

### Status
- [x] Shader cutoff added
- [x] D1 and D2 wired
- [x] Validation run
- [x] Notes updated

### Result
- the plain OGL_MERGE secondary-texture shader now supports a per-draw alpha cutoff for cutout overlays
- `g3_draw_tmap_2` enables a `0.5` cutoff only for non-mask secondary bitmaps flagged with `BM_FLAG_TRANSPARENT`
- mask-backed overlays keep using the existing mask shader path unchanged
- Android validation passed with `:app:assembleDebug`, `:app:testDebugUnitTest`, and `android\run-code-quality.ps1 -Fix`
- manual on-device retest is still needed to confirm that the glancing-view `metl154` holes stay open from the previously bad direction

### Follow-up
- a later on-device retest showed the same one-sided failure even with the shader cutoff enabled
- the second log showed the same logged `metl154` runtime fields on both the failed side and the post-load working side, so the cutoff was not addressing the real difference
- the next runtime fix moved to sampler state: the merge path now forces `GL_NEAREST` min and mag filtering for plain `BM_FLAG_TRANSPARENT` secondary overlays while leaving mask-backed overlays untouched
- the newest runtime log still showed `metl154` on `shader=plain` with the same logged UV and orientation values from both directions, which made the remaining bug most consistent with filtered alpha rather than bad archive data or UV remap state
- Android validation passed again with `:app:assembleDebug`, `:app:testDebugUnitTest`, and `android\run-code-quality.ps1 -Fix`