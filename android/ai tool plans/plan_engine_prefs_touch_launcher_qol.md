# Engine Preferences, Touch Menu, Host Defaults, Mouse, and Gyro Survey

## Scope
This plan covers the following requested items:

1. Preserve HUD size preference across engine start, in-game changes, and export/import app settings
2. Preserve auto-level on/off the same way
3. Add the settings button as a configurable draggable touch element
4. Opening settings should pause the game and closing should unpause in single-player
5. Add a launcher engine preferences screen for HUD, auto-level, guidebot helper line, and coop nearest-player line
6. Persist coop server QoL on/off and the most recently selected host preference in exportable/loadable settings
7. Change in-game extra overlays from grid buttons to check boxes
8. Improve exponential mouse mode based on recent movement rate
9. Extend the gyro centering button with long-press enable/disable, red disabled state, and persistent player preference

## High-confidence findings

### 1. HUD size is almost certainly the cockpit mode preference
- Android admin tray "Cycle View" is wired through `nativeCycleCockpit()` and `nativeGetCockpitMode()` in `android/app/src/main/cpp/android_input.c`
- D1 and D2 already persist `PlayerCfg.PreferredCockpitMode` in the player file and reapply it on gameplay transitions
- `toggle_cockpit()` in both `d1/main/gamerend.c` and `d2/main/gamerend.c` already calls `write_player_file()`
- I did not find an Android startup path that overwrites this preference after load
- Main missing pieces are launcher visibility and config export/import
- Open question: confirm with the user that "HUD size" really means cockpit/status/fullscreen view selection, not a different HUD scale concept
- user: yes, it's cockpit/status/fullscreen (three modes) - not sure on the "status" mode but cockpit and fullscreen are correct

### 2. Auto-level already persists natively
- Android admin tray uses `nativeToggleAutoLeveling()` in `android/app/src/main/cpp/android_input.c`
- That JNI path flips `PlayerCfg.AutoLeveling`, updates `PF_LEVELLING`, and immediately calls `write_player_file()`
- D1 and D2 already load/store the underlying field in `playsave.c`
- Main missing pieces are launcher visibility and config export/import

### 3. The settings button is not a normal layout button today
- It is stored as `DiagnosticType.MENU` in `android/app/src/main/java/com/dxxredux/app/TouchControl.kt`
- It is rendered and hit-tested specially in `TouchOverlayView.kt`
- It is previewed specially in `TouchEditorPage.kt` as the 3x3 dot-grid icon
- This means "make it draggable/configurable" is a touch-layout model migration, not a simple geometry change

### 4. Opening the current settings tray does not pause the game
- The admin tray is a Kotlin overlay in `TouchOverlayView.kt`, not a native engine window
- Opening it just flips `adminTrayOpen` and animates the panel
- Existing `nativeOnPause()` in `android/app/src/main/cpp/android_input.c` already has the right safety rules for single-player only:
  - only inject Escape during live gameplay
  - skip multiplayer
  - skip if a menu already covers the game
- That logic is a good baseline, but the tray still needs its own pause ownership tracking on the Kotlin side

### 5. Launcher export/import is the main Android gap
- `android/app/src/main/java/com/dxxredux/app/ConfigImportExport.kt` only exports four app prefs:
  - `render_resolution`
  - `game_orientation`
  - `music_mode`
  - `touch_overlay_enabled`
- It also exports a limited set of graphics keys from `descent.cfg`
- None of the requested engine/player/host-default additions are currently included

### 6. Host defaults are already centralized, but incomplete
- `HostGameDefaults` in `android/app/src/main/java/com/dxxredux/app/multiplayer/MatchmakingState.kt` stores only:
  - `host_game`
  - `host_mission_$game`
  - `host_mode`
  - `host_difficulty`
  - `host_level_num`
  - `host_max_players`
- `CreateGameDialog.kt` and `MultiplayerScreen.kt` already read/write those defaults
- This is the right place to extend for newer host-side selections
- Open question: "most recently selected host preference" probably means these Create Game defaults, but confirm whether the user meant one specific host-only toggle instead

