# Kotlin refactor survey - 2026-05-24

## Goal
Survey Android Kotlin sources and produce a prioritized work list for splitting only the most obvious oversized or mixed-responsibility files into smaller maintainable units.

## Survey status
- [x] Inventory Kotlin source files and rank by size.
- [x] Read the largest and most responsibility-dense files.
- [x] Spot medium-sized files that are fine as-is or have smaller targeted cleanup opportunities.
- [x] Write a tranche-oriented refactor work list with risks, suggested module boundaries, and validation notes.
- [x] Mark this survey plan complete.

## Inventory summary
- Scope: `android/**/*.kt` and `android/**/*.kts`, excluding build, Gradle cache, and Gradle wrapper directories.
- Files: 151 Kotlin files total, 110 main-source files, 41 unit-test files.
- Lines: 55,576 total, 52,123 main-source lines, 3,453 test lines.
- Heavy tail: 5 main files are 3,000+ lines and account for about 46 percent of main-source Kotlin lines.
- Top 10 main files account for about 60 percent of main-source Kotlin lines.

## Working rules for future tranches
- Prefer same-package file splits first. Moving declarations into new packages can be a later cleanup after the code is smaller.
- Keep JNI declarations that native code calls by exact class/method name in place unless the native registration path is updated in the same tranche.
- For Compose pages, first extract top-level composables and pure helpers. Avoid creating state-holder abstractions until the file already has clear child components.
- For custom `View` classes, extract pure policy helpers first, then consider small controller classes for self-contained overlay modes.
- Run `android\run-code-quality.ps1 -Fix` after code movement tranches, after first checking stale formatters as documented in repo instructions.
- Suggested quick JVM validation after each Kotlin tranche: `Push-Location android; .\gradlew.bat :app:testDebugUnitTest; Pop-Location`.
- Add or extend focused tests when a refactor changes behavior boundaries, especially input routing, config serialization, import handling, and networking.

## Tranche 1 - Split SetupActivity.kt first

Current size: 9,709 lines. This is the clearest refactor target. It combines the launcher activity, automation receivers, accessibility scanning, file readiness, game file catalogs, SAF scanning, archive extraction, config file editing, disc import, resume-save UI, mods/demos/music UI, and several import dialogs.

Keep `SetupActivity.kt` as activity orchestration only:
- lifecycle and `setContent`
- activity result launchers
- launch/return-to-game wiring
- refresh triggers and top-level page state
- calls into extracted helpers and composables

Recommended extraction order:
1. `SetupGameFiles.kt`: `GameFileInfo`, `FileStatus`, D1/D2 file lists, demo package metadata, recommended mod metadata, `findFile`, `detectD2FileList`, `checkFiles`, and `launchDataReadyForGame`.
2. `SetupFileImport.kt`: SAF tree scanning, URI display names, generic file import, temp cleanup, zip/7z/StuffIt extraction, content hashing, and `copyUriToFileWithProgress`.
3. `SetupConfigFiles.kt`: `updateDescentCfgResolution`, config read/write helpers, `updateAllConfigFiles`, `updateConfigFilesForGame`, Redbook config enablement, and launcher setup logging helpers.
4. `SetupDiscImport.kt`: GOG pair detection, SOW extraction post-processing, CUE parsing/order helpers, merged SAF disc staging, disc audio registration, ISO import, and imported game-file hoisting.
5. `SetupResumePanel.kt`: resume candidate thumbnail decode, display formatting, `ResumeSavePanel`, and resume path/callsign helpers.
6. `SetupSections.kt`: `GameSectionHeader`, `SectionHeader`, `ModsSection`, `DemosSection`, `MusicInfoSection`, `FileStatusRow`, `DownloadableFileRow`, `MissingFilesHelp`, and small row/detail composables.
7. `SetupDialogs.kt`: `FileDetailDialog`, `SetManagementDialog`, `GogImportDialog`, `SowImportDialog`, `DiscImportDialog`, and `IsoImportDialog`. If this file grows too large, split import dialogs by type.
8. `SetupAutomationApi.kt`: accessibility node scanning/click helpers, setup/controller/multiplayer introspection JSON, and automation-specific clear/patch helpers. Keep receiver fields in `SetupActivity.kt` unless a later tranche adds a small callback/controller object for the private launch state they close over.

