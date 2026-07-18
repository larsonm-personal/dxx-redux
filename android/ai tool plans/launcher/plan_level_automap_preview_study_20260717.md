# Level Automap Preview Study - 2026-07-17

## Goal

Study a launcher metadata-browser action that starts a fast, native C++ level automap preview without entering normal gameplay. The preview should reuse normal automap touch controls, show only the relevant preview menu actions, place the player at a normal level start, and return directly to the launcher when the map closes.

## Scope

- [x] Trace launcher mission and level metadata UI, launch intents, and native startup arguments
- [x] Trace D1 and D2 level startup and identify the smallest reliable initialization boundary
- [x] Trace automap rendering, touch overlay policy, and menu dispatch
- [x] Determine how preview mode should select a normal player start and handle secrets/objectives
- [x] Compare full-process, reduced-runtime, and separate-native-library designs for startup speed and maintenance cost
- [x] Define lifecycle, error handling, cleanup, and return-to-launcher behavior
- [x] Define an implementation sequence and focused verification strategy
- [x] Record conclusions and mark this initial study complete

## Constraints

- Keep detailed mission and level parsing in native code
- Reuse the C++ game engine automap implementation rather than reproducing it in Kotlin
- Keep D1 and D2 behavior aligned
- Preserve desktop builds and avoid broad changes in `d1/` and `d2/`
- Keep the launcher separable enough to support other games later
- Favor low perceived launch latency and deterministic cleanup over gameplay-state compatibility

## Initial Questions

1. Can the existing Android game activity be started directly into a preview state, skipping menus, briefing, sound, AI, and the gameplay loop?
2. Does automap require a fully initialized level, object array, player object, textures, palette, and renderer, or can some subsystems remain uninitialized?
3. Can the existing touch overlay distinguish preview automap from gameplay automap without duplicating control handling?
4. Which existing metadata identifies the mission, archive, game variant, level number, and objective or secret overlays?
5. Is process reuse safe and worthwhile, or is a short-lived native preview activity/process the cleaner first implementation?

## Findings

### Executive conclusion

The feature fits the current architecture and does not need to enter normal gameplay. The lowest-risk first implementation is a dedicated `LevelPreviewActivity` host with D1 and D2 manifest variants in separate short-lived processes. Each process should load the existing game shared library but call a new preview-only native entry point. That entry point initializes graphics, game data, the selected mission and level, one player start, route metadata, and the automap window. It should not initialize or enter the pilot, title, briefing, sound, music, AI, network, save, cockpit, or gameplay paths.

This distinction is important: the first version can avoid running the entire engine, but it will still load the existing monolithic D1 or D2 shared object. Producing a genuinely small preview-only shared object is possible in principle, but the current C source graph and global state make that a significantly larger maintenance project. It should be considered only if profiling shows shared-library loading and relocation, rather than level data loading, is a meaningful part of visible latency.

### Existing launcher seam

- `LevelMetadataTarget` and `LevelMetadataLevelRow` in `LevelMetadata.kt` already provide the game variant, base data directory, source type and path, mission descriptor, HOG files, selected level number, selected level filename, and archive entries needed to identify a preview.
- `LevelMetadataTargets` already handles installed missions, direct level and HOG files, mission ZIPs, and persistently extracted ZIP missions. Preview should reuse this target model rather than derive a second mission-resolution scheme.
- `LevelMetadataAnalyzer` already serializes a normalized `dxx-level-metadata-request-v1` request and prepares archive entries for native loading. The request construction and target preparation should be extracted into a shared launcher component so metadata analysis and preview cannot drift.
- The natural UI entry point is the selected level detail dialog in `SetupSections.kt`. Thread the owning `LevelMetadataTarget` through `LevelMetadataResultContent`, `LevelMetadataTable`, and `LevelMetadataLevelDialog`, then show `Preview map` for a successfully scanned row. Failed or missing rows should not offer preview.
- The checked-in `game_data/mission_files/*.json` files are regression data, not the launcher's runtime source. Preview must use the live target and selected row.
- `MissionZipExtractionStore` already provides persistent extracted mission files and can turn an archive mission into a `LevelMetadataTarget`. Its freshness check currently validates archive length and extracted file lengths, although it also records the owner modification time. Before preview depends on this cache, compare `ownerLastModifiedMs` as well, or use a stronger identity when available. Reusing a fresh extraction is preferable to making a new per-preview staging copy.

