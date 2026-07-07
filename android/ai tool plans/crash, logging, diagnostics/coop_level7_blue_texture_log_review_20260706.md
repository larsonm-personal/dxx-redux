# Coop level 7 texture log review

## Goal
Use the fresh-start D2 level 7 coop desync log to identify why a texture near start renders visually wrong.

## Plan
- [x] Parse attached coop desync log for fresh-start host and client sequence
- [x] Compare `texdiag` signatures and texture counters across peers
- [x] Inspect code paths that mutate level or texture state when signatures diverge
- [x] Make a focused fix or add narrower logging if the current log is still ambiguous
- [x] Run scoped validation and record outcome

## Findings
- The attached log contains host-side coop desync lines for fresh D2 level 7 starts, but no matching client-side `texdiag` lines.
- Fresh level 7 consistently loaded with `textures=910`, stable `segment_sig=e8f97cd8`, and `invalid_tmaps=11`.
- The first invalid side was `first_bad=26:5:910/0`, which is one past the valid texture range.
- `render_face()` already tried to replace invalid base tmaps with texture 0, but only updated the segment field. It kept using the stale local `tmap1`, so the draw could still index past `Textures[]`.

## Changes
- Updated D1 and D2 `render_face()` to keep local `tmap1` in sync after invalid-base fallback.
- Added a matching invalid-overlay guard for `tmap2 & 0x3FFF`.
- Extended coop desync texture diagnostics with a conditional `texbad[...]` line containing all invalid `seg:side:tmap1/tmap2` refs and a stable `invalid_sig`.

## Validation
- Passed: `.\android\run-code-quality.ps1 -Fix -Paths @('d1\main\render.c','d2\main\render.c','android\app\src\main\cpp\shared\coop_indicator_lines.c')`
- Passed: `.\android\gradlew.bat -p android :app:assembleDebug`

## New log tranche 2026-07-06 14:57
- [x] Extract focused coop, graphics, and texture snapshot lines from `debuglog_20260706_145702.txt`
- [x] Match texture overlay snapshot lines to rendered segment, side, and tmap IDs
- [x] Compare fresh-start `texdiag` and `texbad` output after the renderer fallback fix
- [x] Decide whether the remaining bad texture is an invalid level tmap, a wrong texture upload, or an overlay/merge path issue
- [x] Patch or add narrower logging, then validate

## New log findings
- The bad snapshot is not one of the invalid `tmap1=910` sides. It is `seg=169 side=0 face=0`, with `tmap1=73` (`rock198`) and overlay `tmap2=0x132` (`ceil035`).
- The snapshot draw path is `route=merge_cached merge_impl=gpu_cached_single`, using cached output handle `1743` from base handle `1737` and overlay handle `1742`.
- The bad center sample was blue-ish (`43/45/100/255`) while the surrounding average was gray-ish (`53/52/56/255`), which points at the cached merged-wall texture path or draw state rather than coop level texture-table mutation.
- Existing live cover logging skipped this exact case because the original face context still has `tmap2 != 0`, even though the actual draw is a single cached bitmap.

## New changes
- Added cached-texmerge bitmap lookup inside merged-wall diagnostics.
- Snapshot cover logging now recognizes cached merged-wall single draws and emits `[mwall_cache_live]` with output/base/overlay handles, cache slot/orient, active texture unit bindings, output filters, mip state, and face identity.
- Cached merged-wall covers are now eligible for `[mwall_cover_gpu]` GPU readback, so the next snapshot should show whether the generated cached output is already blue.

## New validation
- Passed: `.\android\run-code-quality.ps1 -Fix -Paths @('android\app\src\main\cpp\shared\merged_wall_debug.c')`
- Passed: `.\android\gradlew.bat -p android :app:assembleDebug`

## New log tranche 2026-07-06 15:28
- [x] Extract cache-live, GPU readback, snapshot, and coop texture diagnostics from `debuglog_20260706_152849.txt`
- [x] Decide whether cached output texture is already blue or draw state corrupts it later
- [x] Patch the cached merge path or add the next narrower diagnostic
- [x] Run scoped validation and record outcome

## 15:28 log findings
- The sampled `rock198 + ceil035` cached merge had plausible color samples in this run, but those samples are not enough to prove the composite is visually correct.
- The framebuffer sample also looked plausible at the tapped point, but the tapped point did not land inside the face polygon, so it cannot be treated as a correctness check for the face.
- The cached-single draw still had stale secondary texture units while drawing: `tex0=1691`, `tex1=1690`, `tex2=1310`. `tex1` was the overlay `ceil035`.
- The stronger finding is that the affected face consistently takes the Android-only `merge_cached` / `gpu_cached_single` route, which creates a reusable FBO-composited bitmap instead of the classic CPU `texmerge_get_cached_bitmap()` result or the live two-texture shader route.
- The secondary texture-unit state leak remains only a speculative state hygiene issue. It should not be treated as the main explanation without a bypass or reference-comparison run.

## 15:28 changes
- Promoted the existing `CLEAR_SECONDARY_UNITS_SINGLE` experiment into the default path for cached merged-wall single draws only.
- The default path now clears texture units 1 and 2 before drawing a cached merged wall as a single bitmap.
- Verbose before/after clear logging remains limited to the experiment mode or a pending snapshot to avoid log spam.

## 15:28 validation
- Passed: `.\android\run-code-quality.ps1 -Fix -Paths @('android\app\src\main\cpp\shared\merged_wall_debug.c')`
- Passed: `.\android\gradlew.bat -p android :app:assembleDebug`

