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

## Post-unification report 2026-07-06
- [x] Record user result that the visual problem is unchanged after honoring `ClassicDepth` in multiplayer.
- [x] Make the next run bypass Android cached merged-wall premerge directly, without relying on a launcher debug preference.
- [x] Add enough route logging to prove whether the focus face uses the bypass.
- [x] Validate and hand off the exact next-run logging instructions.

## Post-unification findings
- The unchanged visual result falsifies the broad `GM_MULTI` render-pass split as the main visible cause for this face, or shows that the tested run did not depend on `ClassicDepth`.
- The strongest remaining targeted suspect is still the Android `merge_cached` / `gpu_cached_single` path for plain transparent overlay walls.
- The existing launcher preference `Force legacy CPU texmerge` should force CPU texmerge via `merged_wall_experiment=10`, but the next run should not depend on toggling that preference correctly.
- Implemented the next diagnostic as a direct Android multiplayer bypass: if `GM_MULTI` is set and the overlay bitmap is normal transparent, D1/D2 now use old CPU texmerge instead of the OGL live/cached merged-wall path. Focus overlays log `[mwall_exp] ... merge_impl=auto_old_texmerge reason=coop_plain_transparent_overlay`.

## Post-unification validation
- Passed: `.\android\run-code-quality.ps1 -Fix -Paths @('d1\main\render.c','d2\main\render.c','android\ai tool plans\crash, logging, diagnostics\coop_level7_blue_texture_log_review_20260706.md')`
- Passed: `git diff --check`
- Passed: `.\android\gradlew.bat -p android :app:assembleDebug`
- Next run needs Graphics and Texture logging enabled. The mwall tap is useful but not required if the same focus overlay logs automatically; use it if the visible wrong wall is easy to aim at.

## Simultaneous correction observation 2026-07-06
- [x] Record user report that textures stayed wrong for several minutes in coop, then flipped correct for both players at once.
- [x] Inspect texture cache, texmerge, and palette invalidation paths that can rebuild visible textures mid-level.
- [x] Add targeted logging around piggy page-out, texmerge flush, and Android cached merged-wall clear/recreate.
- [x] Add a forced native debug-log path so texture tap/probe lines can punch through when the Texture category is off.
- [x] Validate and document next-run logging settings.

## Simultaneous correction findings
- Simultaneous correction on both peers points toward a synchronized game-state event causing cache invalidation/rebuild, not a random per-device GL state leak.
- `piggy_bitmap_page_out_all()` resets `Piggy_bitmap_cache_next`, increments `piggy_page_flushed`, calls `texmerge_flush()`, and pages out piggy bitmaps. A later `PIGGY_PAGE_IN()` rebuilds source bitmap data and merged texmerge entries.
- `texmerge_get_cached_bitmap()` explicitly handles `piggy_page_flushed` by re-reading base and overlay textures before creating a merged bitmap. If the bad wall was using a stale/incorrect merged result, a full piggy page-out can make it correct on the next rebuild.
- Android also has a 32-entry cached merged-wall LRU. Eviction or clear/recreate of that entry can replace a bad cached composite with a good one.
- Palette restore and OGL texture-list resets also clear GL textures and the Android merged-wall cache, but they are less likely during normal flying unless a menu/death/palette restore happened.
- To keep debug logs smaller, the next pass should not require enabling the full Texture category. Forced one-shot/cache-event lines should write through the native logger even when `DLOG_TEXTURE` is disabled.

## Simultaneous correction changes
- Added `debug_log_force()` on the native side plus `DebugLog.logForced()`/`debugLogForcedFromNative()` on the Kotlin side. Forced lines write to the debug log without enabling their category.
- Converted `[mwall_tap_probe]`, `[mwall_snap] request`, and `[mwall_snap_pose]` to forced Texture logs so the tap/probe bundle punches through with Texture logging off.
- Added sparse forced lifecycle lines:
  - `[texcache] event=piggy_page_out_all ...`
  - `[mwall_texmerge] event=flush ...`
  - `[mwall_texmerge] event=create ...` for tracked merged-wall overlays
  - `[mwall_cache] event=clear/create/evict ...` for tracked Android cached merged-wall entries
  - `[texcache] event=ogl_palette_invalidate ...`
