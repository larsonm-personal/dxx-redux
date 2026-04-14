## Phase 4 Metl154 Regression Follow-up

### Goal
- explain why the metl154 result did not change after the converter work
- identify why the newest pack regressed door borders
- land the narrowest fix that removes the regression and keeps investigation moving toward the real metl154 cause

### Steps
- [x] compare D2X-XL runtime semantics for supertransparency vs ordinary alpha
- [x] verify whether the new converter heuristic generates masks for representative door textures
- [x] narrow or revert the heuristic if it is broadening ordinary alpha into supertransparency
- [x] capture the strongest remaining runtime clue after the converter rollback
- [ ] continue tracing the real metl154 failure path after the regression is removed
- [x] run code-quality checks

### Result
- D2X-XL uses exact `120,88,128` key-color detection for TGA supertransparency and does not broaden door-frame masks with an import tolerance
- The converter-side tolerance was expanding some keyed door masks while doing nothing for `metl154`, which has no key-color pixels at all
- The narrowed converter now keeps exact-key masks for doors, keeps the ETC2 edge-bleed cleanup for alpha-cutout TGAs, and stops emitting a synthetic `metl154_mask.png`
- The regenerated validation pack contains `door04#0_mask.png`, omits `metl154_mask.png`, and still includes `metl154.ktx2`
- Doors working again while the rock issue remains isolates the regression to the non-mask path: keyed `*_mask.png` overlays now behave, while `metl154` still goes through the plain alpha `ogl_prog_tex2` shader
- The new manual clue that the defect appears only from one direction points away from converter content and toward runtime orientation-sensitive sampling in `g3_draw_tmap_2`: the overlay UV remap changes with `tmap2` orientation, so a bug that only appears on one approach is most likely in the unmasked OGL_MERGE alpha path or its filter/wrap behavior, not in the archived texture data itself

### Latest Log Review
- The captured bad-direction runtime log keeps `metl154` on `shader=plain` with `mask=0`, so the failure is still entirely in the non-mask merge path
- Every captured bad-direction draw stayed at `orient=0`, which rules out the earlier idea that the remaining defect depended on the `switch(orient)` remap itself
- The bad face samples `metl154` with overlay UVs far outside the base 0..1 tile range, including negative U and repeated U over more than three tiles; that points to oblique minification and filtered alpha averaging instead of a bad archive or missing mask
- The current plain merge shader multiplies in the sampled overlay alpha directly instead of thresholding it, so a binary cutout replacement like `metl154` can lose its holes on glancing views when mip/linear filtering averages the alpha toward opaque

### Current Fix
- landed a plain-merge shader cutoff so `BM_FLAG_TRANSPARENT` secondary overlays use a binary `0.5` alpha threshold before color mixing instead of trusting filtered alpha directly
- kept the masked `*_mask.png` overlay path unchanged, so the restored door behavior should not regress
- validated the code path with Android `:app:assembleDebug`, `:app:testDebugUnitTest`, and `android\run-code-quality.ps1 -Fix`
- manual on-device retest is the next step to confirm that the previous bad direction now preserves the `metl154` cutout

### Second Log Review
- the newer build still showed one-sided failure even after the plain-shader alpha cutoff landed
- the `metl154diag` lines before the inferred save/load transition and after it stayed identical in all logged fields: `orient=0`, `shader=plain`, `bot=rock313`, `mask=0`, and the same general repeated-U ranges
- the save/load transition itself did not emit an explicit restore log line, but there is a clear runtime break before the final `metl154diag` block where texture activity resumes and the later samples match the user's working-side report window
- because the logged values were the same on both sides, the remaining difference is likely implicit sampler LOD or derivative behavior that the diagnostic does not capture, not overlay orientation or bitmap flags

### Current Direction
- the cutoff-only shader change was too late in the pipeline to recover cutout detail once mipmapped sampling had already averaged the alpha
- the newest log from the rebuilt app made the remaining problem clearer: the transparent secondary-overlay override was still using linear filtering, so binary alpha was still being smoothed even after the cutoff change
- the current runtime fix in `g3_draw_tmap_2` now forces `GL_NEAREST` min and mag filtering for plain `BM_FLAG_TRANSPARENT` secondary overlays while keeping the door mask path unchanged
- Android validation passed for that follow-up fix with `:app:assembleDebug`, `:app:testDebugUnitTest`, and `android\run-code-quality.ps1 -Fix`
- the next step is another on-device retest focused on the previously bad side first, because if the bug was driven by mip/LOD selection this change should be visible immediately

### Latest Three-Log Review
- `debuglog_20260413_195237.txt` is launcher-state output only and does not include a game render pass for `metl154`
- `debuglog_20260413_195251.txt` mounted `d2-hires-512-textures-ktx2.dxa`, uploaded `metl154` as a 512x512 ETC2 RGBA texture, and still logged the same failure path: `shader=plain`, `mask=0`, `flags=0x9`, `bot=rock313`
- `debuglog_20260413_195316.txt` ran with no active mod path, used the stock 64x64 `metl154` texture, and still logged the same failure path: `shader=plain`, `mask=0`, `flags=0x9`, `bot=rock313`
- that rules out the converter, DXA archive contents, PNG replacement loading, and ETC2 upload as primary causes because the stock asset path reproduces the same defect
- the shared remaining possibilities are now limited to stock/source transparency interpretation or a generic runtime merge/upload bug that affects both stock and replacement assets