### 7. Coop indicator lines are server-gated only
- `android/app/src/main/cpp/shared/coop_indicator_lines.c` currently gates rendering through `NETGAME_FLAG_COOP_QOL`
- That flag is the server-wide switch in `d1/main/multi.h`, `d2/main/multi.h`, `d1/main/net_udp.c`, and `d2/main/net_udp.c`
- There is no local per-player launcher preference for guidebot line or nearest-player line yet
- The requested launcher settings should be layered on top of the server gate, not replace it

### 8. The current exponential mouse mode uses distance from touch-down origin
- `TouchOverlayView.kt` computes the mouse multiplier from distance between current pointer position and `mouseOriginX/mouseOriginY`
- It does not use recent velocity, recent acceleration, or a short movement history
- So the requested change is a behavior rewrite in Kotlin only, not a D1/D2 change

### 9. Gyro enable already persists in the touch layout
- `GyroConfig.enabled` already exists in `android/app/src/main/java/com/dxxredux/app/TouchControl.kt`
- It is serialized in JSON and the human-readable config path
- `MainActivity.kt` already applies `layout.gyro.enabled` when wiring the live gyro manager
- The current gyro recenter button only calls `gyroManager?.calibrate()` on press
- So the requested long-press disable looks like a UX and rendering change on top of existing persistence, not a new storage format

## Per-item survey

### 1. HUD size / cockpit mode persistence and export-import
Affected files:
- `android/app/src/main/cpp/android_input.c`
- `android/app/src/main/java/com/dxxredux/app/MainActivity.kt`
- `android/app/src/main/java/com/dxxredux/app/ConfigImportExport.kt`
- `d1/main/gamerend.c`
- `d1/main/playsave.c`
- `d1/main/playsave.h`
- `d1/main/game.c`
- `d2/main/gamerend.c`
- `d2/main/playsave.c`
- `d2/main/playsave.h`
- `d2/main/game.c`

Current behavior:
- In-game cycling already persists immediately in D1 and D2
- Native runtime already exposes the current preferred mode back to Android
- Engine reapplication already exists after loading player data and when re-entering gameplay
- Export/import does not include it

Likely fix:
- Treat D1/D2 player-file state as the source of truth
- Add a thin native helper for launcher-side get/set so Kotlin does not duplicate player-file rules
- Extend config export/import to include the exposed value
- Avoid a separate Kotlin-only copy of cockpit mode except possibly as temporary UI state while editing

Risk:
- Low for native persistence
- Medium for launcher export/import if it needs to work while the game engine is not actively running

### 2. Auto-level persistence and export-import
Affected files:
- `android/app/src/main/cpp/android_input.c`
- `android/app/src/main/java/com/dxxredux/app/MainActivity.kt`
- `android/app/src/main/java/com/dxxredux/app/ConfigImportExport.kt`
- `d1/main/playsave.c`
- `d1/main/menu.c`
- `d2/main/playsave.c`
- `d2/main/menu.c`

Current behavior:
- Admin tray toggle already persists immediately through `write_player_file()`
- Existing native player setting already controls live leveling state
- Export/import does not include it

Likely fix:
- Same helper pattern as cockpit mode
- Expose it cleanly to launcher UI and export/import
- Keep C as the single source of truth for the persisted value

Risk:
- Low

### 3. Settings button as a draggable touch element
Affected files:
- `android/app/src/main/java/com/dxxredux/app/TouchControl.kt`
- `android/app/src/main/java/com/dxxredux/app/TouchBindings.kt`
- `android/app/src/main/java/com/dxxredux/app/TouchOverlayView.kt`
- `android/app/src/main/java/com/dxxredux/app/TouchEditorPage.kt`
- `android/app/src/main/java/com/dxxredux/app/HumanReadableConfig.kt`

Current behavior:
- Settings/menu is a diagnostic object, not a `ButtonControl`
- It gets special hit priority and special rendering
- If no menu diagnostic exists, the fallback slide tab can still open the admin tray

Likely fix:
- Best long-term path is probably to make settings a normal `ButtonControl` with a dedicated binding or meta action
- Add migration from old `DiagnosticType.MENU` layouts into the new representation
- Keep fallback tray tab for layouts that intentionally omit the button or for gamepad-only flows
- If a smaller first pass is needed, support both old diagnostic-menu and new button path during migration

Risk:
- Medium
- The highest risk here is breaking existing saved layouts or editor assumptions