- Left high-volume Texture logs, including cache reuse and broad render diagnostics, behind the normal Texture category.

## Simultaneous correction validation
- Passed: `.\android\run-code-quality.ps1 -Fix -Paths @('android\app\src\main\cpp\shared\android_log.c','android\app\src\main\cpp\shared\android_log.h','android\app\src\main\cpp\shared\merged_wall_debug.c','android\app\src\main\java\com\dxxredux\app\DebugLog.kt','android\app\src\main\java\com\dxxredux\app\MainActivity.kt','d1\main\piggy.c','d2\main\piggy.c','d1\main\texmerge.c','d2\main\texmerge.c','d1\arch\ogl\ogl.c','d2\arch\ogl\ogl.c','android\ai tool plans\crash, logging, diagnostics\coop_level7_blue_texture_log_review_20260706.md')`
- Passed: `git diff --check`
- Passed: `.\android\gradlew.bat -p android :app:assembleDebug`
- Next run can leave Texture logging off. Use coop desync logging plus the mwall tap/probe when the wrong texture is visible; the forced tap/cache lines should still appear in the exported debug log.

## Fresh tap log 2026-07-06 17:57
- [x] Review the new host log with the same wrong texture and a mwall tap.
- [x] Check whether the tap resolved to a wall face.
- [x] Improve tap/probe fallback logging so a near miss still reports the tracked/selected face identity.
- [x] Add cache-full trigger logging so `piggy_page_out_all` reports which bitmap request caused it.
- [x] Validate the logging change and hand off next-run instructions.

## Fresh tap log findings
- The run is a fresh D2 coop level 7 host start with build 17260 and categories `Graphics, Texture, Coop Desync`.
- During level load, D2 hit `piggy_page_out_all` three times: once at empty cache, then near capacity twice (`cache_next=2456763` and `2457527` of `2457600`). Each full page-out flushed texmerge entries.
- The coop segment signature stayed stable (`segment_sig=e8f97cd8`) while texture/paging counters alternated, so this still looks like texture/cache lifecycle behavior rather than level segment data mutation.
- The tap request logged pose and timing, but the resolver returned `status=no_crosshair_face` with `tracked=1 selected=1`. The tap therefore did not identify the visible wrong face in this log.
- Because the logger knew one tracked face existed, the next useful improvement is to force-log the tracked/selected face summary on no-hit tap probes instead of only logging counts.

## Fresh tap log changes
- `merged_wall_log_tap_probe()` now keeps the existing `status=no_crosshair_face` line and adds a forced `[mwall_tap_probe] kind=face_candidate` line when a tracked fallback exists. The fallback reports source (`selected` or `nearest`), box, `seg/side/face`, `tmap1/tmap2`, overlay index, route, merge implementation, and reason.
- The no-hit fallback also emits forced `base_fallback`, `overlay_fallback`, and merged-bitmap details for that candidate face, only on the tap/probe path.
- D1/D2 `piggy_bitmap_page_in()` now logs a forced `[texcache] event=pageout_trigger` before cache-full page-outs. The line includes `requested/resolved` bitmap IDs, bitmap name, reason (`rle_pre`, `rle_post`, or `raw_pre`), needed bytes, cache cursor, flags, dimensions, rowsize, and handle.

## Fresh tap log validation
- Passed: `.\android\run-code-quality.ps1 -Fix -Paths @('android\app\src\main\cpp\shared\merged_wall_debug.c','d1\main\piggy.c','d2\main\piggy.c','android\ai tool plans\crash, logging, diagnostics\coop_level7_blue_texture_log_review_20260706.md')`
- Passed: `git diff --check`
- Passed: `.\android\gradlew.bat -p android :app:assembleDebug`

## Tap log 2026-07-06 19:33
- [x] Review the new coop desync log with forced tap/cache diagnostics.
- [x] Identify whether the tap fallback names the visually wrong face.
- [x] Compare page-out trigger bitmaps against coop-only texture pressure.
- [x] Implement a post-level-load multiplayer texmerge cache reset.