Validation focus:
- Existing JVM tests around launch readiness, import location migration, disc import hoisting, audio-source visibility, and mod details.
- One launcher automation smoke test after the receiver/accessibility split.
- Manual import spot checks for GOG, SOW, CUE/BIN, ISO, and folder import after the import-helper split.

Progress as of 2026-05-24:
- Completed steps 1 through 8 with `SetupGameFiles.kt`, `SetupFileImport.kt`, `SetupConfigFiles.kt`, `SetupDiscImport.kt`, `SetupResumePanel.kt`, `SetupSections.kt`, `SetupDialogs.kt`, and the low-risk automation helper split in `SetupAutomationApi.kt`.
- `SetupActivity.kt` is down to 4178 lines after those splits.
- Remaining SetupActivity opportunities are smaller: `ControllerSection` can move, and the setup/multiplayer receiver fields could be wrapped later if the callback shape is worth the extra abstraction.

Expected payoff:
- Removes the 10k-line hotspot.
- Makes later launcher features easier because file/data/import logic becomes searchable without opening the full activity.
- Low risk if the first tranches keep declarations in `com.dxxredux.app` and only change visibility from `private` to `internal` where needed.

## Tranche 2 - Split TouchOverlayView.kt by overlay modes

Original size: 4,591 lines. Current `TouchOverlayView.kt` size after the helper split is 3,912 lines. This is mostly one real feature, but it contains several independent overlay modes and a useful set of pure policies above the view class.

Recommended extraction order:
1. `AdminTrayPolicy.kt`: top-level admin tray action policy helpers such as visible actions, checkbox/slider/action behavior, brightness clamping/stepping, controller menu cycling, and selection movement.
2. `WeaponWheelLabels.kt`: weapon name tables, weapon-wheel slot/current-label functions, bomb/current weapon labels, and active indicator label helpers.
3. `RemainingTouchActions.kt`: remaining-action model, binding discovery, action labels, held-action policy, and tests around remaining actions.
4. `TouchMouseAcceleration.kt`: mouse acceleration history and multiplier helpers.
5. `TouchOverlayState.kt`: small state holder classes for sticks, buttons, radial menus, sliders, diagnostics, and axis regions if they can move without creating an awkward public API.
6. `AdminTrayOverlayController`: later and higher risk. Own tray geometry, drawing, touch handling, and gamepad navigation. This can remove the largest self-contained block from the view class.
7. `RemainingActionsOverlayController`, `CheatsOverlayController`, and `AutomapOverlayController`: later controller-class extractions if the first policy/helper splits are stable.

Progress as of 2026-05-24:
- Completed steps 1 through 4 with `AdminTrayPolicy.kt`, `WeaponWheelLabels.kt`, `RemainingTouchActions.kt`, and `TouchMouseAcceleration.kt`.
- The helper split kept total line count stable: `TouchOverlayView.kt` plus the four helper files is 4,599 lines, about 8 more than the original file.
- Remaining TouchOverlayView opportunities are `TouchOverlayState.kt` and later controller-class extractions for admin tray, remaining actions, cheats, and automap overlays.

Validation focus:
- Existing unit tests: remaining key touch actions, overlay visibility policy, controller menu cycle, weapon wheel label, mouse mode tuning, and touch overlay drag-zone policy.
- Manual smoke: in-game touch buttons, admin tray, gamepad-only admin tray, weapon wheels, automap gestures, and cheats overlay.

Expected payoff:
- Good reduction in file size without forcing a rewrite of the custom `View`.
- Pure helper extractions are low risk and already map to existing unit tests.
- Controller-class extractions should be staged after helper movement because they need careful state ownership.

## Tranche 3 - Split controller and touch editor pages

Files:
- `ControllerConfigPage.kt`: 3,284 lines.
- `TouchEditorPage.kt`: 3,250 lines.

These are both coherent features, but each has clear section boundaries and would be much easier to work on as several files.

