# UI Cleanup Batch - Work Plan

## Status: ALL ITEMS IMPLEMENTED

Build: Android APK assembleDebug - PASS
Linters: All checks passed (clang-format, ktlint, PSScriptAnalyzer, shellcheck, shfmt)

## 1. App Storage Files: Sort by Name / Size buttons
**File:** `android/app/src/main/java/com/dxxredux/app/AdvancedSettingsPage.kt` (lines ~515-560)
**Current:** Files are always sorted alphabetically via `.sortedBy { it.first }`.
**Change:** Add a `sortBySize` state toggle. Add two small buttons ("Name" / "Size") above the LazyColumn. When "Size" is selected, sort descending by `it.second`. When "Name" is selected, sort ascending by `it.first`.
**Effort:** Small.

## 2. Graphics Page: Rename section and add spacing
**File:** `android/app/src/main/java/com/dxxredux/app/GraphicsSettingsPage.kt` (lines ~291-333, `SelectiveFilterSection`)
**Changes:**
- Rename `"Filter by Context"` to `"Enable filters for..."` (line ~297)
- Increase spacing between the two switch rows. Currently each has `padding(vertical = 4.dp)`. Increase to `padding(vertical = 5.dp)` or add a `Spacer(modifier = Modifier.height(4.dp))` between them to create ~1.2x more gap
- Remove the helper text `"When off, these use nearest-neighbor (pixelated) regardless of texture filter above"` (line ~330)
**Effort:** Small.

## 3. Reticle respects wrong filter setting
**Files:** `d2/main/gauges.c`, `d1/main/gauges.c`
**Root cause:** The reticle is drawn inside `render_gauges()` which runs in HUD context (`g_ogl_render_context = 2`). HUD context uses `HudTexFilt` (default ON). The launcher labels MenuTexFilt as covering "reticle", but the code uses HudTexFilt for it.
**Fix:** Temporarily set `g_ogl_render_context = 0` around the `show_reticle()` calls in gauges.c so that the reticle texture filtering is controlled by `MenuTexFilt` instead of `HudTexFilt`. This matches the launcher label. Must do this in both d1 and d2.
**Locations:**
- d2/main/gauges.c: lines ~4567, ~4657, ~4887 (three `show_reticle` calls)
- d1/main/gauges.c: equivalent locations
**Effort:** Small.

## 4. Texture filtering/aniso in-game cycling corruption (BUG)
**Files:** `d2/arch/ogl/ogl.c` (and d1 counterpart)
**Root cause analysis -- TWO likely contributing issues:**

### 4a. Race condition: non-volatile value + volatile flag
`g_texfilt_level` is `int` (non-volatile), while `g_texfilt_pending_apply` is `volatile int`. The JNI thread writes the value then the flag; the GL thread checks the flag then reads the value. On ARM, `volatile` does NOT imply a memory barrier for other variables. The compiler and/or CPU can reorder the non-volatile write *after* the volatile flag write, causing the GL thread to read a stale `g_texfilt_level`.

Same issue exists for `ogl_aniso_level` / `GameCfg.AnisoLevel` which are non-volatile but signaled by `volatile g_aniso_pending_apply`.

**Fix:** Add `__atomic_thread_fence(__ATOMIC_RELEASE)` after writing the value and before setting the flag on the JNI side, and `__atomic_thread_fence(__ATOMIC_ACQUIRE)` after reading the flag on the GL side. Or simpler: make `g_texfilt_level` volatile too, and use `__sync_synchronize()` between the value write and flag write.

### 4b. Texture delete-and-recreate causes driver-level corruption
On mobile GPUs (Adreno, Mali), `glDeleteTextures` + lazy `glGenTextures` with recycled IDs can cause stale driver-internal state (cached sampler parameters, bound texture slots). This is a known class of mobile GPU driver bug.

**Fix:** Instead of deleting textures and relying on lazy reload, update filter parameters in-place:
- For TexFilt changes: iterate all loaded textures, regenerate mipmaps if upgrading (`glGenerateMipmap`), change filter parameters in-place (`glTexParameteri`). If downgrading to nearest, just set GL_NEAREST -- existing mipmaps are harmless.
- For aniso changes: already mostly done via `ogl_apply_anisotropy_all()`, but the unnecessary flush can be removed for the non-mipmap case.
- This avoids the delete/recreate cycle entirely.

