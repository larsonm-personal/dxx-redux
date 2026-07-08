# Coop texture cleanup retrospective

## Goal
- Confirm the door26 coop palette issue is fixed in the newest logs
- Find the previous texture cleanup/retrospective docs and mirror the useful structure
- Identify exploratory code added during this investigation that should be removed or generalized
- Study how coop and pvp rendering should differ, and propose a cleaner stock-texture enforcement path for pvp

## Plan
- [done] Read instructions, newest fixed log, previous cleanup docs, and current git state
- [done] Locate the earliest relevant fix/logging commit and mark suspect code paths
- [done] Audit coop and pvp texture-selection differences against the desired rendering policy
- [done] Produce a cleanup study with retained logging, removable exploration code, and pvp host/client UX notes

## Fixed-log conclusion
- Newest attached log: build 17331, built 2026-07-07 16:22 PST.
- The D2 level load still changes palette/pig state during coop setup:
  - `after-load-level`: `palette=WATER.256`, `last_palette=default.256`, `pig=groupa.pig`
  - `after-bitmap-replacements`: `last_palette=default.256`, `last_pig_palette=WATER.256`, `pig=water.pig`
  - `after-multi-prep`: `last_palette=WATER.256`, `last_pig_palette=WATER.256`, `pig=water.pig`
- The fix is the late Android OGL palette texture invalidation after the final `load_palette(Current_level_palette,0,1); gr_palette_load(gr_palette);` in D2 multiplayer level prep.
- The tap probe now resolves the real face and the texture/GPU readback hashes agree:
  - `geometry_seg=170 geometry_side=0 geometry_tmap1=608`
  - `name=door26#0`
  - `expected_rgba=0x5a3231e9 gpu_rgba=0x5a3231e9 mismatches=0`
- So this was not a level-state texture assignment bug and not a "no face under tap" geometry bug. The texture index was right; the already-uploaded palette-indexed GL texture was stale.

## Prior cleanup model to emulate
Main prior doc:
- `android/ai tool plans/overlay, menu, etc/metl154_postfix_cleanup_and_debug_harness.md`

Useful follow-on docs:
- `android/ai tool plans/overlay, menu, etc/metl154_tranche1_rename_and_mode_cleanup.md`
- `android/ai tool plans/overlay, menu, etc/metl154_tranche2_hardcoded_diagnostic_removal.md`
- `android/ai tool plans/overlay, menu, etc/metl154_tranche3_shared_snapshot_and_regression.md`
- `android/ai tool plans/overlay, menu, etc/metl154_tranche4_shader_and_gl_state_audit.md`
- `android/ai tool plans/code management/cleanup_metl154_rename_finalize.md`

Patterns to copy:
- Protect the one-line behavioral fix first; do cleanup in reviewable tranches.
- Separate "diagnostics worth keeping" from "investigation scaffolding".
- Remove hardcoded level/segment/texture tables and replace them with crosshair/tap-triggered generic capture.
- Keep the debug harness if it answers future classes of bugs, but rename logs and APIs away from the texture that originally motivated them.
- Validate each tranche independently, because texture changes are easy to misattribute.

One warning from current code: some previous-pass naming debt still exists (`metl154` log tags, helper names, and target lists). The new cleanup should follow the docs' intent more completely than the code currently does.

## Suspect boundary
Earliest relevant committed attempt in this branch:
- `1248735b unify single+multiplayer graphics`

Committed attempts after that:
- `258e474e better logging`
- `bc3ee2d3 more coop texture logging`
- `ee22e1e2 better coop texture logging`
- `c76f927e more`
- `e682effe more coop texture debugging`
- `b73dae17 texture debug`

Diff scope from `1248735b^..HEAD`, before the current uncommitted fix:
- 17 files changed, 3145 insertions, 404 deletions.
- Most of the growth is `android/app/src/main/cpp/shared/merged_wall_debug.c` with 3163 insertions.

Current uncommitted code delta is small:
- `d1/main/gameseq.c`: adds an Android OGL palette invalidation after `gr_palette_load(gr_palette)`.
- `d2/main/gameseq.c`: adds the same after D2's final multiplayer palette load. This D2 call is the fix evidenced by build 17331.

## Rendering policy
Desired rule:
- Single-player and coop should use the same rendering policy.
- PVP should enforce stock competitive visuals for fairness.
- Mission-required data should still load in PVP. Do not block a custom mission's own required HOG/RDL/RL2/POG/PIG/HAM/HXM content. Block optional visual replacement overlays such as high-res PNG/TGA/KTX2 texture packs and replacement models.