Controller config recommended splits:
1. `ControllerConfigModel.kt`: control lists, KC index maps, controller config version, joy settings constants, default bindings, thresholds, and exponent math.
2. `ControllerConfigStore.kt`: load/save/import/export config persistence and `buildJoyPairs`/`buildJoySettingsArray` if they remain pure serialization helpers.
3. `ControllerAssignment.kt`: assignment and conflict resolution helpers.
4. `ControllerPreviewCanvas.kt`: `DrawScope` drawing helpers for sticks, D-pad, buttons, triggers, and labels.
5. `ControllerPickerDialogs.kt`: button, stick, axis, D-pad, threshold, and generic motion bridge dialogs.
6. Leave `ControllerConfigPage.kt` as state orchestration and layout composition.

Touch editor recommended splits:
1. `TouchEditorGeometry.kt`: floating-zone edge encode/decode, selection resolution, hit testing, movement, collision detection, and sizing math.
2. `TouchEditorCanvas.kt`: grid/control drawing and preview drawing.
3. `TouchEditorPropertyPanels.kt`: stick, button, radial, slider, diagnostic, axis-region, and more-actions property panels.
4. `TouchEditorPickers.kt`: axis picker, segment binding picker, button binding picker, double-tap picker, curve picker, label editor.
5. `TouchEditorDialogs.kt`: preset picker, add control, global settings, gyro settings.
6. Leave `TouchEditorPage.kt` as page state, toolbar, bottom sheet, import/export launchers, and save/cancel orchestration.

Validation focus:
- Existing tests: controller config serialization, controller axis exponent, controller device selection, touch editor zone edge, touch overlay drag-zone policy.
- Manual smoke: configure controller, import/export controller config, edit touch layout, save layout, import layout, and launch with edited layout.

Expected payoff:
- Strong maintainability gain with relatively low architecture risk because both files already have section markers.
- Splits are mostly mechanical if done in the same package.

## Tranche 4 - Decompose MainActivity.kt carefully

Current size: 3,180 lines. It is large, but also sits at the native/JNI boundary, so refactor this after the launcher and overlay splits.

Do not start by moving JNI declarations wholesale. Many native calls and callbacks may rely on `MainActivity` method names or lookup paths.

Candidate extraction order:
1. `MainOverlayPolicy.kt`: keep moving small pure functions out first. Some visibility and input policy helpers are already top-level and covered by tests.
2. `MainOverlayController`: overlay view creation, overlay polling, admin tray action callbacks, standalone overlay show/hide, and overlay toast lines. Pass native operations as lambdas rather than moving native methods.
3. `GamepadInputRouter`: button/axis dispatch, D-pad mapping, gamepad edge tracking, IME rerouting helpers, and controller menu routing. Keep native joystick calls behind callbacks.
4. `MainAutomapGestures.kt`: automap pointer state and gesture math for the overlay-off path.
5. `SoftKeyboardBridge.kt`: `KeyboardInputView`, `GameSurfaceView`, `GameInputConnection`, keyboard height sampling, show/hide keyboard paths.
6. `MainDebugReceivers.kt`: introspection receiver, automation receiver, and game command receiver if callback injection stays simple.
7. `MusicOverlayController`: track polling, music panel display/dismiss, and track/level name overlays.

Validation focus:
- Existing unit tests: main activity input type, gamepad button dispatch policy, overlay visibility policy, settings child overlay controller, gamepad button edge tracker.
- Manual smoke: launch D1 and D2, touch overlay, gamepad buttons/axes, admin tray, soft keyboard text input, automap gestures, music overlay, automation broadcast.

Expected payoff:
- Meaningful, but higher risk than the Compose page extractions because it is lifecycle-heavy and native-facing.
- Do after `TouchOverlayView.kt`; moving overlay logic is easier once overlay policies are already isolated.

## Tranche 5 - Medium UI files with obvious section splits

Files:
- `AdvancedSettingsPage.kt`: 1,991 lines.
- `MusicPickerPage.kt`: 1,928 lines.
- `MultiplayerScreen.kt`: 910 lines.
- `LanDiscoveryTab.kt`: 859 lines.
- `VideoInfoOverlay.kt`: 968 lines.