The preview request should be a small file under a request-specific cache directory, with only the request path and game variant sent through the intent. Add selected-level fields to the shared request representation instead of passing the full metadata result through Android extras. This keeps large archive entry lists out of Binder and gives native code one stable input contract.

### Existing reduced native runtime

`jni_level_metadata.cpp` is a strong proof of concept. Its `init_levelmeta_runtime()` already initializes a useful native subset, mounts base and mission data, loads configuration and game data, creates the required object state, and avoids normal `main()`. Its `scan_level()` calls `load_level()` and the secret-area scanner without starting gameplay.

It cannot be reused as a running preview process unchanged because it deliberately creates a dummy in-memory screen and does not own a visible SDL/EGL surface or automap event loop. A preview initializer should share helpers and request parsing with it, but must use the real Android graphics/input initialization path.

The conservative first native boundary is:

1. Initialize memory, arguments, error handling, PhysFS, the base HOG, text, and read-only player/config values needed by automap controls.
2. Disable sound and music before platform initialization, initialize SDL input and the real EGL-backed graphics surface, choose the normal game video mode, and load palette and fonts.
3. Mount only the selected mission sources and load mission metadata.
4. Initialize game data and object storage needed by `load_level()` and automap. Keep `texmerge_init()` initially because the metadata runtime already relies on the conservative data setup; remove it only after D1/D2 profiling and tests prove it unnecessary.
5. Load the one selected level directly. Do not call `StartNewGame`, `StartNewLevelSub`, or any equivalent gameplay sequence.
6. Initialize the first normal single-player start, scan secrets and the objective route, create an inert backing game window, and call `do_automap()`.
7. Process events only while the automap window is active. Close the backing window, release preview-owned resources, return from JNI, finish the Activity, and terminate only the dedicated preview process.

The complete `inferno.c` path initializes titles, movies, pilots, menus, audio, and the general window loop. The new entry point should be adjacent to that path, not a flag that lets normal startup advance and later diverts into automap. An early, explicit preview runner is easier to audit for accidental game subsystem startup.

### Automap and player state

- D1 and D2 `do_automap()` already implement the native camera, draw loop, keyboard, joystick, mouse, touch-fed virtual input, close behavior, and player marker.
- The automap expects valid `Players[Player_num].objnum`, `ConsoleObject`, `Viewer`, player ship/model data, loaded segments and objects, configuration, and a `Game_wind` to hide and restore. A tiny inert backing window is less invasive than teaching both automap implementations that no game window exists.
- In non-multiplayer mode, `gameseq_init_network_players()` walks loaded start objects in object order and populates player slot zero from the first normal `OBJ_PLAYER` or `OBJ_GHOST`; cooperative-only starts are not chosen. Preview can reuse this behavior, then explicitly set `Player_num = 0`, `N_players = 1`, `ConsoleObject`, and `Viewer`, and call `InitPlayerPosition(0)` if its remaining preconditions are satisfied. If linking that function brings unwanted gameplay dependencies, extract the start-object initialization into a small shared D1/D2 helper and test it against the existing sequence.
- A preview should set `PLAYER_FLAGS_MAP_ALL` for the preview player. Otherwise the normal visited-segment rules produce an almost empty map at a new level start, which defeats the preview purpose. This reveals level topology but does not enable the separate secret annotations by default.
- Run `secret_area_rescan_current_level()` and `level_metadata_rescan_route_from_object(player_objnum)` after choosing the start. The existing `automap_metadata_overlay.c` code already renders secret and objective labels and connectors from this native scan. The launcher metadata JSON does not need to be injected into the renderer.
- Initial preview state should be secrets hidden and objectives off. `Show secrets` toggles the existing secret reveal state. `Show objectives` can retain the existing Off, All, Remaining, and Next cycle, although in a non-playing preview Remaining initially equals All.
- `do_automap()` unconditionally restores `Game_wind` when it closes. The native preview runner should therefore own an inert backing window, call `do_automap()`, and run `event_process()` only while `Automap_active` and the preview windows exist. The map key, Escape/Android Back, or a close action will then end the map and naturally return control to the preview runner.