### New Diagnostic Direction
- added a one-shot Android texture-source diagnostic in both D1 and D2 OGL loaders for `metl154`
- the next run will emit `[metl154src]` once for stock palette uploads and once for PNG replacement uploads, reporting either palette counts (`idx254`, `idx255`) or RGBA alpha counts (`alpha0`, `alpha255`, `alpha_partial`)
- this is intended to answer the next binary question directly: whether `metl154` actually arrives with transparent texels before `ogl_loadtexture`, or whether the source data is already wrong before the merge shader samples it
- Android validation passed for the diagnostic build with `:app:assembleDebug`, `:app:testDebugUnitTest`, and `android\run-code-quality.ps1 -Fix`

### Follow-up On New Logs
- the newest three logs still reproduce the same plain-path failure with and without `d2-hires-512-textures-ktx2.dxa`, so the bug remains independent of the texture pack content
- the expected `[metl154src]` lines did not appear in those logs at all, which means the load-time source diagnostic is not a reliable observation point for this case
- to remove that blind spot, the existing `[metl154diag]` render-time line now also reports `real_flags`, `src254`, and `src255` by expanding the bitmap on demand and counting the original palette indices at draw time
- that makes the next capture self-contained: it will show the runtime path, the stored bitmap flags, and whether the original `metl154` source actually contains transparent palette pixels in the same line
- Android validation passed again for that follow-up diagnostic build with `:app:assembleDebug`, `:app:testDebugUnitTest`, and `android\run-code-quality.ps1 -Fix`

### Latest Diagnostic Extension
- the latest log evidence answered the source-data question directly: `real_flags=0x9`, `src254=0`, and `src255=1914` prove `metl154` carries ordinary transparent palette pixels at draw time but no supertransparent pixels
- that rules out "missing source transparency" as the remaining theory, so the next branch to separate is runtime sampling location versus alpha loss after sampling
- the current `[metl154diag]` line now also reports a representative wrapped source sample from the average overlay UVs: `sample_uv`, `sample_xy`, and `sample_idx`
- it also reports the live GL texture state for the overlay: `filt=min/mag` and `mips`, so the next capture can confirm whether the transparent overlay is still hitting filtered or mipmapped sampling on device
- the first build attempt after adding that helper failed because `OGL_BINDTEXTURE` is defined later in the file; the helper now uses plain `glBindTexture`, which keeps the diagnostic correct without moving the macro block
- Android validation passed for the updated diagnostic build with `:app:assembleDebug`, `:app:testDebugUnitTest`, and `android\run-code-quality.ps1 -Fix`

### Review Of The New 2050xx Logs
- `debuglog_20260413_205053.txt` mounted `d2-hires-512-textures-ktx2.dxa` and `debuglog_20260413_205119.txt` ran with no active mod path, so the new capture again covers both the 512 pack and stock assets
- both runs now agree on the new runtime fields: `sample_idx=255`, `filt=9728/9728`, and `mips=1`, which means the representative source texel was transparent and the live min/mag filters were both `GL_NEAREST`
- that is enough to rule out the earlier "still linearly filtering on Android" theory for this path, and it also shows the sample was not taken from the texture edge; the logged point is the center row (`sample_y=31`), not the top row
- however, the user's objection is still correct in substance: the current helper only samples one horizontal stripe through the texture, and `metl154` is a horizontal-bar texture, so one center-row sample is not strong enough to represent the full face
- the diagnostic therefore now logs a five-point vertical slice at the wrapped average U across the current overlay V span: `vslice_y` and `vslice_idx`
- the next capture should answer the row-coverage question directly: if those five samples contain a mix of opaque and transparent palette indices, then the face really does cover bar rows and the remaining bug is after source lookup; if they are all transparent, then the current bad face is mostly landing in gap rows
- Android validation passed again for that follow-up diagnostic build with `:app:assembleDebug`, `:app:testDebugUnitTest`, and `android\run-code-quality.ps1 -Fix`

### Review Of The New 2108xx Logs
- `debuglog_20260413_210834.txt` and `debuglog_20260413_210857.txt` answered the vertical-coverage question directly for stock and 512-pack runs
- both runs show the same mixed vertical slice: the current face covers opaque bar rows at the top and bottom (`vslice_idx` values like `193/.../22`, `34/.../34`, `210/.../37`) and transparent rows through the middle (`255/255/255`)
- that means the texture lookup itself is no longer the weak point in the theory; the bad view is not sampling only gap rows, and the source data is still present on both stock and replacement paths
- the user report that the bars sometimes appear and sometimes disappear while the rock remains fully opaque now matches the runtime path closely: `metl154` is an ordinary transparent tmap2 overlay, so the rock base stays opaque by design, while the overlay contribution can flicker if the renderer forces it into binary nearest-sampled behavior
- the previous Android-only plain-path override was doing exactly that: it forced `BM_FLAG_TRANSPARENT` overlays to `GL_NEAREST` min/mag filtering and also forced a `0.5` shader alpha cutoff in the plain merge path
- for a thin horizontal-bar overlay, that turns perspective/minification into a row-selection problem where samples jump between opaque bar rows and transparent gap rows, which explains why the bars pop in and out on top of an always-opaque rock base
- the current fix removes that Android-only nearest-filter override for plain transparent overlays and restores ordinary alpha blending in the plain merge shader by setting the secondary alpha cutoff back to `0.0`
- the masked supertransparent path remains unchanged, so door-mask behavior is still isolated to the `BM_FLAG_SUPER_TRANSPARENT` route
- Android validation passed for the updated build with `:app:assembleDebug`, `:app:testDebugUnitTest`, and `android\run-code-quality.ps1 -Fix`