Recommended work:
1. `AdvancedSettingsPage.kt`: split `DebugLoggingSection`, `CrashReportsSection`, `RecordedInputDemosSection`, `StorageInspectorSection`, `ImportLocationSection`, file transfer helpers, and dangerous-zone actions. This page has a lot of unrelated admin tools.
2. `MusicPickerPage.kt`: keep the page cohesive, but split import helpers, `AddToSetDialog`, MIDI section, CD section, audio files section, and preview/detail dialogs if music work resumes.
3. `MultiplayerScreen.kt`: extract coop autosave history/read/write helpers into `CoopAutosaveHistory.kt`; optionally split recent games UI and status log later.
4. `LanDiscoveryTab.kt`: split joined-lobby view, discovery view, LAN lobby card, IP join dialog, and IP address helpers only if LAN UI work continues.
5. `VideoInfoOverlay.kt`: extract layout math and controller-action policy first. Keep draw/input in one view unless the overlay grows again.

Validation focus:
- Existing tests for video info overlay layout, audio-source visibility/normalization/persistence, launcher file/copy helpers, and related music import tests.
- Manual smoke for advanced settings exports, debug logging toggles, crash report list, music import/preview, and multiplayer LAN tabs.

Expected payoff:
- Moderate. These are readable today, but splitting them will make future feature work less likely to touch thousand-line pages.

## Tranche 6 - Service and manager files

Files:
- `MatchmakingService.kt`: 1,171 lines.
- `LobbyService.kt`: 1,129 lines.
- `ModManager.kt`: 1,052 lines.

Recommended work:
1. `ModManager.kt`: split first if mod work continues. Candidate files are `ModManifest.kt` for data classes and manifest load/save, `ModImporter.kt` for import/install/delete/reorder, `ModCompatibility.kt` for base requirement checks and user-facing failure descriptions, and `ModPatchAnalysis.kt` for patch document parsing/conflict detection/generated overrides.
2. `MatchmakingService.kt`: split into WebSocket connection/auth/send APIs, message handling, STUN/connectivity/UPnP/proxy code, and game-state update polling. Keep the public singleton facade while moving internals behind collaborators.
3. `LobbyService.kt`: split only after adding/confirming LAN tests. Candidate files are UDP socket transport, packet encoding/decoding/handlers, hosted lobby state, joined lobby state, chat/kick handling, and stale lobby pruning.

Validation focus:
- `cargo test` is for the Rust server, but these Android files need JVM/instrumented tests or manual emulator tests.
- For `ModManager.kt`, existing mod manager tests are useful and should be expanded around patch conflicts before splitting.
- For networking services, prefer adding focused tests around packet/message parsing before moving code.

Expected payoff:
- `ModManager.kt` is a good medium-priority split because logic is pure and testable.
- `MatchmakingService.kt` and `LobbyService.kt` are worthwhile but higher risk because they manage sockets, coroutines, reconnects, and shared state.

## Watch list, but not first-priority splits
- `TouchControl.kt` at 690 lines is a large data model, but it is cohesive. Leave it unless touch-layout schema work resumes; then split enums/response math from data classes.
- `LauncherScriptExecutor.kt` at 614 lines is cohesive test automation. Consider splitting assertion helpers only if new assertion types keep accumulating.
- `AutoselectEditorPage.kt` at 612 lines is fine as a single page.
- `HumanReadableConfig.kt` at 604 lines is cohesive but could split touch-layout conversion from controller-config conversion if either side grows.
- `NetworkProtocol.kt`, `LocalhostProxy.kt`, `AudioSourceManager.kt`, `FileSetManager.kt`, `ConfigImportExport.kt`, `EnginePreferencesPage.kt`, `FingerprintBridge.kt`, `TouchBindings.kt`, `CrashLog.kt`, and `CustomAudioSetManager.kt` are all under about 550 lines and appear reasonable as-is.
- Existing test files are all under 350 lines. No test refactor is needed from size alone.

## Full size inventory and disposition

Action tags:
- `split-now`: top refactor target.
- `split-soon`: good candidate after higher-payoff files.
- `watch`: leave for now unless actively working in that area.
- `keep`: cohesive or small enough.

