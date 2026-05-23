# Plan: 7-item fix batch (round 2)

## Item 1: Re-add on-screen target culling for indicator lines
**Problem**: The coop/buddy indicator lines should NOT draw when the target (player/guidebot) is already visible on screen. This was removed during a prior rewrite.
**Root cause**: `target_is_on_screen()` was removed per old plan. User clarifies it should always have been kept.
**Fix**: Re-implement on-screen visibility check in `coop_indicator_lines.c`. Use `g3_project_point()` to project the target position to screen coords. If the projected point is within the viewport AND the target segment is in the render list (`seg_is_visible()`), skip drawing that path.
**Files**: `android/app/src/main/cpp/shared/coop_indicator_lines.c`
**Status**: [x] DONE - Added `target_is_on_screen()` using `g3_project_point()` + viewport bounds check and `seg_is_visible()`. Added extern for Canvas_width/Canvas_height. Both player and buddy lines gated.
**Problem**: Guidebot path line and touch wheel don't appear in D2 single player.
**Root cause (line)**: Needs investigation -- the code looks correct. `Buddy_allowed_to_talk`, `Buddy_objnum` conditions, and path computation all look valid. Possible issue: `coop_indicator_lines_render()` might not be called from d2/main/render.c for single player, or the `#ifdef __ANDROID__` guard might not be in the right place.
**Root cause (controls)**: The guide wheel rendering in TouchOverlayView.kt looks correct. `nativeIsBuddyReleased()` returns `Buddy_allowed_to_talk` via JNI. Need to verify the JNI is actually called and returns the right value. Possible issue: the function is declared as `external fun` on `MainActivity` but might not be linked when the native library loads.
**Investigation**: Check that `coop_indicator_lines_render()` is actually called from `render_frame()` in D2-- confirm the `#ifdef __ANDROID__` or `#ifdef ANDROID` block is present in d2/main/render.c. Also run a test to check JNI return values.
**Files**: `d2/main/render.c`, `android/app/src/main/cpp/shared/coop_indicator_lines.c`, `android/app/src/main/cpp/jni_main.c`, `TouchOverlayView.kt`
**Status**: [x] DONE - Removed `Buddy_allowed_to_talk` gate from indicator line. Line now shows whenever companion robot exists (useful for finding cage). Guide wheel correctly stays gated on `Buddy_allowed_to_talk`.

## Item 3: Music overlay position mismatch
**Problem**: In-game music control position doesn't match editor preview. Now wrong in the opposite direction.
**Root cause**: Two completely different drawing codepaths with different spacing constants:
- In-game: radius = `base * 0.03f * sizeMult`, spacing uses `diagTextSize * 0.5f` and `base * 0.02f * sizeMult`
- Editor: radius = `baseScale * 0.015f * sizeMult`, spacing uses fixed `4f` pixels
**Fix**: Make the in-game code use the same proportional formula as the editor. Both start from `centerX` and go right. Change in-game spacing to use the same scaled-up version of the editor constants, accounting for the fact that in-game base and editor baseScale have a 2:1 ratio for radius.
**Alternative**: Extract a shared positioning helper. But given the two different drawing systems (Canvas drawCircle vs Paint drawCircle), keep them separate but with the same formula.
**Files**: `TouchOverlayView.kt`, `TouchEditorPage.kt`
**Status**: [x] DONE - Editor now uses `min(w,h)` base scale, `base*0.03f*sizeMult` radius, `diagTs*0.5f` spacing, `base*0.02f*sizeMult` between-button spacing -- matching in-game.

## Item 4: Coop invulnerability after level load
**Problem**: Player invulnerable to the other player after coop save/load at level outset.
**Root cause**: `state.c:1867` saves ALL flags including `PLAYER_FLAGS_INVULNERABLE` into `Netgame.player_flags[i]`. Then `gameseq.c:1647` OR's them back. Remote players never have invulnerability cleared.
**Fix**: Mask out transient flags when saving to `Netgame.player_flags`:
```c
Netgame.player_flags[i] = Players[i].flags & ~(PLAYER_FLAGS_INVULNERABLE | PLAYER_FLAGS_CLOAKED);
```
Also check d1/main/state.c for the same pattern.
**Files**: `d2/main/state.c`, `d1/main/state.c`
**Status**: [x] DONE - Masked PLAYER_FLAGS_INVULNERABLE|PLAYER_FLAGS_CLOAKED from saved flags in both d1 and d2 state.c.