Current scattered policy:
- `d1/arch/ogl/ogl.c` and `d2/arch/ogl/ogl.c`: `ogl_allow_png()` special-cases Android coop, then otherwise allows replacements only outside multiplayer or when `Netgame.AllowCustomModelsTextures`.
- `d1/main/polyobj.c` and `d2/main/polyobj.c`: duplicate the same coop special-case for replacement models via `xmodel_show_if_loaded`.
- `Netgame.AllowCustomModelsTextures` can still permit custom visuals in non-coop multiplayer. That conflicts with "PVP should be stock visuals" on Android.

Cleanup recommendation:
- Add a single Android visual-replacement policy helper used by both D1 and D2 OGL/model paths.
- Proposed semantics on Android:
  - single-player: allow visual replacements
  - coop: allow visual replacements
  - non-coop multiplayer: deny visual replacements, regardless of `Netgame.AllowCustomModelsTextures`
- Keep `Netgame.AllowCustomModelsTextures` for desktop/protocol compatibility if needed, but Android PVP should ignore or forcibly clear it for optional visual replacements.
- Replace duplicated conditionals in `ogl_allow_png()` and `draw_polygon_model()` with the shared helper.

## Keep, generalize, delete
Keep as behavior:
- D2 late `ogl_invalidate_game_palette_textures()` after the final multiplayer palette reload. This is the actual cure.
- The D1 invalidation after `gr_palette_load(gr_palette)` is defensible symmetry, but it should be reviewed as "same hazard prevention" rather than "proven by this bug".
- The Android coop parity change from `1248735b` is directionally right: coop should not have special texture/model restrictions that single-player lacks.

Keep as generic diagnostics:
- Level texture state snapshots (`[leveltex]`) with palette names, pig name, texture-table signature, and segment texture signature. These proved the palette handoff.
- Palette invalidation logs (`[texcache] event=ogl_palette_invalidate`) because they prove when stale GL textures are discarded.
- Tap probe face resolution via geometry ray/FVI, plus the ranked all-face fallback. This should remain a generic "what face am I actually seeing" tool.
- Tap probe palette and GPU readback fields (`palette_state`, source hash, expected RGBA, GPU RGBA, mismatch count), but only behind explicit texture debug/tap capture.

Generalize or trim:
- Piggy page-in/page-out, texmerge flush, upload, and draw forensics should stay only when the texture debug category or an explicit probe is active. They are useful but too noisy as broad coop logging.
- `mwall_tap_probe` is generic enough in concept, but names should move toward "texture probe" or "face texture probe" if we touch nearby APIs.
- FNV-1a constants should be named/commented (`MERGED_WALL_FNV1A_OFFSET`) rather than repeated as magic `2166136261u`.

Delete or rename:
- Any hardcoded logging target names such as `metl154`, `ceil035`, `rock198` in `merged_wall_is_logging_target_name()`. Use the existing Android debug texture target (`crosshair`, named target, or none) instead.
- `metl154` log tags and helper names in shared wall diagnostics and OGL paths. Replace with neutral tags such as `[mwall_clip]`, `[mwall_submit]`, `[mwall_src]`, or `[texprobe_*]`.
- The special clip path named `ogl_clip_and_draw_metl154_single` should be renamed or removed if it only exists to support a past target-specific experiment.
- Hardcoded `metl154cache` pre/post load logs in OGL cache loops should be removed or converted to named-target debug logging.
- Continuous draw-readbacks for forensic targets should be removed unless they are triggered by a tap/probe request. GPU readback is valuable but should not ride normal rendering.

## PVP stock-visual notice
Detection should live in launcher/mod code, not inside render code:
- `GameFileFormats.isTextureReplacement()` already classifies `.png`, `.tga`, `.ktx2`, and `.dtx`.
- `DxaTextureScanner.scan()` already counts texture replacements inside DXA archives.
- `ModManager` already knows enabled mods, active path lines, mission zips, and mod categories.

Recommended helper:
- Add `ModManager.enabledVisualReplacementSummary(game, includeD1MissionZipsForD2)` returning:
  - `hasOmittedVisuals`
  - `omittedModCount`
  - `omittedTextureCount`
  - a short list of display names/examples for UI
- It should conservatively detect enabled mods that contain optional texture/model replacement assets. Keep mission-required archives enabled; the engine policy will omit only visual replacements at render/load time.

Host/client UX:
- Online host create flow: when mode is non-coop and enabled visual replacements are detected, include game info fields such as:
  - `stock_visuals_enforced=true`
  - `omitted_visual_mod_count=N`
  - `omitted_visual_mods=["..."]`
- Online lobby screen: show the notice to host and clients when those fields are present.
- LAN announce/start/join-ack should carry equivalent fields, following the existing `restrict_noncoop_fov_to_base` pattern.
- LAN host view and discovered lobby cards should show the same notice before launch/join.
- Suggested wording: "PVP uses stock visual textures and models. Enabled visual packs will be ignored for this game."