| Lines | Action | File |
| ---: | --- | --- |
| 9709 | split-now | android/app/src/main/java/com/dxxredux/app/SetupActivity.kt |
| 4591 | split-now | android/app/src/main/java/com/dxxredux/app/TouchOverlayView.kt |
| 3284 | split-soon | android/app/src/main/java/com/dxxredux/app/ControllerConfigPage.kt |
| 3250 | split-soon | android/app/src/main/java/com/dxxredux/app/TouchEditorPage.kt |
| 3180 | split-soon | android/app/src/main/java/com/dxxredux/app/MainActivity.kt |
| 1991 | split-soon | android/app/src/main/java/com/dxxredux/app/AdvancedSettingsPage.kt |
| 1928 | split-soon | android/app/src/main/java/com/dxxredux/app/MusicPickerPage.kt |
| 1171 | split-soon | android/app/src/main/java/com/dxxredux/app/multiplayer/MatchmakingService.kt |
| 1129 | split-soon | android/app/src/main/java/com/dxxredux/app/lobby/LobbyService.kt |
| 1052 | split-soon | android/app/src/main/java/com/dxxredux/app/ModManager.kt |
| 968 | watch | android/app/src/main/java/com/dxxredux/app/VideoInfoOverlay.kt |
| 910 | watch | android/app/src/main/java/com/dxxredux/app/multiplayer/MultiplayerScreen.kt |
| 859 | watch | android/app/src/main/java/com/dxxredux/app/multiplayer/LanDiscoveryTab.kt |
| 690 | keep | android/app/src/main/java/com/dxxredux/app/TouchControl.kt |
| 614 | watch | android/app/src/main/java/com/dxxredux/app/LauncherScriptExecutor.kt |
| 612 | keep | android/app/src/main/java/com/dxxredux/app/AutoselectEditorPage.kt |
| 604 | watch | android/app/src/main/java/com/dxxredux/app/HumanReadableConfig.kt |
| 545 | keep | android/app/src/main/java/com/dxxredux/app/multiplayer/NetworkProtocol.kt |
| 528 | keep | android/app/src/main/java/com/dxxredux/app/GraphicsSettingsPage.kt |
| 527 | keep | android/app/src/main/java/com/dxxredux/app/AudioSourceManager.kt |
| 519 | keep | android/app/src/main/java/com/dxxredux/app/multiplayer/LocalhostProxy.kt |
| 501 | keep | android/app/src/main/java/com/dxxredux/app/FileSetManager.kt |
| 482 | keep | android/app/src/main/java/com/dxxredux/app/ConfigImportExport.kt |
| 423 | keep | android/app/src/main/java/com/dxxredux/app/EnginePreferencesPage.kt |
| 409 | keep | android/app/src/main/java/com/dxxredux/app/FingerprintBridge.kt |
| 398 | keep | android/app/src/main/java/com/dxxredux/app/TouchBindings.kt |
| 373 | keep | android/app/src/main/java/com/dxxredux/app/multiplayer/MultiplayerStatsOverlay.kt |
| 348 | keep | android/app/src/main/java/com/dxxredux/app/CrashLog.kt |
| 348 | keep | android/app/src/main/java/com/dxxredux/app/CustomAudioSetManager.kt |
| 342 | keep | android/app/src/main/java/com/dxxredux/app/multiplayer/LobbyScreen.kt |
| 331 | keep | android/app/src/main/java/com/dxxredux/app/multiplayer/CreateGameDialog.kt |
| 331 | keep | android/app/src/main/java/com/dxxredux/app/ImportLocationManager.kt |
| 317 | keep | android/app/src/main/java/com/dxxredux/app/DiscImportBridge.kt |
| 311 | keep | android/app/src/main/java/com/dxxredux/app/DebugLog.kt |
| 310 | keep | android/app/src/main/java/com/dxxredux/app/multiplayer/UpnpClient.kt |
| 309 | keep | android/app/src/main/java/com/dxxredux/app/MusicControlPanel.kt |
| 276 | keep | android/app/src/main/java/com/dxxredux/app/CoopStatsOverlay.kt |
| 257 | keep | android/app/src/main/java/com/dxxredux/app/multiplayer/FriendsTab.kt |
| 251 | keep | android/app/src/main/java/com/dxxredux/app/SkipButtonView.kt |
| 248 | keep | android/app/src/main/java/com/dxxredux/app/lobby/LobbyProtocol.kt |
| 242 | keep | android/app/src/main/java/com/dxxredux/app/multiplayer/MissionPicker.kt |
| 234 | keep | android/app/src/main/java/com/dxxredux/app/multiplayer/NetworkEventsOverlay.kt |
| 233 | keep | android/app/src/main/java/com/dxxredux/app/InputDemoManager.kt |
| 229 | keep | android/app/src/main/java/com/dxxredux/app/multiplayer/IceProgressPanel.kt |
| 226 | keep | android/app/src/main/java/com/dxxredux/app/multiplayer/StunClient.kt |
| 220 | keep | android/app/src/main/java/com/dxxredux/app/ControllerLongPressDetector.kt |
| 199 | keep | android/app/src/main/java/com/dxxredux/app/AssetManifest.kt |
| 192 | keep | android/app/src/main/java/com/dxxredux/app/multiplayer/MatchmakingState.kt |
| 174 | keep | android/app/src/main/java/com/dxxredux/app/TouchLayoutRepository.kt |
| 168 | keep | android/app/src/main/java/com/dxxredux/app/WarpButtonOverlay.kt |
| 165 | keep | android/app/src/main/java/com/dxxredux/app/DiscIdentifier.kt |
| 156 | keep | android/app/src/main/java/com/dxxredux/app/GyroInputManager.kt |
| 156 | keep | android/app/src/main/java/com/dxxredux/app/DxaTextureScanner.kt |
| 154 | keep | android/app/src/main/java/com/dxxredux/app/multiplayer/NetworkEventsPanel.kt |
| 149 | keep | android/app/src/main/java/com/dxxredux/app/multiplayer/ConnectivityChecker.kt |
| 145 | keep | android/app/src/main/java/com/dxxredux/app/DpadFocusUtils.kt |
| 144 | keep | android/app/src/main/java/com/dxxredux/app/PendingResumeLaunch.kt |
| 144 | keep | android/app/src/main/java/com/dxxredux/app/LoadingProgressOverlayView.kt |
| 141 | keep | android/app/src/main/java/com/dxxredux/app/AcoustIdClient.kt |
| 133 | keep | android/app/src/main/java/com/dxxredux/app/CdAudioSourceNormalization.kt |
| 123 | keep | android/app/src/main/java/com/dxxredux/app/UpdateChecker.kt |
| 121 | keep | android/app/src/main/java/com/dxxredux/app/NativePilotPreferences.kt |
| 115 | keep | android/app/src/main/java/com/dxxredux/app/SafManifest.kt |
| 112 | keep | android/app/src/main/java/com/dxxredux/app/LauncherFileLabels.kt |
| 110 | keep | android/app/src/main/java/com/dxxredux/app/SafUriPermissions.kt |
| 110 | keep | android/app/src/main/java/com/dxxredux/app/multiplayer/TvButtons.kt |
| 109 | keep | android/app/src/main/java/com/dxxredux/app/CdPreviewBridge.kt |
| 109 | keep | android/app/src/main/java/com/dxxredux/app/TvButtons.kt |
| 107 | keep | android/app/src/main/java/com/dxxredux/app/DemoInstallerPackages.kt |
| 103 | keep | android/app/src/main/java/com/dxxredux/app/AcceptJoinButtonView.kt |
| 102 | keep | android/app/src/main/java/com/dxxredux/app/ResumeSaveBridge.kt |
| 99 | keep | android/app/src/main/java/com/dxxredux/app/StartGameButtonView.kt |
| 98 | keep | android/app/src/main/java/com/dxxredux/app/GogImportBridge.kt |
| 94 | keep | android/app/src/main/java/com/dxxredux/app/multiplayer/PlayGamesAuth.kt |
| 92 | keep | android/app/src/main/java/com/dxxredux/app/ExitButtonView.kt |
| 90 | keep | android/app/src/main/java/com/dxxredux/app/BinHexDecoder.kt |
| 87 | keep | android/app/src/main/java/com/dxxredux/app/ImportStorageGuard.kt |
| 85 | keep | android/app/src/main/java/com/dxxredux/app/multiplayer/ActiveGamesTab.kt |
| 84 | keep | android/app/src/main/java/com/dxxredux/app/LauncherFileCopy.kt |
| 83 | keep | android/app/src/main/java/com/dxxredux/app/multiplayer/ChatArea.kt |
| 77 | keep | android/app/src/main/java/com/dxxredux/app/InputMixer.kt |
| 76 | keep | android/app/src/main/java/com/dxxredux/app/KnownVersions.kt |
| 75 | keep | android/app/src/main/java/com/dxxredux/app/MidiPreviewBridge.kt |
| 73 | keep | android/app/src/main/java/com/dxxredux/app/ImportTreeScanner.kt |
| 67 | keep | android/app/src/main/java/com/dxxredux/app/multiplayer/MultiplayerForegroundService.kt |
| 67 | keep | android/app/src/main/java/com/dxxredux/app/multiplayer/RecentAddressPrefs.kt |
| 65 | keep | android/app/src/main/java/com/dxxredux/app/GameActivityState.kt |
| 65 | keep | android/app/src/main/java/com/dxxredux/app/WeaponState.kt |
| 64 | keep | android/app/src/main/java/com/dxxredux/app/NativeAutoselectPatcher.kt |
| 62 | keep | android/app/src/main/java/com/dxxredux/app/multiplayer/NetLog.kt |
| 59 | keep | android/app/src/main/java/com/dxxredux/app/NativePilotPatcher.kt |
| 54 | keep | android/app/src/main/java/com/dxxredux/app/DxxReduxApp.kt |
| 54 | keep | android/app/src/main/java/com/dxxredux/app/ControllerOverlayNavigation.kt |
| 49 | keep | android/app/src/main/java/com/dxxredux/app/AndroidGameFileExtensions.kt |
| 42 | keep | android/app/src/main/java/com/dxxredux/app/multiplayer/NetworkConstants.kt |
| 41 | keep | android/app/src/main/java/com/dxxredux/app/MidiEnumerationBridge.kt |
| 37 | keep | android/app/src/main/java/com/dxxredux/app/ControllerDisplayDevice.kt |
| 31 | keep | android/app/src/main/java/com/dxxredux/app/multiplayer/ClientIdentity.kt |
| 31 | keep | android/app/src/main/java/com/dxxredux/app/NativeTextureLookupCache.kt |
| 25 | keep | android/app/src/main/java/com/dxxredux/app/Json5.kt |
| 25 | keep | android/app/src/main/java/com/dxxredux/app/NativeMetaActions.kt |
| 25 | keep | android/app/src/main/java/com/dxxredux/app/ArchiveInputStreams.kt |
| 18 | keep | android/app/src/main/java/com/dxxredux/app/DebugLogCategory.kt |
| 17 | keep | android/app/src/main/java/com/dxxredux/app/ImportChooserConfig.kt |
| 15 | keep | android/app/src/main/java/com/dxxredux/app/GamepadButtonEdgeTracker.kt |
| 14 | keep | android/app/src/main/java/com/dxxredux/app/BuildInfo.kt |
| 13 | keep | android/app/src/main/java/com/dxxredux/app/LauncherDebugLog.kt |
| 12 | keep | android/app/src/main/java/com/dxxredux/app/TvDetection.kt |
| 5 | keep | android/app/src/main/java/com/dxxredux/app/GraphicsDebugPrefs.kt |
| 2 | keep | android/app/src/main/java/com/dxxredux/app/InputDemoPrefs.kt |

Test sources are all small enough to keep as-is from size alone. The largest is `ModManagerDetailsTest.kt` at 334 lines; most are under 150 lines.

## Recommended first work item
Start with `SetupActivity.kt` tranche 1 steps 1 through 4: game-file catalog, file/import helpers, config helpers, and disc import helpers. These are mostly top-level or pure helpers, reduce the largest file immediately, and avoid the highest-risk activity/receiver wiring until later.
