# Plan: Texture Scan, Net Stats, Debug Log Session, Shader Warnings

## Scope
1. D2 512 texture pack summary in the launcher undercounts textures
2. Net Stats stays enabled in single player even though Net Events is disabled there
3. Debug logs are split across multiple files during one launcher to game session
4. Shader compile and link warnings are not surfaced in exportable Graphics logs

## Current Findings
- `android/app/src/main/java/com/dxxredux/app/DxaTextureScanner.kt` only scans `.png` and `.tga` entries. It ignores `.ktx2` and `.jpg`
- `game_data/mods/d2x-xl/convert_d2xxl_textures.ps1` packages real texture payloads as `.ktx2` and writes `_mask.png` sidecars for supertransparency masks. The launcher summary is therefore likely counting mask PNGs while ignoring the real texture entries
- `android/app/src/test/java/com/dxxredux/app/DxaTextureScannerTest.kt` currently codifies that `.ktx2` entries are ignored
- `android/app/src/main/java/com/dxxredux/app/SetupActivity.kt` only writes launcher DXA scan details to the debug log when `oversizedCount > 0`, so the temp logs do not currently explain a non-oversized `512x512` pack summary
- `android/temp_game_logs/debuglog_20260413_210857.txt` confirms the mounted pack is `d2-hires-512-textures-ktx2.dxa`
- `android/ai tool plans/plan_d2x_xl_hires_mods.md` records the original D2 source pack as `569 TGA + 1 JPG`, which is consistent with the user expecting a much larger count than `168`
- `android/app/src/main/java/com/dxxredux/app/MainActivity.kt` already has `shouldEnableNetEventsControl(...)`, but `ADMIN_NET_STATS` is not wired into the same enabled-state policy
- `android/app/src/main/java/com/dxxredux/app/SetupActivity.kt` creates `MainActivity` intents in three places. Only the multiplayer launch path passes `netlog_path`, so normal launch and automation launch create a fresh debug log file in the game process
- `android/app/src/main/java/com/dxxredux/app/MainActivity.kt` appends to an existing log only when `intent.getStringExtra("netlog_path")` is present. Otherwise it calls `DebugLog.init(...)`, which opens a new file
- `d1/arch/ogl/oglprog.c`, `d2/arch/ogl/oglprog.c`, and `android/app/src/main/cpp/shared/gles3_shim.c` only fetch shader and program info logs on hard failure. Warning-only info log output is dropped today

## Phase 1: Fix Launcher Texture Pack Summary
- [x] Extend `DxaTextureScanner.scan(...)` to understand `.ktx2` dimensions so the launcher counts the real texture payloads in KTX2 packs
- [x] Exclude `_mask.png` sidecars from `textureCount` so mask helper assets do not inflate the visible texture summary
- [x] Keep existing `.png` and `.tga` handling for older packs. No `.jpg` DXA handling was added because the current generated hires packs convert JPG payloads to `.ktx2` before import
- [x] Keep the launcher mod summary driven by the scanner so KTX2 packs report honest counts once the scan result is fixed
- [x] Extend launcher debug logging so scan summaries are logged even when nothing is oversized, when the Launcher category is enabled
- [x] Expand `DxaTextureScannerTest.kt` with a mixed archive case: KTX2 texture entries plus `_mask.png` sidecars, and assert that masks do not inflate the visible texture count

### Key Files
- `android/app/src/main/java/com/dxxredux/app/DxaTextureScanner.kt`
- `android/app/src/main/java/com/dxxredux/app/SetupActivity.kt`
- `android/app/src/test/java/com/dxxredux/app/DxaTextureScannerTest.kt`
- `game_data/mods/d2x-xl/convert_d2xxl_textures.ps1`

### Likely Functions
- `DxaTextureScanner.scan(...)`
- `DxaTextureScanner.readPngDims(...)`
- new KTX2 header reader in `DxaTextureScanner.kt`
- `ModsSection(...)` / `ModRow(...)` in `SetupActivity.kt`

## Phase 2: Disable Net Stats in Single Player
- [x] Add a shared policy helper for MP-only admin tray overlays so Net Stats and Net Events use the same enable rule
- [x] Wire `TouchOverlayView.ADMIN_NET_STATS` into `adminTrayEnabledStateProvider`
- [x] When Net Stats becomes disabled, immediately hide the stats overlay so the UI state matches the disabled control state
- [x] Keep multiplayer and pending-launch behavior aligned with Net Events so the tray still works during matchmaking and pre-game MP setup
- [x] Add JVM coverage for the shared enabled-state rule and for the Net Stats disabled path

### Key Files
- `android/app/src/main/java/com/dxxredux/app/MainActivity.kt`
- `android/app/src/main/java/com/dxxredux/app/TouchOverlayView.kt`
- `android/app/src/test/java/com/dxxredux/app/OverlayVisibilityPolicyTest.kt`
- `android/app/src/test/java/com/dxxredux/app/AdminTrayUiTest.kt`