### Touch controls and reduced menu

`TouchOverlayView`, `AutomapTouchPolicy`, and the existing MainActivity callbacks already supply nearly all required behavior. Automap mode keeps the map button and movement controls, routes pan/rotate/zoom through the usual virtual input, and exposes native recenter, secret reveal, and objective mode calls.

Add an explicit preview policy rather than filtering actions ad hoc in the Activity:

- Keep the normal automap button and movement/axis controls.
- Keep recenter because it is a normal map navigation action.
- Omit D2 marker creation, naming, and jump actions because preview state is transient.
- Make the preview settings/admin tray return only `Show secrets` and `Show objectives`.
- Provide `Close preview` as a navigation action in the map action sheet, in addition to the map button and Android Back. It is not a persistent game setting, so the only two settings remain the two requested toggles.

The last distinction resolves an otherwise contradictory UI requirement: a menu containing literally only the two display toggles cannot also close the preview through the menu. Treat `Close preview` as the sheet's navigation row, with secrets and objectives as its only settings. All three close paths should dispatch the same map-close input and must not invoke autosave, abort-game confirmation, or a game main menu.

A minimal preview Activity should host the same `SurfaceView`, `TouchOverlayView`, input mixer, and touch layout used by the game, but it should not inherit MainActivity's multiplayer, audio, save, cockpit, gameplay polling, or game activity-state behavior. Small reusable surface/input plumbing can be extracted from MainActivity if needed.

### Process and lifecycle boundary

`MainActivity` runs in `:game`, records `GameActivityState`, and deliberately kills that process after the native `main()` returns because engine globals do not support a second lifecycle. The metadata workers similarly use separate `:levelmeta_d1` and `:levelmeta_d2` processes because a process can safely load only one game variant.

Use two manifest components, such as `LevelPreviewD1Activity` in `:levelpreview_d1` and `LevelPreviewD2Activity` in `:levelpreview_d2`, backed by a common Activity implementation. Do not use the launcher process, `:game`, or either metadata worker. This allows a preview to open while a paused game remains returnable, prevents the preview from overwriting `GameActivityState`, and isolates D1 from D2 native globals.

The launcher stays underneath the fullscreen preview Activity. Use a no-animation transition and no title or splash. Show a neutral loading surface immediately, adding text or a progress indicator only if loading passes a short threshold. When the native loop returns, call `finish()` and kill the preview process as the current game JNI wrapper does. Process death is deterministic cleanup for global engine state and makes each preview independent.

Use an Activity result for expected initialization failures, containing a short user-facing reason. The launcher can reopen the level detail dialog and show the error. A native crash will still return to the launcher because it is a separate process; add a launcher-side in-flight marker so `onResume` can distinguish an unexplained preview death from a normal close. Clear request/cache directories on normal completion and prune stale request directories on launcher startup.

Do not keep the native preview process warm in the first version. Warm reuse complicates D1/D2 state reset, mission unmounting, renderer recreation, and coexistence with normal gameplay. Persistent archive extraction, OS file cache, skipped media/audio/gameplay initialization, and a transition-free Activity should deliver most of the perceived latency benefit. Measure before adding a warm process.

### Design options

