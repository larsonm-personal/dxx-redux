## Overlay, save preview, and supertransparency research

### Goals
- [x] Confirm how net events overlays should be disabled in single-player and where startup defaults are set
- [x] Trace why admin tray quick save/load closes the settings tray instead of opening save/load menus
- [x] Trace how save preview images are written and read, and identify why load previews are garbled
- [x] Research supertransparency handling for METL154 and related high-res replacement textures, especially one-sided failures in D2 level 1
- [x] Produce an implementation plan with likely touch points, risks, and verification steps

### Notes
- User asked for research first, with extra focus on the transparency problem
- Prefer a unified import-time or load-time supertransparency rule over one-off texture fixes

### Findings

#### 1. Net events tray state in single-player
- MainActivity already initializes `netEventsManualToggle` to false and resets it when the game stops
- The auto-show path is multiplayer-driven (`isMultiplayerGame`, host-selecting, or pending matchmaking launch info), so single-player does not need the overlay
- The current gap is UI policy, not state storage: the admin tray has toggle-state and label providers, but no enabled-state provider, so Net Events can still be manually toggled in single-player
- Likely touch points: `android/app/src/main/java/com/dxxredux/app/MainActivity.kt`, `android/app/src/main/java/com/dxxredux/app/TouchOverlayView.kt`

#### 2. Quick save/load from the tray
- Both D1 and D2 pause handlers only accept Escape, F1, and Pause while the pause window is front
- The current Android tray path still depends on leaving pause and then injecting Alt+F2 or Alt+F3 from Kotlin
- If the pause handoff is not perfect, the net effect is that the settings tray closes without ever opening the save/load menu
- The robust fix is engine-side: expose Android-safe native entry points that invoke the same save/load code paths the in-game Escape menu uses, instead of simulating keyboard chords from Kotlin
- Likely touch points: `android/app/src/main/java/com/dxxredux/app/MainActivity.kt`, `android/app/src/main/cpp/android_input.c`, `d1/main/gamecntl.c`, `d2/main/gamecntl.c`, and possibly `d1/main/state.c` / `d2/main/state.c`

#### 3. Save preview thumbnails
- Both games save a raw 100x50 indexed thumbnail produced from `glReadPixels()` and palette quantization
- D2 save files also write a 256-color palette next to the thumbnail and remap the preview on load, which should keep previews stable across palette changes
- D1 save files do not store a thumbnail palette at all; they just write raw indexed pixels and later display them as-is
- D2 bitmap allocation uses `bm_rowsize == bm_w`, so the thumbnail read itself is not obviously corrupt from row-stride mismatch
- That makes a missing D2 palette application less likely than an OGL preview draw-path issue after remap, while D1 remains the main format-risk case for future work
- Likely touch points: `d1/main/state.c`, `d2/main/state.c`

#### 4. METL154 and supertransparency
- The base engine meaning of supertransparency is orientation-aware texmerge: `merge_textures_super_xparent()` rotates the overlay per side orientation and converts palette index 254 into transparency while preserving the bottom texture
- The Android hires path is different: it uses `OGL_MERGE` plus one pre-generated mask PNG per bitmap name, loaded by `ogl_load_dxa_mask()` and sampled in the merge shader
- Older repo notes flagged `metl154` as skipped in some packs when ETC2 was RGB-only, but temp conversion logs now show the 256px and 512px `metl154.ktx2` being generated as RGBA, so the current 512px issue is probably not the old no-alpha skip
- The likely class of failure is now mismatch between the generic mask-based hires pipeline and orientation-specific overlay semantics, especially when the same super-transparent overlay bitmap is used with multiple `tmap2` orientations in D2 level 1
- The strongest evidence points to a gap in the generalized mask-orientation pipeline, not a METL154-only special case
- Likely touch points: `game_data/mods/convert_d2xxl_textures.ps1`, `d2/arch/ogl/ogl.c`, `d2/arch/ogl/oglprog.c`, `d2/main/render.c`, `d2/main/texmerge.c`, and the D1 equivalents for shared fixes

### Implementation plan

#### Phase 1. Disable Net Events in single-player
- [x] Add an admin tray enabled-state provider so specific actions can render disabled and ignore presses
- [x] Feed Net Events enabled-state from multiplayer runtime state in `MainActivity.kt`
- [x] Force-hide the overlay and clear `netEventsManualToggle` on any single-player launch or state transition, not just full game stop
- [x] Add JVM tests covering disabled tray state and single-player reset behavior

#### Phase 2. Add direct save/load menu entry points
- Add Android JNI helpers for "open save menu if safe" and "open load menu if safe"
- Implement those helpers in D1 and D2 by routing directly to the same engine save/load functions used by the in-game Escape menu, while explicitly handling the pause window front case
- Replace Kotlin Alt+F2 / Alt+F3 injection for admin tray actions with the new native calls
- Verify with a targeted emulator/manual test that the tray can open save/load menus from paused gameplay without collapsing back to gameplay

#### Phase 3. Fix save preview rendering
- Reproduce in D1 and D2 separately so the bug is not treated as one problem if it is actually format-specific
- Do not change save formats unless a future investigation proves there is no compatible alternative
- For D2 first, inspect the post-remap preview draw path (`state_callback`, scaling, and OGL blit) before touching thumbnail serialization
- If the garble is D1-only, prefer a compatible runtime fix or fallback draw-path investigation before considering any save-format change
- If D2 is also affected, inspect the saved palette/readback path and add temporary diagnostics around thumbnail bytes and palette remap
- Add a focused regression check around save preview generation if practical

#### Phase 4. Build a unified supertransparency validation pass
- Audit the converter output for every `BM_FLAG_SUPER_TRANSPARENT` texture: KTX2 format, mask existence, mask dimensions, and naming consistency
- Confirm at runtime whether `metl154_mask.png` is present in the 512px DXA and whether `ogl_load_dxa_mask()` loads it on both failing and working faces
- If the mask exists but one orientation still fails, adjust the hires merge pipeline so mask sampling matches orientation semantics generically instead of patching METL154 by name
- Prefer a converter or load-path rule that applies to all super-transparent textures, with validation against the actual bitmap-flag set, rather than accumulating texture-specific exceptions
- Verify against D2 level 1 METL154 plus a representative set of doors and other super-transparent overlays

### Verification ideas
- JVM tests for tray enabled-state and single-player Net Events reset
- Emulator/manual test for direct save/load menu opens from the admin tray
- Save preview reproduction in both D1 and D2 using known fresh save files
- Runtime texture logging for `metl154` KTX2 format, mask presence, and `tmap2` orientation on D2 level 1 faces

### Phase 1 verification
- `android\run-code-quality.ps1 --fix` passed after the tray and activity changes
- `gradlew.bat -p android :app:testDebugUnitTest --tests com.dxxredux.app.AdminTrayUiTest --tests com.dxxredux.app.OverlayVisibilityPolicyTest --rerun-tasks --console=plain` passed
- `gradlew.bat -p android :app:assembleDebug --console=plain` passed