## Tap log 2026-07-06 19:33 findings
- The run used build 17262 with only `Coop Desync` enabled; forced Texture diagnostics still punched through.
- The page-out triggers during D2 level 7 load were `gauge06b` (`requested=254`) and `misc068#6` (`requested=1718`), both `reason=rle_post`, not the tapped wall textures.
- The tap fallback identified the same visible wall: `seg=169 side=0 face=0`, `tmap1=73` (`rock198`), `tmap2=0x132`, overlay `306` (`ceil035`).
- The face was already using `route=old_texmerge merge_impl=auto_old_texmerge reason=coop_plain_transparent_overlay`. This rules out the Android cached merged-wall path as the active renderer for this wrong-texture run.
- The remaining fit for "wrong from fresh coop start, then both players flip correct together" is a stale or incorrectly built legacy texmerge cache entry surviving from level-load paging until a later piggy/texmerge flush rebuilds it.

## Tap log 2026-07-06 19:33 changes
- Added an Android-multiplayer-only post-level-load cache reset in D1/D2 `LoadLevel()` after texture paging and OGL texture cache warmup.
- The hook force-logs `[texcache] event=post_level_load_flush ...`, then calls `texmerge_flush()` and `android_merged_wall_cached_texmerge_clear_cache()`.
- This leaves single-player behavior unchanged and should prevent level-load-time merged composites from carrying into gameplay.

## Tap log 2026-07-06 19:33 validation
- Passed: `.\android\run-code-quality.ps1 -Fix -Paths @('d1\main\gameseq.c','d2\main\gameseq.c','android\ai tool plans\crash, logging, diagnostics\coop_level7_blue_texture_log_review_20260706.md')`
- Passed: `.\android\gradlew.bat -p android :app:assembleDebug`

## Ten-target forensics pass
- [x] Instrument source bitmap page-in/page-out generation for the target textures.
- [x] Instrument raw and expanded source bitmap hashes before every target legacy merge.
- [x] Instrument legacy texmerge create/reuse/evict with merged bitmap hashes and slot provenance.
- [x] Instrument GL upload of target/merged bitmaps with CPU hash, handle, and upload state.
- [x] Instrument draw submission of target merged bitmaps with GL binding/state at first few frames and tap.
- [x] Instrument target face route transitions and per-frame first draw after level start.
- [x] Instrument post-level-load cache reset with before/after cache generation details.
- [x] Instrument palette/alpha/mask source decisions for target bitmaps without requiring Texture logging.
- [x] Instrument tap render readback/hash for fallback candidates even when the crosshair misses.
- [x] Instrument global texture signatures around coop lifecycle phases and after multiplayer prep.

## Ten-target forensics changes
- Added compact forced `[mwall_forensic]` records for target bitmaps `rock198`, `ceil035`, and the older `metl154` case.
- Added target source/page-in, legacy texmerge create/reuse/evict/flush, GL upload, draw bind, tap fallback render sample, and D2 coop lifecycle signature logging.
- Legacy texmerge entries now keep Android-only source `tmap1/tmap2` provenance so eviction logs identify the old slot contents.
- Reuse and draw-bind logs are capped per cache generation; tap, create, evict, flush, upload, and lifecycle records still punch through Texture category filtering.

## Ten-target forensics validation
- Passed: `.\android\run-code-quality.ps1 -Fix -Paths @('android\app\src\main\cpp\shared\merged_wall_debug.c','android\app\src\main\cpp\shared\merged_wall_debug.h','d1\arch\ogl\ogl.c','d2\arch\ogl\ogl.c','d1\main\piggy.c','d2\main\piggy.c','d1\main\texmerge.c','d2\main\texmerge.c','d1\main\gameseq.c','d2\main\gameseq.c','android\ai tool plans\crash, logging, diagnostics\coop_level7_blue_texture_log_review_20260706.md')`
- Passed: `git diff --check`
- Passed after fixing one missing local declaration caught by the first build attempt: `.\android\gradlew.bat -p android :app:assembleDebug`