| Option | Startup and APK characteristics | Maintenance and correctness | Decision |
| --- | --- | --- | --- |
| Divert normal game startup into automap | Reuses the installed library but still initializes too much | Entangles preview with pilot, menu, gameplay, save, and `:game` lifecycle | Reject |
| Preview-only runner in existing D1/D2 libraries | Loads the full library, then executes only the required native subset | Reuses the authoritative loader, renderer, overlays, and input with modest D1/D2 glue | Recommended first version |
| New slim D1/D2 preview libraries | May reduce loading and relocation, but can duplicate code and increase APK size | Current global/source coupling makes source selection and stubbing fragile | Profile-gated follow-up |
| Parse and draw the map in Kotlin | Could avoid native process startup | Duplicates engine behavior and violates the native automap requirement | Reject |

### Performance budget and measurements

Instrument these milestones for both a cold and immediate repeat preview: launcher click, Activity created, library loaded, native request parsed, mission mounted, game data ready, level loaded, automap first frame, and launcher visible after close. Also record archive staging bytes and peak RSS.

Prioritize work based on the measured critical path:

- Reuse a fresh persistent mission ZIP extraction. Fix its modification-time validation first.
- Mount only the active base data and selected mission files.
- Skip movies, titles, briefings, pilot enumeration, audio/music devices, AI, textures used only by cockpit/game rendering, save setup, network, and gameplay windows.
- Avoid launcher configuration writes and enabled-mod selection changes. Preview is read-only and must not alter what the next normal game launch will use.
- Avoid a new slim library unless cold `System.loadLibrary()` plus relocations is material after the reduced runner exists. The full library is already in the APK and its file pages may benefit from the OS page cache.

## Recommendation

Implement the feature in four independently testable tranches.

### Tranche 1: shared request and launcher action

1. Extract metadata request construction and target preparation from `LevelMetadataAnalyzer` into a shared `LevelContentRequest` builder.
2. Add selected level number, filename, and secret-level status to a preview request schema. Keep metadata analysis compatible with its current schema.
3. Reuse `MissionZipExtractionStore` for imported archives and fix freshness validation to include the recorded owner modification time.
4. Thread the metadata target into the level detail dialog and add `Preview map` for successful rows.
5. Add the two preview Activity manifest components, result handling, request cleanup, and transition behavior. Do not modify normal launch selection or `GameActivityState`.

### Tranche 2: minimal native preview runner

1. Add a JNI entry point shared by the two preview Activity variants and reuse the metadata runtime's request parsing and mission mounting helpers.
2. Bring up silent SDL/EGL graphics and the conservative minimum game-data initialization for D1 and D2.
3. Load only the selected level, initialize the first normal player start, grant full-map visibility, rescan secrets and route objectives, and open automap over an inert backing window.
4. Exit the event loop when automap closes and terminate the dedicated process without saves or game-menu transitions.
5. Add timing checkpoints before attempting subsystem removal.

### Tranche 3: preview touch policy

1. Extract the reusable SurfaceView, input mixer, and touch-overlay setup needed by the preview host.
2. Add a preview flag or policy object to `TouchOverlayView` and `adminTrayVisibleActions()`.
3. Keep normal automap navigation and recenter, hide marker and gameplay actions, expose only secrets and objectives as settings, and expose one `Close preview` navigation row.
4. Route the map button, Back, and close row through one idempotent native close operation.

### Tranche 4: performance and hardening

1. Run cold/repeat D1 and D2 measurements for installed missions, direct HOG/level files, and mission ZIPs.
2. Remove conservative initializers only when tests demonstrate they are unnecessary in both games.
3. Add stale request pruning and preview-crash recovery messaging.
4. Consider separate slim libraries only if the library-load milestone is a significant portion of the target latency and the APK-size tradeoff is acceptable.

### Suggested ownership boundaries

- Launcher: target selection, persistent archive preparation, request lifecycle, error presentation, and Activity transition.
- Preview Activity: visible surface, existing touch/input bridge, preview-only action policy, and finish behavior.
- Shared Android native bridge: request parsing, Android lifecycle callbacks, timing, and process exit.
- D1/D2 preview runner: engine-specific initialization order, level/player setup, automap window, and cleanup.
- Existing shared metadata/overlay code: secret scan, route scan, and automap annotations.

## Verification Record

This tranche was an architecture study only. No production code or build graph was changed.

