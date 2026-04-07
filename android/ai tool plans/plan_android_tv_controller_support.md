# Android TV / NVIDIA Shield Controller Support Plan

## Overview
Add Android TV compatibility so the app can be deployed to NVIDIA Shield and other Android TV devices. This requires manifest changes, a TV banner, and ensuring all UI is navigable with a game controller (no touch required).

## Phase 1: Manifest & Resource Changes (this session)
- [x] Add `uses-feature` declarations for touchscreen (required=false), leanback (required=false), gamepad (required=false)
- [x] Add leanback support metadata
- [x] Add NVIDIA controller support metadata
- [x] Add `android.intent.category.LEANBACK_LAUNCHER` intent filter to SetupActivity
- [x] Create placeholder TV banner (1280x720) drawable resource
- [x] Verify no other `uses-feature` declarations implicitly require touch-only hardware

## Phase 2: Launcher Controller Navigation (done)
Key changes:
- [x] Modified `dispatchKeyEvent` to stop consuming D-pad/A/B events (lets Compose focus system work)
- [x] B button mapped to system Back via `onBackPressedDispatcher`
- [x] A button mapped to DPAD_CENTER for element activation
- [x] Added `controllerConfigActive` flag to preserve controller config page behavior
- [x] Added `BackHandler` to all sub-pages for B-button back navigation
- [x] Added `DisposableEffect` to set/clear controller config flag
- [x] Created `DpadFocusUtils.kt` with `tvFocusable()` modifier for custom surfaces
- [x] Added keyboard-based grab-and-move to AutoselectEditorPage (A to grab, D-pad to move, A to drop)
- [x] All Material3 components (Button, RadioButton, Switch, Checkbox, FilterChip, TextField) work automatically with focus
Detailed review of every launcher screen below with controller-readiness assessment.

### Screen-by-screen Controller Audit

#### Setup Screen (main screen) - SetupActivity.kt ~L2365
**Elements:** Game tabs (D1/D2), file status list, import buttons, launch buttons, settings buttons
**Controller approach:**
- Tabs: focusable, D-pad left/right to switch
- Buttons: focusable, D-pad up/down to navigate, A to activate
- File list: scrollable LazyColumn, D-pad nav works natively with focusable items
- Import buttons: SAF file picker is system UI and has basic D-pad support
**Issues:** None critical. Standard focus-based navigation should work.
**TV-specific:** Need to handle case where SAF picker is less usable on TV. Consider adding a "scan for files" option that checks common paths (/storage/*, USB drives) automatically.

#### Graphics Settings - GraphicsSettingsPage.kt
**Elements:** Dropdowns, radio buttons, sliders
**Controller approach:**
- Radio buttons: focusable, A to select
- Dropdowns: focusable, A to open, D-pad to select
- Sliders: focusable, D-pad left/right to adjust
**Issues:** None critical. Material Compose components have built-in focus support.

#### Controller Config - ControllerConfigPage.kt
**Elements:** Canvas visualization, binding picker dialogs, radio groups
**Controller approach:**
- The current canvas-based picker (tap a button region to bind) is problematic
- **Simplify for now: preset-based system** with 3-4 named presets
- Presets selectable via radio buttons, A to apply
- Optional: simple list-based binding editor (LazyColumn of action -> button pairs)
**Issues:** Canvas coordinate picking is not controller-friendly
**Decision:** Presets only for initial TV release

#### Touch Layout Editor - TouchEditorPage.kt
**Elements:** Full canvas drag-and-drop editor
**Controller approach:**
- **Skip for TV entirely** -- touch layout is irrelevant when using a controller
- Hide/disable this screen when running on TV (no touchscreen)
**Issues:** N/A for TV

#### Music Picker - MusicPickerPage.kt
**Elements:** Tabs, file picker, seek slider, text input, track list
**Controller approach:**
- Tabs: D-pad left/right
- Track list: LazyColumn with focusable items, A to select
- Volume/seek sliders: D-pad left/right
- File picker: SAF system UI
**Issues:** Seek slider fine control, text input for custom names
**TV-specific:** Text input via on-screen keyboard (Android TV has one built in)

#### Autoselect Weapon Editor - AutoselectEditorPage.kt
**Elements:** Draggable reorderable list
**Controller approach:**
- **Replace drag with button-based reorder:** D-pad to select item, then X/Y to move up/down
- Or: A to "grab" item, D-pad to move, A to "drop"
- Need to add this interaction mode
**Issues:** Drag-to-reorder is touch-only. Button-based reorder needed.

#### Advanced Settings - AdvancedSettingsPage.kt
**Elements:** Buttons, dropdowns, text input, file picker
**Controller approach:**
- Buttons: focusable, A to activate
- Dropdown: focusable, A to open
- Text input: Android TV on-screen keyboard
**Issues:** None critical

#### Multiplayer Lobby & Matchmaking - multiplayer/*.kt
**Elements:** Server list, lobby list, player list, chat, dropdowns
**Controller approach:**
- Lists: focusable items, D-pad nav, A to select/join
- **"Last used server" presets:** Add a saved-servers list at top of server browser, selectable with D-pad + A
- Chat: text input via on-screen keyboard
- Create lobby: focusable form fields
- Ready/Start buttons: focusable, A to activate
**Issues:** Text input for callsign/chat. Server address entry.
**TV-specific:** Saved server presets are critical since typing IPs on a controller is painful

## Phase 3: In-Game Overlay Controller Support (future)
Controller bindings for overlay actions currently touch-only:

#### Overlays needing controller bindings:
1. **WarpButtonOverlay** - needs Start/Select or button chord to warp, another to cycle target
2. **MusicControlPanel** - needs button to open/close, D-pad to select track, A to play
3. **AcceptJoinButtonView** - needs button to accept (could be auto-bound to a prompt)
4. **SkipButtonView** - ESC key already works via controller mapping
5. **ExitButtonView** - admin tray already accessible

#### Display-only overlays (no changes needed):
- VideoInfoOverlay (toggle via admin tray)
- CoopStatsOverlay (auto-show)
- MultiplayerStatsOverlay (toggle via admin tray)
- NetworkEventsOverlay (toggle via admin tray)

#### Start/Select Button Usage Plan:
- **Start button** -> Open in-game pause/escape menu (standard mapping)
- **Select button** -> Toggle overlay panel (music, warp, admin tray cycle)
- These should be **reserved** from the controller binding system (not assignable to game actions)
- Controller presets should not use Start/Select for game controls

## Phase 4: Controller Config Presets (future)
- Define 3-4 controller presets matching common TV controller layouts:
  - "Standard" (Shield controller layout)
  - "Xbox-style" (A/B/X/Y standard)
  - "PS-style" (Cross/Circle/Square/Triangle)
  - "Flight-sim" (sticks for movement, triggers for weapons)
- Presets stored as JSON files in assets
- Simple selector UI: radio button list, A to apply, preview showing binding summary

## Phase 5: TV Banner & Store Assets (future)
- Create proper 1280x720 TV banner with game artwork
- Create 320x180 TV banner for smaller displays
- Update Play Store listing with TV screenshots