### Likely Functions
- `shouldEnableNetEventsControl(...)` or its replacement
- `adminTrayEnabledStateProvider`
- `adminTrayCallback`

## Phase 3: Keep One Debug Log File Per Launcher To Game Session
- [x] Factor a small helper in `SetupActivity.kt` so every `MainActivity` intent gets the active `netlog_path`
- [x] Update the normal launch path and the automation launch path to use that helper. Keep the multiplayer path on the same helper so future launch sites do not drift again
- [x] Confirm by code path review and Android build/test that `MainActivity.onCreate(...)` still uses `DebugLog.initAppend(...)` when the path is present
- [ ] Re-verify on-device file continuity with debug logging enabled. The integration run in this session used an emulator profile with all debug-log categories disabled, so no fresh `debuglog_*.txt` file was produced for a direct file-count check

### Key Files
- `android/app/src/main/java/com/dxxredux/app/SetupActivity.kt`
- `android/app/src/main/java/com/dxxredux/app/MainActivity.kt`
- `android/app/src/main/java/com/dxxredux/app/DebugLog.kt`
- `android/app/src/main/java/com/dxxredux/app/multiplayer/NetLog.kt`

### Likely Functions
- `launchGameForAutomation(...)`
- `launchMultiplayerGame(...)`
- `onLaunchGame = { game -> ... }`
- `DebugLog.init(...)`
- `DebugLog.initAppend(...)`

## Phase 4: Surface Shader Warnings In Graphics Logs
- [x] Add a small helper in `d1/arch/ogl/oglprog.c` and `d2/arch/ogl/oglprog.c` that always reads shader and program info logs after compile and link, not just on failure
- [x] On Android, route any non-empty info log output to `debug_log(DLOG_GRAPHICS, ...)` with enough labeling to identify vertex vs fragment vs program and which inline shader path produced the message
- [x] Preserve the current fatal behavior on hard shader compile or link failures. The new logging adds visibility without changing failure handling
- [x] Update `android/app/src/main/cpp/shared/gles3_shim.c` the same way so shader warnings from the GLES3 shim land in exportable Graphics logs instead of logcat only
- [x] Keep the non-Android shader paths functionally unchanged aside from reading info logs and using clearer failure text

## Validation Result
- [x] `android\run-code-quality.ps1 -Fix`
- [x] `android\gradlew.bat :app:testDebugUnitTest :app:assembleDebug` with `JAVA_HOME=c:\local\jdk-21`
- [x] `android\run_test.ps1 -ScriptName test_launch_to_automap.json5 -Game d2` passed after the script restarted an unhealthy emulator. Final exit code was `0`
- [x] On-device launcher logging now identifies the exact oversized textures for
	the 512 pack warning. `android\temp_game_logs\debuglog_20260414_075137.txt`
	logged `mod-dxa-scan file=d2-hires-512-textures-ktx2.dxa bytes=408948131
	textures=1612 oversized=4 max=4096x8192` with entries `arw01.ktx2`,
	`cockpitbx2.ktx2`, `flare.ktx2`, and `statusbx2.ktx2`
- [ ] Desktop CMake build and test remain blocked in this environment. Existing cached build trees are incomplete, and a fresh D2 Visual Studio configure failed on missing `SDL_mixer` desktop dependencies

### Key Files
- `d1/arch/ogl/oglprog.c`
- `d2/arch/ogl/oglprog.c`
- `android/app/src/main/cpp/shared/gles3_shim.c`
- `android/app/src/main/cpp/shared/android_log.h`
- `android/app/src/main/cpp/shared/debug_log_categories.h`

### Likely Functions
- `ogl_mk_prog(...)`
- `ogl_init_prog()`
- `compile_shader(...)` in `gles3_shim.c`
- `gles3_shim_init()`

## Verification Plan
1. Run `android\run-code-quality.ps1 --fix`
2. Run focused JVM tests for the launcher scanner and overlay-policy changes
3. Build Android debug APK after the Kotlin and C/C++ changes
4. Run a normal launcher to game launch and confirm one `debuglog_*.txt` file continues growing across the transition
5. Launch the D2 512 KTX2 pack in the launcher and confirm the summary reflects real texture entries instead of mask sidecars
6. Verify Net Stats is disabled in single player and enabled in multiplayer or pending MP launch states
7. Start D1 and D2 with Graphics logging enabled and inspect exported debug logs for shader info-log output
8. Run at least one focused Android integration script such as `android\run_test.ps1 -ScriptName test_launch_to_automap.json5 -Game d2`
9. Run the required desktop CMake build because `d1/` and `d2/` OGL files will change

## Suggested Implementation Order
1. Launcher texture scan fix and JVM tests
2. Net Stats enabled-state fix and JVM tests
3. Debug log path handoff unification
4. Shader warning logging in d1, d2, and the GLES3 shim
5. Android and desktop verification pass