### 4. Pause on settings open and unpause on close in single-player
Affected files:
- `android/app/src/main/java/com/dxxredux/app/TouchOverlayView.kt`
- `android/app/src/main/java/com/dxxredux/app/MainActivity.kt`
- `android/app/src/main/cpp/android_input.c`
- possibly small D1/D2 helper exposure if Kotlin needs a safer native query/action pair

Current behavior:
- Tray open/close does not touch game pause state
- The actual native Game Menu is opened through the admin action that injects Escape
- `nativeOnPause()` already has the desired single-player-only safety rules for app backgrounding

Likely fix:
- Add an explicit overlay-owned pause token, for example `adminTrayPausedGame`
- On tray open:
  - only do this during live gameplay
  - skip multiplayer
  - skip if a native menu already covers the game
  - open the native pause/menu safely through a helper instead of raw blind toggling if possible
- On tray close:
  - only send the matching unpause/close action if the tray itself caused the pause
  - do not close a menu that the player opened separately

Notes:
- Existing `nativeIsInGame()` is helpful but probably not enough by itself to make close-side behavior safe
- A dedicated helper like "open pause menu if safe" or "close pause menu if still front and tray-owned" would reduce toggle mistakes

Risk:
- Medium to high
- This is the most behavior-sensitive item because bad Escape toggling can unpause at the wrong time

### 5. Launcher engine preferences page
Affected files:
- `android/app/src/main/java/com/dxxredux/app/SetupActivity.kt`
- likely a new Compose section or helper inside `SetupActivity.kt` unless split into a new file
- `android/app/src/main/java/com/dxxredux/app/ConfigImportExport.kt`
- `android/app/src/main/java/com/dxxredux/app/MainActivity.kt`
- `android/app/src/main/cpp/android_input.c`
- `android/app/src/main/cpp/shared/coop_indicator_lines.c`

Current behavior:
- Controller section already links to Touch Layout, Weapon Autoselect, Graphics, and Advanced
- There is no dedicated engine-preferences screen
- Android already has hook points where a new launcher page could fit cleanly

Likely fix:
- Add a "Game Preferences" or similarly named launcher page near Graphics/Advanced
- Put the following there:
  - cockpit/HUD mode
  - auto-level
  - guidebot helper line
  - nearest-player line
- Use native helpers for player-file-backed prefs
- Use Android-local prefs for local-only visual toggles, then feed those to native rendering through JNI/state plumbing

Risk:
- Medium
- The main design constraint is keeping C as the source of truth for player-file details

### 6. Persist coop server QoL toggle and latest host defaults in exportable settings
Affected files:
- `android/app/src/main/java/com/dxxredux/app/multiplayer/MatchmakingState.kt`
- `android/app/src/main/java/com/dxxredux/app/multiplayer/CreateGameDialog.kt`
- `android/app/src/main/java/com/dxxredux/app/multiplayer/MultiplayerScreen.kt`
- `android/app/src/main/java/com/dxxredux/app/ConfigImportExport.kt`
- `d1/main/net_udp.c`
- `d2/main/net_udp.c`
- `d1/main/multi.h`
- `d2/main/multi.h`

Current behavior:
- Native server flag exists and already drives the real coop QoL behavior
- Android host default persistence does not include a saved QoL on/off choice
- Export/import does not include any of the `host_*` defaults today

Likely fix:
- Extend `HostGameDefaults.Defaults` with the host-side QoL default and any other confirmed missing host selection
- Thread it through `CreateGameDialog.kt`, LAN hosting, and matchmaking hosting
- Add the new keys to config export/import
- Keep the native `NETGAME_FLAG_COOP_QOL` as the actual network truth once a session is created

Open question:
- If the user meant a different "most recently selected host preference", confirm which field before implementation

Risk:
- Low to medium

### 7. Change in-game extra overlays from grid buttons to check boxes
Affected files:
- `android/app/src/main/java/com/dxxredux/app/TouchOverlayView.kt`
- `android/app/src/main/java/com/dxxredux/app/MainActivity.kt`
- possibly `android/app/src/main/java/com/dxxredux/app/VideoInfoOverlay.kt`
- possibly `android/app/src/main/java/com/dxxredux/app/CoopStatsOverlay.kt`