## Reframed analysis 2026-07-06
- [x] Treat the symptom as "visually wrong texture/composite", not a specific sampled color.
- [x] Re-check the selected face identity and route from both `14:57` and `15:28` logs.
- [x] Compare the Android cached premerge route against the classic CPU texmerge and live two-texture paths.
- [ ] Patch the next run to bypass cached premerge or add a direct CPU-reference comparison for cached entries.
- [ ] Run scoped validation and record outcome.

## Reframed findings
- Both logs identify the same focus face: D2 level 7 `seg=169 side=0 face=0`, `tmap1=73` (`rock198`), `tmap2=0x132` (`ceil035`), `orient=0`.
- That face is not one of the invalid `tmap1=910` sides reported by `texbad`, so the earlier invalid-tmap fix is separate from this visible issue.
- The face consistently renders through `route=merge_cached merge_impl=gpu_cached_single`; the cached bitmap is also reused by other faces with the same texture pair/orientation.
- Current logging proves the texture identities and GL handles, but it does not prove the FBO-built cached composite matches the CPU reference merge. A color sample is the wrong proof here.
- Best next diagnostic/fix: disable the Android cached premerge for plain transparent overlays and let the existing live two-texture path draw it, or log a direct CPU-reference hash against the cached output.

## Coop-only analysis 2026-07-06
- [x] Check whether coop changes the level texture table or selected face identity.
- [x] Check multiplayer-only render branches that can affect the cached merge path.
- [x] Rank coop-only hypotheses by how well they match the logs.
- [ ] Test the top hypotheses by toggling render path/cache behavior.

## Coop-only findings
- The selected wall texture IDs stay stable in coop: `seg=169 side=0 face=0`, `tmap1=73`, `tmap2=0x132`. `segment_sig` also stays stable, and `custom_tex=0`, so the current evidence does not point at coop mutating this wall's level texture fields.
- The log's `game_mode=0x1c` is `GM_NETWORK | GM_MULTI_ROBOTS | GM_MULTI_COOP`, which satisfies `GM_MULTI`.
- `render_mine()` explicitly avoids the `ClassicDepth` path whenever `Game_mode & GM_MULTI`; coop therefore uses the 3-pass alpha/object renderer even if comparable single-player runs use the single-pass `ClassicDepth` renderer.
- The affected wall still renders in pass 1, but the GL state/cache history around that pass differs in multiplayer because pass 2 renders remote player objects with multiplayer alt textures, and pass 3/coop indicator lines can leave state for the next frame.
- Coop-only player texture substitution and extra object paging can change texture/cache timing, but it should not change `seg=169`'s `tmap1/tmap2`. Treat it as an indirect state/cache-pressure suspect, not the primary level-data suspect.

## Render-unification direction 2026-07-06
- [x] Identify the non-player-texture render differences between single-player and coop.
- [x] Decide which render path should be the baseline for unification.
- [x] Patch D1/D2 render and OGL frame setup so `ClassicDepth` is honored in multiplayer instead of being force-disabled by `GM_MULTI`.
- [x] Rename UI text from `Classic Depth Ordering (SP)` to `Classic Depth Ordering`.
- [ ] Validate single-player and coop fresh starts, watching for transparent-wall/object occlusion regressions.

## Render-unification notes
- The only legitimate coop-only rendering delta should be the remote player objects and their alternate ship textures. The wall renderer should not branch on coop/network state.
- Current code has two broader multiplayer branches: `render_mine()` uses the 3-pass alpha/object renderer when `Game_mode & GM_MULTI`, and `ogl_start_frame()` enables depth testing when `!ClassicDepth || (Game_mode & GM_MULTI)`.
- The direction should be to remove `GM_MULTI` from these rendering/depth conditions, not to make single-player use the multiplayer path. Single-player with `ClassicDepth` is the known-stable baseline.
- The frame setup should explicitly set depth state every frame: disable depth when `ClassicDepth` is enabled, enable it otherwise. That avoids stale GL depth state after mode/menu/endlevel transitions.
- D1 and D2 need the same change in lockstep. D1 also has an endlevel terrain branch with `!ClassicDepth || (Game_mode & GM_MULTI)` that should be reviewed with the same principle.
- Implemented changes remove `GM_MULTI` from D1/D2 `render_mine()`, D1 object sorting, D1/D2 extended model depth toggles, D1 endlevel terrain depth toggles, and D1/D2 OGL frame depth setup. Multiplayer-specific ship texture selection remains in the model draw code.

## Render-unification validation
- Passed: `.\android\run-code-quality.ps1 -Fix -Paths @('d1\main\render.c','d2\main\render.c','d1\arch\ogl\ogl.c','d2\arch\ogl\ogl.c','d1\xmodel\xmodel.cpp','d2\xmodel\xmodel.cpp','d1\main\endlevel.c','d1\main\menu.c','d2\main\menu.c','android\app\src\main\java\com\dxxredux\app\GraphicsSettingsPage.kt','android\ai tool plans\crash, logging, diagnostics\coop_level7_blue_texture_log_review_20260706.md')`
- Passed: `.\android\gradlew.bat -p android :app:assembleDebug`
- Build note: the native build still reports existing packed-member warnings in D1/D2 `xmodel.cpp` around the model view-matrix upload helper, outside the modified `xmodel_show()` branch.
