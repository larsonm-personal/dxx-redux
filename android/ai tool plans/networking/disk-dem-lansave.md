# Disk Space + Demo Import + LAN Save Picker -- Plan

## Task 1: Emulator Disk Space Handling

### Problem
- 4GB data partition can fill up after failed GOG extraction (temp files not cleaned)
- No script checks free disk space before or during operations
- emu_health.ps1 checks process/adb/boot but not disk

### Changes
1. **emu_health.ps1**: Add disk space check to `Test-EmulatorHealth` -- query `/data` free space via `df`, warn if <500MB
2. **create_light_avds.ps1**: Increase data partition from 4G to 8G for new AVDs
3. **emu_health.ps1**: Add `Clear-EmulatorTmp` function that cleans `/data/local/tmp/` stale files

## Task 2: .dem Demo File Import

### Problem
- `.dem` not in EXTENSION_TYPES -- file viewer shows "unknown type"
- SAF scanner only matches exact filenames in ALL_GAME_FILENAMES -- `.dem` files ignored
- `import_files` copies to set root, but engine expects demos in `demos/` subdirectory

### Changes
1. **SetupActivity.kt EXTENSION_TYPES**: Add `"dem" to ".dem -- game demo recording"`
2. **SetupActivity.kt import_files**: Place `.dem` files into `demos/` subdirectory
3. **SetupActivity.kt scanTreeForGameFiles**: Also match files with `.dem` extension
4. **SetupActivity.kt importFile**: Route `.dem` files to `demos/` subdirectory
5. No engine changes needed -- D1/D2 already search `demos/` for `.dem` files via PHYSFSX_findFiles

## Task 3: LAN Lobby Coop Save Picker

### Problem
- Online lobby has CoopSaveOffer composable (auto-offer matching saves)
- LAN lobby StartLanGameDialog has only difficulty + level, no save picker
- Helper functions (readCoopAutosaveHistory, writeCoopRestoreSlot, etc.) are already `internal`

### Changes
1. **LanDiscoveryTab.kt**: Track `hostedGame` and `hostedMode` state from HostLanGameDialog
2. **LanDiscoveryTab.kt**: Add CoopSaveOffer card in hosted lobby section (between player list and Start Game button) when mode == "coop"
3. Reuse existing internal helpers from MultiplayerScreen.kt
4. Write coop_restore_slot.txt before LobbyService.startGame()

### Key detail:
- HostLanGameDialog already passes game/mission/mode via onHost callback
- Need to store game+mode in LanDiscoveryView state so they're available when rendering the hosting section
- Also need the mission for save matching -- track hostedMission too