Current behavior:
- The admin tray is a 3-column button grid in `TouchOverlayView.kt`
- Labels like `Net Stats`, `Net Events`, and `Video Info` are rendered as action buttons even though they toggle ongoing overlays
- Touch handling comments still explicitly refer to checking "grid buttons"

Likely fix:
- Split admin tray entries into two groups:
  - one-shot actions
  - toggles with persistent checked state
- Keep the existing tray layout/hitboxes if possible, but render toggle items with checkbox visuals and current state
- Likely checkbox candidates are the overlay toggles first, not every tray item

Open question:
- Confirm whether the user wants checkbox visuals only for overlay toggles or for the whole extra-overlay section of the tray

Risk:
- Medium if done as a full tray redesign
- Low to medium if limited to rendering/state semantics for toggle items only

### 8. Improve exponential mouse mode using recent movement rate
Affected files:
- `android/app/src/main/java/com/dxxredux/app/TouchOverlayView.kt`
- possibly `android/app/src/main/java/com/dxxredux/app/TouchControl.kt`
- possibly `android/app/src/main/java/com/dxxredux/app/TouchEditorPage.kt`

Current behavior:
- Scale factor uses pointer distance from the touch-down origin
- This means slow large drags and fast short swipes can feel wrong in opposite ways

Likely fix:
- Keep the existing accumulated mouse-delta pipeline
- Replace or blend the current origin-distance multiplier with a short-term velocity estimate
- A simple first pass would use an exponential moving average of pixels/ms and map that to the multiplier curve
- Only add new editor controls if the existing `mouseExponential` and `mouseExponentialMax` knobs are not enough

Risk:
- Medium
- This is tuning-heavy and probably needs real device validation after an emulator pass

### 9. Gyro recenter long-press disable, red disabled state, persisted
Affected files:
- `android/app/src/main/java/com/dxxredux/app/TouchOverlayView.kt`
- `android/app/src/main/java/com/dxxredux/app/TouchEditorPage.kt`
- `android/app/src/main/java/com/dxxredux/app/GyroInputManager.kt`
- `android/app/src/main/java/com/dxxredux/app/TouchControl.kt`
- `android/app/src/main/java/com/dxxredux/app/HumanReadableConfig.kt`
- `android/app/src/main/java/com/dxxredux/app/MainActivity.kt`

Current behavior:
- Pressing the gyro recenter button calibrates immediately
- The layout already persists `gyro.enabled`
- The button does not display a disabled-state color

Likely fix:
- Short press keeps the existing recenter behavior
- Long press toggles `layout.gyro.enabled`
- Save the updated layout through the existing layout persistence path
- Update the button draw style to red or another strong disabled-state tint when gyro is off
- Keep the editor preview in sync so the user can understand what the button will do in game

Risk:
- Low to medium

## Cross-cutting design choices

### Keep player-file preferences in native code
- Cockpit mode and auto-level already belong to D1/D2 player-file logic
- Do not duplicate player-file parsing in Kotlin
- Use thin native helper functions to expose launcher-safe get/set operations
- Mirror only clean values across the JNI boundary

### Keep Android-local defaults in Android storage
- Host defaults and local-only visual toggles belong in Android storage and config export/import
- `HostGameDefaults` and `ConfigImportExport.kt` are already the right central points

### Minimize D1/D2 edits
- Most of this work is Android/Kotlin/JNI plumbing
- D1 and D2 changes should stay limited to small helper exposures or shared preference hook points if needed
- Any D1/D2 hook added for one title will likely need the same hook in the other

### Plan for migration, not breakage
- Existing touch layouts likely contain `DiagnosticType.MENU`
- Migration or dual-read support is needed if settings/menu becomes a true button binding
- Export/import round-tripping must preserve older saved layouts cleanly

## Proposed implementation phases

### Phase 0 - Survey and plan
- [x] Map affected files and current behavior
- [x] Identify where native persistence already exists
- [x] Record likely implementation order and risks

### Phase 1 - Native bridge for player-backed preferences
- [x] Add or confirm thin native helpers for cockpit mode and auto-level get/set
- [x] Make those helpers callable from the launcher without duplicating player-file details
- [x] Apply the same shape to both D1 and D2

