# Landscape/Portrait UI Fixes Plan

## File-by-file changes

### 1. SetupActivity.kt - Import button text + Multiplayer gating

**1a. Import button text size** (~L3401)
- Change `fontSize = 14.sp` to `fontSize = 11.sp` on the "Select Game Files..." text
- Text is currently "Select Game Files or Archive to Import" at 14.sp

**1b. Gate Multiplayer button** (~L3888)
- Add `enabled = canLaunch` to the Multiplayer Button
- Add conditional color (gray when disabled, same pattern as Launch button)
- Since `canLaunch` is `d2RequiredOk || d1RequiredOk`, same gate as Launch

### 2. GraphicsSettingsPage.kt - Remove "next launch" lines

**2a.** Remove "Takes effect on next launch" line (~L220) + preceding Spacer
**2b.** Remove "MSAA and AF take effect on next launch" line (~L120) + preceding Spacer

### 3. AdvancedSettingsPage.kt - Debug logging rename + remove description

**3a.** Rename "Debug Logging" to "Debug Logging Categories" (~L265)
**3b.** Remove the description line and its Spacer (~L267-272):
  - "Log categories to files for debugging. Enable only what you need."

### 4. AutoselectEditorPage.kt - Spacing and size reductions

**4a.** Reduce title font sizes
- TopAppBar "Weapon Autoselect": reduce from default (~20sp) to ~14sp
- "Long press + drag..." subtitle: reduce from 12.sp to ~8.sp
- Both landscape and portrait instances

**4b.** Reduce gap between title area and D1/D2 buttons
- `Modifier.padding(vertical = 2.dp)` on game selector Row -> remove or reduce

**4c.** Reduce D1/D2 button heights by 30%
- `contentPadding = PaddingValues(horizontal = 16.dp, vertical = 4.dp)` -> reduce vertical to 1.dp

**4d.** Reduce Reset/Save button heights by 30%
- Add `height(28.dp)` to the Reset/Save buttons (default ~40dp * 0.7 = 28dp)
- `.padding(vertical = 8.dp)` on button Row -> reduce to 4.dp

### 5. ControllerConfigPage.kt - Size reductions

**5a.** Reduce "Controller Layout" title + "Tap any control..." to one line
- "Controller Layout" (20.sp) -> 16.sp
- Remove Spacer between them
- Put "Tap to assign" (shorter) on same line as "Controller Layout"

**5b.** Reduce "Not detected" / controller name line
- 13.sp -> 11.sp

**5c.** Reduce Cancel/Save button height by 20%
- 48.dp -> 38.dp
- Save button: reduce content padding to fit "Save" + "(to all pilots)"
- Reduce line spacing between "Save" and "(to all pilots)"

### 6. MultiplayerScreen.kt - Layout changes

**6a.** Rename "Server URL" to "Matchmaking Server URL" (~L168)

**6b.** Portrait mode: move Connect next to Server URL, LAN next to Callsign (~L185)
- Need to import LocalConfiguration
- In portrait: wrap URL+Connect and Callsign+LAN in Rows
- In landscape: keep current layout

### 7. LanDiscoveryTab.kt / MultiplayerScreen.kt - LAN games fixes

**7a.** In LanContent (MultiplayerScreen.kt ~L594):
- Remove the "Back" button (that goes to main launcher)
- Rename "Back to Lobbies" to "Back"

**7b.** In portrait mode, move all buttons onto the header line:
  Back at left, Host next, Join by IP next, Stop scanning last
- Need import LocalConfiguration in LanDiscoveryTab
- Restructure action buttons Row for portrait

### 8. CreateGameDialog.kt - Vertical space reclamation

**8a.** Move Cancel to the top line next to title
- Remove dismissButton from AlertDialog
- Add Cancel button to the title Row

**8b.** Reduce Host button container height
- Move confirmButton content into the scrollable area as a sticky bottom
- OR: reduce padding around the confirm button TextButton

**8c.** Change difficulty buttons to a popup/dropdown picker
- Replace the 5-button Row with an ExposedDropdownMenuBox
- Shows selected difficulty, tap to expand full list

**8d.** Add scroll indicators to "Your Hosted Lobby" section
- In LanDiscoveryTab, wrap the hosted lobby Card content in a scrollable
  container with ScrollArrows

## Implementation order

Phase 1: Simple text/sizing changes (1a, 2a, 2b, 3a, 3b, 5b) -- DONE
Phase 2: Button sizing (4a-4d, 5a, 5c) -- DONE
Phase 3: Layout restructuring (1b, 6a, 6b, 7a, 7b) -- DONE
Phase 4: CreateGameDialog rework (8a, 8b, 8c) -- DONE (8d skipped - hosted lobby already in LazyColumn)
Phase 5: Build, lint, test -- DONE