## Proposed implementation tranches
1. Lock in the fix:
   - Keep the D2 late palette invalidation.
   - Decide whether to keep D1 symmetric invalidation.
   - Add a small comment explaining "palette-indexed GL textures must be rebuilt after final level palette load".
2. Centralize visual replacement policy:
   - Add a shared Android helper used by D1 and D2.
   - Replace `ogl_allow_png()` and `draw_polygon_model()` custom conditionals.
   - Force Android non-coop multiplayer to stock optional visual replacements.
3. Generalize retained logging:
   - Keep `[leveltex]`, `[texcache]`, palette hashes, face probe, and GPU readback in a non-level-specific form.
   - Gate noisy logs behind texture debug or explicit probe request.
4. Remove investigation residue:
   - Delete hardcoded target names.
   - Rename/remove `metl154` symbols and log tags.
   - Remove always-on forensic draw/readback hooks that are not tied to a probe.
5. Add PVP notice plumbing:
   - Add mod visual-replacement summary helper.
   - Add online `game_info` fields.
   - Add LAN announce/start/join-ack fields.
   - Show notice in host create flow, lobby screen, LAN host view, and discovered lobby cards.
6. Validate:
   - Coop with a texture pack should match single-player visual replacement behavior.
   - PVP with a texture pack enabled should launch and show stock visuals.
   - PVP custom missions that require mission texture data should still load.
   - Level 7 D2 coop door26 should stay correct after the cleanup.

## Implementation status
- [done] Tranche 1/2: protected the palette invalidation fix and centralized Android visual replacement policy.
- [partial] Tranche 3/4: removed hardcoded logging target names, converted target cache/source logs to generic named-target logging, and removed the `metl154` symbol/tag debt.
- [done] Tranche 5: surfaced PVP stock-visual notices in online and LAN host/lobby flows.
- [done] Tranche 6: ran scoped formatting/static checks and Android Kotlin/native build verification.

## Implementation notes
- Added `android_visual_policy.h`; Android single-player and coop allow optional texture/model replacements, while Android non-coop multiplayer uses stock optional visuals regardless of `Netgame.AllowCustomModelsTextures`.
- Added `VisualReplacementPolicy.kt` and `ModManager.enabledVisualReplacementSummary()` so host/client lobby UI can warn only when enabled visual replacement packs will actually be omitted.
- Online lobbies publish `stock_visuals_enforced`, `omitted_visual_mod_count`, `omitted_visual_texture_count`, and `omitted_visual_mods` in `game_info`.
- LAN announce, join-ack, and start packets carry the same fields, and host/join/discovered lobby views render the same notice.
- Validation passed:
  - `.\android\run-code-quality.ps1 -Fix -Paths <touched files>`
  - `.\gradlew.bat :app:compileDebugKotlin :app:externalNativeBuildDebug`

## Continuation tranche
- [done] Renamed remaining `metl154` diagnostic symbols and log tags in the Android merged-wall path to neutral merged-wall names.
- [done] Confirmed no fixed texture names or old `metl154` tags remain in `android/app/src/main/cpp/shared`, `d1/arch/ogl`, or `d2/arch/ogl`.
- [done] Ran scoped formatting and Android native build verification.

Continuation validation:
- `.\android\run-code-quality.ps1 -Fix -Paths android\app\src\main\cpp\shared\merged_wall_debug.c d1\arch\ogl\ogl.c d2\arch\ogl\ogl.c`
- `.\gradlew.bat :app:externalNativeBuildDebug`

## Reduction tranche
- [done] Deleted the standalone merged-wall forensic logging stream and draw/upload/page-in/legacy-texmerge call sites.
- [done] Replaced the old single-draw forensic hook with `android_merged_wall_probe_record_draw_face()` so tap probes still collect drawn-face candidates.
- [done] Removed dead-end render overrides: merged-wall experiment mode, force-legacy/auto-old-texmerge routing, force-two-pass debug routing, and the matching JNI/Kotlin controls.
- [done] Kept tap-triggered generic face, texture, palette, merge-reference, and GPU readback diagnostics.
- [done] Measured line-count reduction and validated.

Reduction notes:
- `merged_wall_debug.c` went from 8075 lines before this tranche to 7584 lines after cleanup.
- Current whole working diff is net negative after this pass: 538 insertions, 1292 deletions.
- Active source scan is clean for removed investigation names: `forensic`, `mwall_single_draw`, `merged_wall_experiment`, `force_two_pass`, `android_oldmerge`, `auto_old_texmerge`, and the old hardcoded texture tags.
- Retained diagnostics are still generic and are gated by texture debug target or explicit snapshot/probe request.

Reduction validation:
- `.\android\run-code-quality.ps1 -Fix -Paths <touched native, JNI, Kotlin files>`
- `.\gradlew.bat :app:externalNativeBuildDebug`
- `.\gradlew.bat :app:compileDebugKotlin`