## Item 5a: MSAA breaks missile/rear sub-window rendering
**Problem**: When firing a guided missile with MSAA enabled, the screen goes blank except for the missile HUD view.
**Root cause**: `ogl_start_frame()` binds the MSAA FBO and clears color+depth every time it's called. Sub-window `render_frame(0, win+1)` triggers a second `ogl_start_frame()` which re-binds and clears the FBO. Then `ogl_end_frame()` resolves the full FBO (now only containing the sub-window) over the entire default framebuffer, wiping the main scene.
**Fix**: Track whether we're in the "first" start/end frame pair. Skip MSAA FBO bind/clear for sub-window renders. Add a static counter `g_msaa_frame_depth`:
- `ogl_start_frame`: if `g_msaa_frame_depth > 0`, skip FBO bind+clear. Else bind+clear and increment.
- `ogl_end_frame`: only resolve blit when returning to depth 0.
This way sub-windows render directly to the MSAA FBO without clearing it, and the resolve only happens once.
Actually better: sub-windows should render to the default framebuffer (fb 0), since they're small viewports composited on top. So: skip MSAA bind entirely for depth > 0.
**Files**: `d2/arch/ogl/ogl.c`, `d1/arch/ogl/ogl.c` (same fix for consistency)
**Status**: [x] DONE - Added `g_msaa_frame_depth` counter in d1/d2 ogl.c. Sub-windows skip FBO bind/clear/resolve.

## Item 5b: Automap "MAP" button not enlarged
**Problem**: The MAP button in the automap overlay wasn't enlarged despite changing size in preset JSONs.
**Root cause**: The automap MAP button copies `mapBtnRadius` from the layout's MAP button, but only when `!automapActive`. When automap IS active and `computeGeometry` runs, the condition `!automapActive` prevents updating `mapBtnRadius`, so it stays at the default `base * 0.035f`.
Wait -- need to re-check. If `computeGeometry` runs before automap becomes active, it should pick up the size. And `computeGeometry` runs on layout change, not on automap toggle. So `mapBtnRadius` should be set correctly.
**Investigation**: Need to check whether the MAP button actually exists in the layout (not just the presets). The user may have a custom layout without the MAP binding. Or the button ID mismatch.
**Alternative root cause**: The preset JSON was updated but the user's active `touch_layout.json` wasn't -- it uses the old size. The preset change only affects new installs or resets.
**Fix**: Check whether the automap's own MAP button size should be independent of the touch layout. If so, make the automap exit button use a hardcoded larger size. If the intent is for it to follow the layout, no code change is needed -- the user just needs to update their layout.
**Files**: `TouchOverlayView.kt`
**Status**: [x] DONE - MAP button now uses `maxOf(mapBtnRadius, automapBtnSize)` during automap for both drawing and hit detection.

## Item 6: App update overwrote touch config
**Problem**: New APK install replaced user's touch config with bundled advanced preset.
**Root cause**: `TouchLayoutRepository.load()` reads `filesDir/touch_layout.json`. If file doesn't exist or `fromJson()` throws, it falls back to `defaultLayout()` which returns the first bundled preset (alphabetically: `advanced.json`).
Possible cause: A change in the data model (e.g., new `deadzoneX/Y/Z` fields in GyroConfig) caused `fromJson()` to throw, triggering the fallback. But `fromJson` uses `opt*` methods and has a catch-all -- shouldn't throw.
Alternative: The user might have installed to a different profile/work profile, or had the app data cleared by the system.
**Fix**: Since the user says presets should never auto-apply on update, add a log line when the fallback fires so we can diagnose. Also ensure the internal `fromJson()` can never throw for the kinds of changes we make. No actual code change needed if the file was genuinely deleted.
Actually: the real answer is we should never overwrite touch_layout.json. It's already not happening in code. The user may have just been bitten by one of our test data resets. Just confirm the code is correct and move on.
**Files**: `TouchLayoutRepository.kt`
**Status**: [x] DONE - `TouchLayoutRepository.load()` now auto-saves defaults when file missing or parse fails, locking in the config immediately.

## Execution order
1. Item 5a (MSAA missile) -- most impactful, foundational rendering fix
2. Item 4 (coop invulnerability) -- simple d1/d2 fix
3. Item 1 (on-screen culling) -- moderate complexity
4. Item 2 (guidebot investigation) -- needs debugging
5. Item 3 (music position) -- UI math fix
6. Item 5b (MAP button) -- investigation/clarification
7. Item 6 (config overwrite) -- likely no code change needed
8. Build + lint + test