The study traced the live metadata UI and target builder, native metadata worker, ZIP extraction cache, normal Activity/JNI lifecycle, D1 and D2 startup and player sequencing, both automap implementations, shared metadata overlays, touch action policies, and existing automation/introspection coverage.

Implementation verification should include:

- JVM tests for target-to-preview-request serialization, selected normal and secret levels, preview menu filtering, all close actions, and archive freshness invalidation.
- Native host or headless tests for selecting the first normal start, rejecting a level with no normal start, granting map visibility without secret reveal, and scanning objectives from the chosen start.
- D1 and D2 Android automation based on `test_level_metadata_launcher_d2_obsidian.json5`, `test_launch_to_automap.json5`, and `test_secret_reveal_automap_d2.json5`.
- Preview introspection fields for preview mode, requested level, player object and segment, automap active state, full-map state, secret reveal state, objective mode, and timing milestones. Existing automap overlay introspection can remain authoritative for rendered secret/objective counts.
- An end-to-end case that opens metadata, selects a level, launches preview, exercises pan/rotate/zoom/recenter, toggles secrets and objectives, closes with the map button, and verifies `SetupActivity` is foreground with no preview process.
- Equivalent close cases for Android Back and `Close preview`, including repeated close input.
- A coexistence case proving that a paused `:game` process remains returnable and its `GameActivityState` is unchanged after a preview.
- Error cases for missing base data, missing/corrupt level, stale archive extraction, native initialization failure, process death, and rotation/background/foreground transitions during loading.
- Cold and repeat startup measurements for D1 and D2, with an explicit first-frame target chosen after baseline measurement rather than guessed in advance.

Document review completed on 2026-07-17. The plan is ASCII-only, has no trailing whitespace, and is the only workspace change. No build or runtime test was warranted because this tranche changes documentation only.

## Implementation Progress - 2026-07-17

### Filesystem and cache invariants

- [x] Preview treats base game data, imported archives, extracted mission files, pilots, saves, configuration, and active mod selections as read-only
- [x] Preview requests live under a dedicated cache root and use request-specific directories
- [x] Request creation is atomic, request paths are canonicalized, and native code receives no writable game-data path
- [x] A preview process never owns or deletes a persistent mission extraction
- [x] Normal completion deletes only its validated request directory; startup pruning removes only stale children of the dedicated preview cache root
- [x] Archive extraction freshness includes archive size and modification time, plus extracted-file presence and size
- [x] Preview does not reuse the metadata worker's mutable dummy-screen state or the normal game's global state
- [x] Preview cannot write pilot/config/save files or change the launcher's active path file

### Initial implementation tranche

- [x] Add and test cache-safe preview request creation and cleanup
- [x] Tighten persistent mission extraction freshness and add regression coverage
- [x] Thread the metadata target through the level detail UI and add the preview launch action
- [x] Add isolated D1/D2 preview Activity components without touching `GameActivityState`
- [x] Add a preview-specific touch/admin action policy and unit coverage
- [x] Add the native preview entry point and reduced D1/D2 runner
- [x] Build, lint, and run focused JVM and Android integration verification

### Initial implementation verification

- Focused JVM tests pass for owned cache deletion and pruning, atomic request publication, request validation, archive modification-time invalidation, preview automap actions, and preview admin filtering.
- The debug native build passes for arm64-v8a, armeabi-v7a, and x86_64 with both D1 and D2 libraries.
- The debug APK assembles and installs on the emulator.
- D2 Obsidian level 1 opens in the preview Activity and closes to the existing metadata dialog with both Android Back and the visible MAP touch button.
- D1 built-in level 1 opens in the preview Activity and closes to the existing metadata dialog with the visible MAP touch button.
- Clean close removes the request-specific preview directory. Emulator SHA-256 checks confirm that D1/D2 active-set files, the D2 active-mod file, and both games' configuration files are unchanged. Persistent route-cache file listings are also unchanged.
- The first D2 device run found and fixed a flattened extracted-mission mount issue before final verification. No native crash appears in the final D1 or D2 runs.