### 4c. Post-corruption menu text filtering
User reports: "After this happens, menu text becomes filtered too even when it's set to not be". This is consistent with the race condition: if `g_texfilt_level` reads as stale/wrong (e.g., old non-zero value when user set it to 0), `GameCfg.TexFilt` ends up wrong, and the `if (GameCfg.TexFilt > 0)` check in `ogl_bindbmtex` either does or doesn't apply the selective filtering incorrectly.

**Effort:** Medium. Needs careful testing (emulator + device).
**Testing approach:** Log `g_texfilt_level` and `GameCfg.TexFilt` at the point of application and after each frame. Cycle rapidly and check for mismatches. A memory fence fix alone should help narrow down whether the corruption is race-only or also driver-related.

## 5. Weapon autoselect: text spanning two lines
**File:** `android/app/src/main/java/com/dxxredux/app/AutoselectEditorPage.kt` (~line 555)
**Root cause:** Item height is fixed at `40.dp` (line ~362). "--- Never Autoselect below ---" wraps in portrait. Text uses `fontSize = 13.sp` with no `lineHeight` control.
**Fix:** Add `lineHeight = 14.sp` (or similar) to the Text composable for separator items, so wrapped text stays within the 40dp item height. Alternatively, reduce the separator text font size to 11.sp for separators, or shorten the text.
**Effort:** Small.

## 6. Weapon autoselect: landscape layout for instructions
**File:** `android/app/src/main/java/com/dxxredux/app/AutoselectEditorPage.kt` (~lines 164, 239-243)
**Current:** "Weapon Autoselect" is in TopAppBar. "Long press + drag..." text is below the D1/D2 buttons.
**Change:** In landscape, move the instruction text to be beside the title in the TopAppBar (e.g., use a Row in the title slot). Detect landscape via `LocalConfiguration.current.orientation`.
**Effort:** Small-medium.

## 7. Weapon autoselect: reduce space above D1/D2 buttons
**File:** `android/app/src/main/java/com/dxxredux/app/AutoselectEditorPage.kt` (~line 182)
**Current:** Game selector row has `padding(vertical = 8.dp)`, plus the Column itself has implicit spacing.
**Fix:** Reduce to `padding(vertical = 2.dp)` or `padding(top = 0.dp, bottom = 4.dp)`.
**Effort:** Small.

## 8. D-pad navigation in launcher menus
**File:** `android/app/src/main/java/com/dxxredux/app/SetupActivity.kt`, various page composables
**Current state:**
- `dispatchKeyEvent()` correctly maps A -> DPAD_CENTER, B -> Back, and passes D-pad through to Compose
- D-pad events DO flow to Compose's focus system
- Material3 `Button`, `OutlinedButton`, etc. are inherently focusable
- `DpadFocusUtils.kt` provides `tvFocusable()` modifier but it's not widely used

**Root cause:** Compose's default focus traversal works, but there's no initial focus target. When the page loads, nothing is focused. D-pad navigation only starts working AFTER something receives focus (e.g., tapping A which sends DPAD_CENTER, but with nothing focused, it does nothing meaningful).

**Fix:**
1. On each page's top-level composable, use `LaunchedEffect(Unit) { focusRequester.requestFocus() }` to request focus on the first interactive element when the page appears
2. Add `focusRequester` modifiers to key interactive elements (the first button/switch on each page)
3. This gives D-pad a starting point for traversal
4. Custom non-Material3 interactive elements need `.focusable()` (or `.tvFocusable()` for visual highlight)

**Effort:** Medium. Each launcher page needs a FocusRequester on its first element.

## Proposed work order
1. Items 1, 2, 5, 7 (small, independent Kotlin UI tweaks)
2. Item 3 (reticle context fix, d1+d2 C changes)
3. Item 6 (landscape layout, slightly more complex Compose)
4. Item 4 (texture corruption -- needs careful investigation and testing)
5. Item 8 (D-pad focus -- needs per-page changes and testing)

Each phase should include a build + test pass.
