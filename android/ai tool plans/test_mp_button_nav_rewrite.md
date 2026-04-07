# test_mp.ps1 button-based navigation rewrite

## goal
Rewrite test_mp.ps1 to navigate through the launcher UI instead of using
invisible MP_COMMAND broadcasts. The test should open the multiplayer page,
tap buttons, and verify UI state as it progresses.

## changes

### 1. Add `tap_button` MP_COMMAND to SetupActivity.kt
- Reuse existing `findButtonByText()` + `performAccessibilityClick()` / `injectTapAt()` infrastructure
- New MP_COMMAND `tap_button` with `--es text "Button Label"`
- Runs on main thread via coroutine scope, same as LauncherScriptExecutor
- Returns button list in logcat if button not found (for debugging)

### 2. Add `set_text_field` MP_COMMAND to SetupActivity.kt (if needed)
- For setting server URL and callsign text fields
- Alternative: keep using `set_callsign` and `connect --es url` MP_COMMANDs since they
  set the underlying state that the UI reads

### 3. Add Send-TapButton helper to test_helpers.ps1
- Wraps the tap_button MP_COMMAND broadcast
- Includes polling for button to appear (accessibility tree may take time)

### 4. Rewrite test_mp.ps1 phases 3-7
- Phase 3: Tap "Multiplayer" on both, set callsigns, connect
- Phase 4: EMU1 taps "Create Lobby" (after setting game/mission via MP_COMMAND)
- Phase 5: EMU2 refreshes, taps lobby to join
- Phase 6: Slim chat (one message each way, brief verify, non-fatal)
- Phase 7: Both tap "Ready", EMU1 taps "Start Game"
- Phase 8+: Keep existing game launch / verify / sustain phases

## non-goals
- Don't rewrite phases 8-10 (game launch, verify, sustain)
- Don't add full text field manipulation (keep set_callsign/connect commands)