### Phase 2 - Launcher engine preferences page
- [x] Add a launcher page entry near Graphics/Advanced
- [x] Wire cockpit mode and auto-level to the new native helper layer
- [x] Apply the engine preference edits to D1 and D2 pilot files together without an in-page game picker
- [x] Add local toggle storage for guidebot line and nearest-player line
- [x] Feed local visual toggles into the native coop-line renderer path

### Phase 3 - Export/import coverage
- [x] Extend `ConfigImportExport.kt` for the new engine/host/local visual settings
- [x] Extend `HostGameDefaults` for coop QoL and any confirmed missing host selection
- [x] Add migration/default handling for older exports that lack the new keys

### Phase 4 - Settings button migration
- [x] Use an `Info` control category named `Settings` with no compatibility layer for the old menu diagnostic name
- [x] Update editor UI and preview
- [x] Preserve fallback tray tab behavior and allow a visible `Settings` button to close the tray

### Phase 5 - Tray behavior cleanup
- [x] Add explicit pause ownership for tray open/close in single-player
- [x] Use safer native pause/menu helpers instead of blind Escape toggles where practical
- [x] Change overlay-style tray items from action buttons to checkbox-style toggles
- [x] Keep gamepad-only tray navigation consistent
- [x] Keep the settings tray and launcher-side overlays visible while tray-owned pause is active
- [x] Keep AutoLevel toggles from closing the settings tray

### Phase 6 - Mouse mode tuning
- [x] Replace or blend origin-distance exponential scaling with recent-motion-rate scaling
- [ ] Validate on emulator first, then real device if possible
- [x] Only expose new tuning controls if existing config is insufficient
- Refined the multiplier trigger into a short-history fine-aim grace band so tiny drags stay unboosted even when event timing is uneven; tactile emulator/device validation is still pending

### Phase 7 - Gyro recenter UX upgrade
- [x] add a long-press option for *all* touch buttons that can be configured for a 2nd binding. when turned on (have a checkbox), a slider should appear with a minimum duration, from 200ms to 2s, with default 500ms
- [x] add a binding option for touch buttons and controller buttons that turns gyro on or off, and save the last setting immediately in the active touch layout
- [x] Toggle `gyro.enabled` on long press and persist it
- [x] route gyro on or off through the normal Android-side binding path, with only the touch-button tint remaining as a special case
- [x] add the red off state special case
- existing layouts now migrate gyro recenter buttons to long-press gyro toggle by default, and bundled advanced and claw presets declare the same behavior directly

### Phase 8 - Tests, build, and quality
- [ ] Add at least one integration-level check for config round-trip or launcher preference persistence. make every attempt to add this as an additional verification for an existing json-based test
- [ ] Add/extend automation scripts under `android/game_scripts/` for cockpit and auto-level verification if practical. extend if at all possible
- [ ] If automation needs more visibility, extend introspection to report cockpit mode and auto-level state
- [x] Run `android/run-code-quality.ps1 -Fix`
- [x] Run a successful CMake build
- [x] Run targeted tests and fix regressions

## Suggested test focus

### Low-risk automated tests
- Config export/import round-trip for new Android-side keys
- `HostGameDefaults` load/save round-trip with new fields
- Touch-layout round-trip for `Info: Settings` and fallback tray behavior when no settings control is present

### Good Android automation candidates
- Launch into gameplay, change cockpit mode, restart or reload, verify persisted mode
- Launch into gameplay, toggle auto-level, restart or reload, verify persisted value
- Open and close the settings tray in single-player, verify paused/unpaused behavior

### Introspection additions worth considering
- Expose current cockpit/preferred cockpit mode
- Expose current auto-level state
- Expose whether a game menu is currently covering the live game window

## Recommended order if scope needs trimming
1. Cockpit mode and auto-level launcher/export bridge
2. Host defaults plus config export/import
3. Settings button migration
4. Pause/unpause tray behavior and checkbox tray rendering
5. Gyro long press
6. Mouse-mode retuning

## Status
- [x] Research complete
- [x] Phase 1: Native bridge for player-backed preferences
- [x] Phase 2: Launcher engine preferences page
- [x] Phase 3: Export/import coverage
- [x] Phase 4: Settings button migration
- [x] Phase 5: Tray behavior cleanup
- [ ] Phase 6: Mouse mode tuning
- [x] Phase 7: Gyro recenter UX upgrade
- [ ] Phase 8: Tests, build, and quality