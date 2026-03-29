# Plan: Button-Based Launcher Automation

## TL;DR

Replace direct `enter_game` / `SETUP_COMMAND launch` automation with real button taps via MotionEvent injection. This catches UI regressions (buttons not rendering, disabled when they shouldn't be, wrong text) and more closely mirrors user behavior. Applies to all 16 json5 test scripts, not just the 4 with launcher phases.

---

## Phase 1: Button Discovery Infrastructure [DONE]

**Goal:** Discover visible Compose button coordinates + enabled state automatically so automation can find and tap them.

**Approach:** Uses Compose's AccessibilityNodeProvider to walk the semantics tree (IDs -1..16383). No per-button annotation needed -- every clickable Compose element is discovered automatically. Text is matched to clickable nodes via spatial containment (Rect.contains).

**Files modified:**
- `android/app/src/main/java/com/dxxredux/app/SetupActivity.kt`

**What was done:**

1.1. Added `ButtonInfo` data class (text, enabled, centerX, centerY, width, height)

1.2. Added `collectAccessibleButtons()` -- walks AccessibilityNodeProvider, collects text nodes and clickable nodes separately, matches by spatial containment. Scans fixed range -1..16383 (needed because game data presence increases semantics node count).

1.3. Added `findComposeView()` -- finds the Compose view in the view tree by checking for accessibilityNodeProvider

1.4. Added `findButtonByText(text)` -- exact match first, then substring fallback (prevents "Descent 1" matching "Launch Descent 1")

1.5. Added `injectTapAt(screenX, screenY)` -- MotionEvent injection with ACTION_DOWN/UP and 50ms delay

1.6. Added `scrollDown()` -- swipe gesture from 75% to 25% screen height with 30ms delays between events, used by tap_button when button is off-screen

1.7. Extended `writeIntrospectJson()` to emit `"buttons"` JSONArray from collectAccessibleButtons()

1.8. Added pending-launch support in onLaunchGame: checks `launcherExecutor?.consumePendingLaunch()`, routes through launchGameForAutomation if non-null

---

## Phase 2: tap_button and assert_button Actions [DONE]

**Goal:** New `tap_button` and `assert_button` actions in LauncherScriptExecutor that find buttons by text, validate state, and inject real MotionEvents.

**Files modified:**
- `android/app/src/main/java/com/dxxredux/app/LauncherScriptExecutor.kt`

**What was done:**

2.1. Changed constructor from `Context` to `SetupActivity` (with `private val context: Context get() = activity`)

2.2. Added `PendingGameLaunch` data class and `consumePendingLaunch()` method

2.3. Added `tap_button` handler: polls for button with timeout (default 10s), auto-scrolls if not found (up to 5 scrollDown() attempts with 400ms delay), validates enabled, sets pending launch if `launches_game: true`, injects tap, suspends if launching game. On failure: dumps all available button texts.

2.4. Added `assert_button` handler: validates button existence + optional `enabled` state check. On failure: dumps available buttons.

---

## Phase 3: Migrate 4 Existing Launcher Scripts [DONE]

**Goal:** Replace `enter_game` with `tap_button` in scripts that already have launcher phases.

**Files modified:**
- `android/game_scripts/test_gog_installer_redbook_unified.json5` -- assert_button enabled:true + tap_button "Launch Descent 2"
- `android/game_scripts/test_resolution_unified.json5` -- 2x tap_button "Launch Descent 2"
- `android/game_scripts/test_controller_compare_unified.json5` -- added GAME_NUM var, chip tap + launch tap
- `android/game_scripts/test_autoselect_crash_unified.json5` -- 2x tap_button "Launch Descent 2"

No remaining `enter_game` in any unified scripts.

---

## Phase 4: Migrate 10 Game-Only Scripts [DONE]

**Goal:** Convert standalone game-only scripts to start from SetupActivity with button taps.

**Files modified:** All 10 standalone game-only json5 scripts in `android/game_scripts/`

**What was done:**

4.1. Added `enter_launcher` + chip selection + `tap_button launches_game:true` preamble to all 10 standalone scripts

4.2. Game selection patterns:
- D2-only with both games data (test_saf_basic, test_keyboard_manual): chip tap "Descent 2" + "Launch Descent 2"
- D2-only with d2-only data (test_fire_primary): just "Launch Descent 2" (no chip needed)
- Dual-game scripts (7 scripts): added GAME_NUM var mapping, chip tap "Descent ${GAME_NUM}" + "Launch Descent ${GAME_NUM}"

4.3. Skipped 2 non-standalone scripts (test_extract_regression_template, test_lan_mp) -- launched by parent scripts

4.4. Also added test_button_discovery.json5 smoke test (assert_button + tap_button verification)

---

## Phase 5: Verification [DONE]

5.1. Build APK and deploy -- done (assembleDebug)
5.2. test_button_discovery.json5 -- PASS (7 steps)
5.3. test_launch_to_automap.json5 d2 -- PASS (31 steps, 13.1s) -- full end-to-end with auto-scroll
5.4. Code quality linters -- ALL PASS (clang-format 67 files, ktlint 66 files, PSScriptAnalyzer 42 files, shellcheck 27 files, shfmt 27 files)

**Bugs found and fixed during verification:**
- Text in child nodes: Compose puts text in child semantics nodes, not merged into parent clickable. Fix: spatial matching
- Scan range (4095 too small): game data increases semantics node count. Fix: extended to -1..16383
- ANR with adaptive gap scan: blocked main thread too long. Fix: fixed range instead of adaptive
- Off-screen buttons: scroll container clips accessibility nodes. Fix: auto-scroll in tap_button (up to 5 swipe attempts)
- findButtonByText substring ambiguity: "Descent 1" could match "Launch Descent 1". Fix: exact match first

---

## Phase 6 (Future): In-Game Overlay Button Taps

**Not in this implementation -- outline only.**

**Goal:** Replace in-game key presses (e.g., TAB for automap) with overlay button taps.

**Approach:**
6.1. Extend game introspection (game_introspect.c) with overlay button coordinates:
   - Add JNI callback to query TouchOverlayView for button positions
   - Serialize as `"overlay_buttons"` in introspect.json
   
6.2. Add `STEP_TAP_OVERLAY` in game_automate.cpp:
   - Find button by text/id substring
   - Inject SDL touch event at button center coordinates
   - Validate button exists/visible (fail if not)

6.3. TouchOverlayView already tracks button positions in instance fields:
   - mapBtnCenterX/Y/Radius (lines 304-306)
   - adminTrayRects (line 2623)
   - buttonStates (line 210)
   - automapBtnRects (line 354)

6.4. Migration targets:
   - `key "tab"` -> `tap_overlay "MAP"` (automap)
   - Admin tray buttons (quick save/load, menu, etc.)
   - Fire buttons (primary/secondary)

---

## Relevant Files

- `SetupActivity.kt` -- button discovery, tap injection, scroll, introspection, pending launch
- `LauncherScriptExecutor.kt` -- tap_button/assert_button action handlers
- `test_helpers.ps1` -- Start-GameWithRetry, Get-ScriptIsLauncher
- `run_test.ps1` -- launcher vs game detection + launch
- `TouchOverlayView.kt` -- overlay button positions (future Phase 6)
- All 16 json5 scripts in `android/game_scripts/`

## Decisions

- **MotionEvent injection** over programmatic onClick -- per user preference, more realistic
- **All scripts migrated** including game-only -- per user preference
- **GOG test**: keep broadcast import_gog, replace enter_game with tap_button "Launch Descent 2"
- **FilterChip game selection**: tapped as a button via the same tap_button mechanism
- **launches_game field**: explicit flag on tap_button steps that trigger game launch (controls executor suspend/resume)
- **enter_game preserved**: not removed, just not used in migrated scripts -- allows incremental rollback
- **SAF file picker**: too complex to automate (system UI), keep broadcast-based import for now
