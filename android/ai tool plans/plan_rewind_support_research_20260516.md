# Rewind Support Research Plan - 2026-05-16

## Goal
Research a single-player rewind feature that stores about 60 seconds of 5 second restore points, exposes a new `rewind` binding or action for touch and gamepads, keeps playtime counters coherent, preserves the input-demo system where practical, and can be configured from game preferences with rewind support enable plus a 5s, 10s, or 20s target amount selector that defaults to 10s.

## Work Plan
- [x] Create this plan file before research work
- [x] Map save/load state coverage and cost in D1 and D2
- [x] Map timing/playtime state that must rewind with the snapshot
- [x] Map input binding, touch, and More menu action plumbing
- [x] Map input-demo recorder/replay behavior around a rewind action
- [x] Estimate memory and stutter risk for likely implementation choices
- [x] Recommend a shared D1/D2 implementation shape and test plan
- [x] Add preference and binding requirements to the implementation plan
- [x] Add demo consistency, rewind target, overlay, and anti-stutter notes

## Implementation Tranche
- [x] Wire `META_REWIND` through touch, gamepad, and More menu surfaces
- [x] Add rewind enable toggle in Game Preferences and pass it to native
- [x] Add shared native rewind API with status results and HUD feedback
- [x] Add D1/D2 pending-action consumption for rewind requests
- [x] Add initial rewind ring manager skeleton and level-reset behavior
- [x] Replace the temp-file DGSS bridge with a pure memory-backed capture/restore path
- [x] Generalize `rewind_file` into a cross-platform file abstraction and collapse Android-only helper signatures
- [x] Add input-demo truncation APIs for kept-path rewinds
- [x] Add overlay notification for successful rewinds
- [x] Add focused tests for binding and target selection
- [x] Add focused recorder truncation test coverage

## Rewind Demo Desync Follow-Up
- [x] Reproduce or inspect the level 10 rewind demo one-frame drift
- [x] Add a focused host test for record, rewind truncate, continue, and stop timing
- [x] Fix the remaining one-frame timeline mismatch without special rewind commands in demos
- [x] Re-run focused host tests and Android build

## Coop Host Rewind Planning
- [x] Inspect current rewind request or capture code and coop restore touchpoints
- [x] Split rewind target preview from restore so UI can announce before load begins
- [x] Add host-only coop eligibility checks and non-host feedback
- [x] Reuse or extract the existing multiplayer restore sync path for host rewinds
- [x] Add focused unit coverage for coop eligibility and request policy
- [ ] Add two-emulator automation coverage

### Coop Host Scope
- Keep this Android-only for now. Single-player behavior should stay unchanged
- Allow rewind in cooperative multiplayer only when `Game_mode & GM_MULTI_COOP` and `multi_i_am_master()` are true
- Keep deathmatch, team, bounty, robo-anarchy, observers-as-host, endlevel sequence, destroyed control center, demo playback, and non-host coop blocked
- Non-host coop button presses should not touch rewind state. Show a local fading overlay line such as `Not host` and return a dedicated status like `ANDROID_REWIND_STATUS_NOT_HOST`
- Host coop capture should use the same boundary timing as single-player capture, but only while the host is authoritative, all active players are alive, object sync is idle, and the level identity still matches. If master or host changes, reset the ring and start fresh

### Coop Restore Sync Plan
- Reuse the DGSS memory snapshot as the host's source of truth. Coop metadata is already written by the normal state serializer when `GM_MULTI_COOP` is active, so do not add a second coop-specific snapshot format
- Before the host restores, run the same safety gates used by multiplayer load: `multi_i_am_master()`, `multi_all_players_alive()`, no duplicate callsigns, no endlevel sequence, no destroyed control center, and no observing host
- Extract the non-file-specific synchronization setup from `multi_restore_game()` into a helper shared by file restore and rewind restore. The helper should strip robot ownership as needed, put remote playing players into `CONNECT_WAITING`, reset observer targeting if needed, and prepare normal host-to-client object sync
- Broadcast a small Android coop rewind begin packet before the host restore, or reuse an existing restore packet only if the current missing-file fallback reliably causes clients to enter the same waiting or sync state. The packet should carry rounded seconds so clients can show `Rewinding N seconds`, clear local controls, and wait for host sync without attempting to restore their own snapshot
- After `state_restore_from_memory()` succeeds on the host, finish through the same path as multiplayer restore: send restored scores and let normal multiplayer synchronization repopulate clients. If testing shows normal object sync is insufficient after an in-memory host restore, add a second phase that transfers a compact restore payload or writes a transient coop autosave only on button press
- Keep input-demo truncation single-player only unless a later multiplayer demo recorder explicitly needs it. Coop rewind should not encode a rewind command in demos

### Rewind Overlay Timing Plan
- Replace the post-restore success overlay with a pre-restore overlay. Once a target snapshot is selected, compute the rounded seconds immediately and show `Rewinding N seconds` before `state_restore_from_memory()` starts
- Remove `Rewound N seconds` after restore. Restore success should be silent after loading finishes except for debug logging
- If restore fails after the pre-restore message, keep the existing HUD failure message such as `Rewind failed`; do not add a second overlay success or failure line unless automation needs it
- Keep blocked or unavailable attempts as immediate local feedback: `Not host` as a fading overlay for non-host coop, `No rewind point yet` for an empty ring, `Rewind is disabled` for the preference gate, and a specific multiplayer-blocked HUD or overlay line for unsupported multiplayer modes

### Coop Test Plan
- Add host-side unit tests around a pure eligibility helper: single-player allowed, coop host allowed, coop client returns not-host, non-coop multiplayer blocked, observing host blocked, and sync-in-progress blocked
- Extend rewind policy tests or add a small Android-native host test for pre-restore message ordering: selected target computes `Rewinding N seconds`, request performs no post-restore `Rewound` overlay
- Add a two-emulator automation script where the client presses rewind and introspection sees a `Not host` overlay line with no level, time, or position reset
- Add a two-emulator automation script where the host waits for at least one rewind point, moves, presses rewind, and introspection confirms both host and client return to the earlier level time and position after sync
- Validate with D1 and D2 because both have duplicated game loop and multiplayer restore code. Run the focused host tests, `run-windows-build.ps1 -Target both`, Android `assembleDebug`, code quality for touched Android files, and the two-emulator scripts before marking this tranche complete

## Rewind Amount Preference Planning
- [x] Inspect current Game Preferences rewind wiring in `EnginePreferencesPage.kt` and `MainActivity.applyRewindPref()`
- [x] Add a rewind amount radio group gated by the rewind support toggle
- [x] Persist the selected rewind amount and push it to native on launch and preference refresh
- [x] Update rewind target-selection tests for 5s, 10s, and 20s amounts

### Rewind Amount UI Plan
- Keep the existing `Enable rewind support` switch in the Gameplay section of Game Preferences
- When rewind support is enabled, show a radio-button group directly below it with three choices: `5s`, `10s`, and `20s`
- Hide or disable the radio group when rewind support is off so the dependency is obvious and the screen stays compact
- Default the selected option to `10s` for missing, stale, or invalid saved values
- Reuse the existing Compose `RadioButton` row style already present in `EnginePreferencesPage.kt` so the control matches the rest of the settings UI

### Rewind Amount Runtime Plan
- Store the selected option in `dxx_prefs` alongside `PREF_REWIND_SUPPORT_ENABLED`, using a new key such as `PREF_REWIND_TARGET_SECONDS = "rewind_target_seconds"`
- Extend `MainActivity.applyRewindPref()` to send both values to native, either through a second JNI setter like `nativeSetRewindTargetSeconds(int seconds)` or a combined rewind-settings setter if that keeps the JNI surface cleaner
- Native should sanitize the configured value to the supported set `{5, 10, 20}` and fall back to `10` on invalid input so old installs and edited prefs remain safe
- Apply the selected amount to both single-player rewind and future coop-host rewind so one preference controls the rewind distance everywhere rewind is allowed
- Keep the existing 5 second snapshot capture cadence and 60 second ring size for now. The selected value should therefore control target selection, not capture cadence or retention depth
- Because snapshots are still quantized to 5 second points, the actual rewind may overshoot the selected amount by up to just under 5 seconds. The displayed `Rewinding N seconds` text should continue to use the actual selected snapshot delta, not the configured label

## Notes
- Keep any future source edits minimal in `d1/` and `d2/`
- Prefer reusing existing save/load serialization if it is efficient enough
- Rewind is single-player only for the initial feature; the next planned expansion is cooperative host-only rewind
- The memory-backed rewind path now uses a shared `rewind_file` abstraction so D1 and D2 can keep reusing the normal DGSS serializer without temp `.rwtmp` files, while public helper signatures stay free of Android-only `#ifdef` splits
- Focused rewind coverage now lives in `RemainingKeyTouchActionsTest` for Android binding exposure and `test_android_rewind_policy` for the 3 second target-selection rule, demo-timeline filtering, and rewind or continue boundary frame count

## Research Summary

### Recommendation
Use the existing DGSS save/load serializer as the first rewind backend, but route it to a memory-backed stream and disable thumbnail or metadata work for rewind snapshots. A raw memcpy snapshot of all game state is not a good first option because the useful state is spread across global arrays, linked object lists, AI/path runtime state, RNG streams, game tick state, sound/object links, UI/window state, and platform resources. The save path already serializes the semantic state and rebuilds fragile links on restore.

The feature should be a 12-entry ring buffer: 60 seconds divided by 5 second points. Each point stores serialized state bytes plus small metadata: player total time, player level time, current level, captured `GameTime64`, input-demo recorder frame count, RNG trace event count if recording, and a validity flag. On rewind, choose the first point at least the configured rewind amount older than the current in-level time, restore it, discard points newer than the restored point, flush inputs, then suppress new captures until 5 seconds of post-restore player time have elapsed. Rewind support should default to enabled, and Game Preferences should also expose a rewind amount radio group with 5s, 10s, and 20s choices that defaults to 10s.

### Rewind Target Selection
- Game Preferences should let the player choose a rewind amount of 5s, 10s, or 20s. This selected amount becomes the target age threshold for the next rewind press, and defaults to 10s
- Rewind should not always choose the newest point. On button press, scan valid points from newest to oldest and pick the first point whose captured player level time is at least the configured rewind amount older than the current player level time
- Because capture points are still spaced every 5 seconds, the actual rewind distance is quantized. Example: with points at 5 and 10 seconds and current time 12.99, the 10 second point is only 2.99 seconds old, so a 5s, 10s, or 20s target all fall back to the 5 second point and the user rewinds about 7.99 seconds
- If only one older point remains, allow restoring it even when it is less than the configured target amount old. This handles repeated rewinds down to the beginning of the retained history
- Exclude the current restored point itself from the next target. After restoring a point, keep it as the current anchor if useful for scheduling, but only older points are candidates for the next rewind press
- Compute the user-facing rewind amount from the same fixed-point player level time values used for target selection. Round to the nearest whole second for display

### Save/Load State Coverage
- `d1/main/state.c` and `d2/main/state.c` already save player state, weapons, object list, walls, doors, triggers, segment texture/wall fields, fuel centers, control center state, AI state, automap state, RNG state, d_tick state, object allocator state, laser/weapon runtime state, morph/stuck-object/effect runtime state, and Android save metadata
- Player `time_level`, `time_total`, `hours_level`, and `hours_total` are already saved and restored through `player_rw`
- D2 has extra state for exploding walls, cloaking walls, markers, afterburner, last super weapon selection, palette flash, light subtraction, secret level flags, omega charge, guided missiles, and afterburner runtime state
- Normal saves intentionally write saved `GameTime64` as zero and store time-sensitive fields as deltas from the current `GameTime64`. Keep that for disk saves, but rewind restore should use the captured absolute `GameTime64` from the ring slot as the restore base. This is important for demo recording: the live kept path after a rewind must run with the same timeline time that a later replay will reach by simulating continuously from the original checkpoint
- `ThisLevelTime` is only incremented under `NETWORK` when multiplayer play-time limits are active, so it is not relevant to the first single-player rewind feature

### Save/Load Integration Details
- The existing disk API is filename-oriented: `state_save_all_sub()` opens a `PHYSFSX_openWriteBuffered()` file, writes a DGSS header and payload, then appends Android metadata; `state_restore_all_sub()` opens a `PHYSFSX_openReadBuffered()` file, validates the DGSS header or version, reloads the mission and level, calls `StartNewLevelSub()`, then rebuilds the runtime state
- The first implementation should not create a parallel rewind serializer. Split the current save and restore bodies into stream-oriented helpers, then leave the current filename wrappers as compatibility callers. A concrete shape would be `state_save_all_to_stream(fp, desc, flags)` and `state_restore_all_from_stream(fp, secret_restore, flags)`, or a tiny local `state_io` adapter if `PHYSFS_file` cannot reasonably be memory-backed
- Disk save wrappers keep the current behavior. Rewind capture calls the same stream helper with flags such as `STATE_SAVE_REWIND`, `STATE_SAVE_NO_THUMBNAIL`, and `STATE_SAVE_NO_ANDROID_METADATA`. Rewind restore calls the same restore helper with `secret_restore = 0`, a rewind flag to suppress normal load UI and disk-only diagnostics, and a restore base `GameTime64` supplied by the rewind slot metadata
- The save stream should keep the DGSS payload layout close to normal saves. That preserves save coverage and reduces D1/D2 drift. The rewind flags should only skip expensive or irrelevant outer work: screenshot thumbnail generation, Android launcher save metadata trailers, coop metadata, message boxes, and filename-based validation
- Player playtime should rewind automatically because `player_rw` already serializes `time_level`, `time_total`, `hours_level`, and `hours_total`. After restore, the rewind manager should schedule the next capture from the restored player time, not from wall-clock time or the old pre-rewind `GameTime64`
- Keep the rewind ring outside normal save slots. The user should not see rewind points in launcher save lists, and autosave or manual save metadata should not be emitted for them

### Demo Consistency State Checklist
- Treat the input-demo requirement as a determinism audit for rewind saves. If the post-rewind live game can diverge from a later replay of the kept input path, the fix should be to save more game state in the rewind/DGSS path, not to encode rewind operations in the demo
- The save payload already covers many high-risk simulation inputs: player state and playtime, objects, wall/door/trigger/segment state, fuel centers, control center state, AI state, automap visited flags, main RNG state, FX RNG state and call count, d_tick state, object allocator/free list/signature seed, homer cadence, weapon runtime state, morph state, stuck-object state, control-center runtime state, afterburner runtime state, effect runtime state, and D2-only lighting/marker/omega/guided-missile style state
- Rewind metadata must add the timeline and recorder pieces that are outside normal saves: captured `GameTime64`, captured player level and total times for target selection and display, `input_demo_recorder_frame_count()`, and `input_demo_rng_trace_event_count()`
- Add or verify save coverage for globals that were previously handled only by input-demo checkpoint metadata. A known candidate is `Collision_delay_last_play_time`: the recorder stores it for the initial checkpoint, but a mid-level rewind point needs the current value restored too, otherwise later collision sound delay logic can call RNG at different frames
- Audit other `GameTime64`-relative or RNG-affecting globals that are not clearly in `ai_save_state()` or `state_write_runtime_state()`. Examples to verify include scrape-sound cooldowns, thief/buddy/boss hit timers, boss gate/teleport timers, and any Android-only gameplay helpers. Use per-frame state traces to prove they match instead of special-casing the demo system
- Rewind while recording should restore the captured `GameTime64`, then let all saved time deltas rebase from that value. If it restores to zero like a normal manual load, the live kept path can differ from final replay because the final demo replay does not include the rewind load event
- Do not truncate the input-demo recorder until the rewind restore succeeds. If restore fails, leave the current recording intact and report the failure
- After a successful restore, truncate recorder vectors and RNG trace events to the counts stored in the rewind point, clear pending pulses and pending frame events, flush current controls, and only then accept new inputs for the next frame
- The overlay message for rewind should not be recorded as a demo command or frame event. It is UI feedback only; if it is added to `overlay_ringbuf`, that is for introspection and automation visibility

### Current Cost Risks
- Disk saves use `PHYSFSX_openWriteBuffered` with a 1 MB PhysFS buffer, then flush on close. Writing to Android private flash every 5 seconds is the main stutter risk if the existing file path is used directly
- D2 normal save thumbnail generation renders a thumbnail and reads GL pixels unless `g_android_save_blank_thumbnail` is set. Rewind must use a no-thumbnail mode
- The save loop allocates and frees one `object_rw` per object and one `player_rw` per player. With up to 1000 objects this is probably tolerable every 5 seconds, but it is avoidable churn. A rewind path should use stack or reusable scratch buffers
- Restoring will still have a visible pause because it calls `StartNewLevelSub`, rebuilds objects and links, and may touch rendering or sound state. That happens only on button press, not every 5 seconds

### Efficiency And Anti-Stutter Notes
- The first anti-stutter rule is no disk and no GL readback during periodic capture. Rewind points should be written to a preallocated memory stream, skip thumbnail rendering, skip Android save metadata, and avoid JNI or UI calls on capture
- Preallocate the ring metadata and slot buffers. Use 12 slots, plus optional scratch/current buffers, with an initial per-slot capacity based on a conservative estimate or the previous level's high-water snapshot size. Grow only when a snapshot does not fit, and keep the enlarged capacity until level reset
- After the first snapshot in a level establishes a real byte size, optionally reserve the remaining slot buffers one per frame over the next few frames. That spreads allocation cost without streaming an inconsistent snapshot across frames
- Keep serializer scratch allocations out of the hot path. Replace per-object `CALLOC`/`d_free` loops with stack or reusable `object_rw`/`player_rw` scratch structs for rewind and, if safe, for normal saves too
- Memory-stream writes should be append-only pointer copies with bounds checks. Avoid per-field virtual dispatch, `std::vector` growth during capture, and compression. Compression is acceptable only for final demo files or diagnostics outside gameplay
- Do not shrink recorder vectors or RNG trace storage on rewind truncation. Resize counts down but retain capacity so repeated rewind and re-record cycles do not allocate every time
- A single rewind snapshot should remain a consistent point-in-time capture. Do not serialize one logical snapshot across multiple simulation frames unless a stable copy of every changing source has already been taken. Otherwise the snapshot can combine state from different frames
- Later optimizations can stream updates into cached sections between captures, but this should be a second phase. Reasonable dirty/cached candidates are automap visited flags, light/segment side changes, walls/doors/triggers, fuel-center state, and maybe per-object serialized rows. These need explicit dirty marking at mutation sites, so they are higher-risk than the memory-backed full snapshot
- The most practical phase-1 streamed work is allocation and cache warming, not partial snapshot serialization: reserve buffers gradually, keep reusable scratch memory hot, and measure capture duration before adding complex delta tracking
- Add timing diagnostics for capture and restore from the start. Log snapshot byte count, allocation growth, total capture time, largest section time if section timing is cheap, restore time, and skipped-capture reason

### Memory Estimate
- `object_rw` is roughly 260 bytes and `MAX_OBJECTS` is 1000, so the object block is about 260 KB at maximum occupancy
- Segment tmap or wall fields are 36 bytes per segment. Typical original levels are near 900 segments, about 32 KB. The expanded `MAX_SEGMENTS` path can be about 324 KB for this piece
- Thumbnail bytes are 15 KB and should be skipped or blanked for rewind
- With AI state, runtime state, walls, triggers, fuel centers, automap/light arrays, and metadata, a typical uncompressed in-memory point should likely be a few hundred KB to under 1 MB. A conservative 12-point ring should usually be in the 6 to 12 MB range. A high-water preallocation around 12 to 24 MB is a reasonable Android tradeoff
- Do not zlib-compress live rewind points. Compression can be done for final input-demo files, but live compression every 5 seconds risks more stutter than it saves

### Input-Demo Handling
- The current input-demo recorder keeps captured frames in vectors and writes the file only on flush. That is the right structure for preserving a continuous kept path across rewinds
- Do not store a rewind command in the demo. A rewind is an editor-like operation on the in-memory recording session: it deletes abandoned future frames, restores game state locally, then lets recording continue from the restored point
- Each rewind point should store `input_demo_recorder_frame_count()` and `input_demo_rng_trace_event_count()` at the same moment the state bytes are captured. These counts identify the last kept point in the recording timeline
- Add `input_demo_recorder_truncate(frame_count)`. It should resize all frame-aligned vectors together: `control_frames`, `rng_frames`, `has_state_frames`, `state_frames`, `has_diag_frames`, `diag_frames`, and `frame_events`. It should clear `pending_pulse` and `pending_frame_events`, because those belong to input that happened after the restored point or has not been committed to a frame yet
- Add `input_demo_rng_trace_truncate(event_count)`. It can reduce the active trace count without freeing capacity, then clear current object context. This keeps the sidecar trace aligned with the kept demo frames
- The recorder's initial checkpoint data should remain the original demo start checkpoint. Rewind snapshots are not embedded as additional checkpoints. On final flush, the demo remains: original start state plus the frames that survived all rewinds
- Capture point timing must align with recorder timing. Rewind snapshots now run at a true frame boundary after `calc_frame_time()` and before `calc_game_time()` or `input_demo_record_game_frame()`. The stored recorder frame count is therefore the kept timeline count directly, and replay resumes from the next frame without a one-frame `GameTime64` drift
- Tests should assert that after recording frames A, rewinding to a point before frames B, then recording frames C, the flushed demo contains A plus C only. No rewind marker, no stale RNG events from B, and no frame number gaps

### Rewind Overlay Message
- The existing pop-open overlay is the multi-line overlay in `MainActivity.showOverlayLine()`. JNI entry points `showTrackName()` and `showLevelName()` both delegate to it
- Native code reaches it through `android_jni_overlay.c`: `android_send_track_name()` calls `showTrackName`, and `android_send_level_name()` calls `showLevelName`. `track_names.c` formats track and level messages, sends them to Java, and also records them in `overlay_ringbuf` for introspection
- Add a generic overlay helper, for example `android_send_overlay_line(const char *text)`, or add a small rewind-specific wrapper if keeping the Java method names explicit is preferred. The generic helper should call a public Kotlin method that delegates to `showOverlayLine()`
- Add `rewind_overlay_notify(int seconds)` in shared Android C/C++ code. Format `Rewinding %d seconds`, using the nearest whole second from the actual rewind delta. Consider singular `Rewinding 1 second` if the UI polish is worth the branch
- Add `overlay_ringbuf_add("rewind", text)` so automation and introspection can verify the message without screenshots
- Trigger the overlay after target selection and before restore starts. Disabled, unavailable, or blocked rewind attempts should continue using HUD messages or local overlay feedback. Successful restore should not add a second post-restore `Rewound` line

### Preferences, Bindings And More Menu
- `TouchBindings.kt` is the Kotlin source for visible labels and meta action IDs. `android_meta_actions.h` duplicates those IDs for C
- `android_meta_actions.c` dispatches most meta actions by SDL key injection, but special actions can set a volatile flag for the game thread. Rewind should be a flag, not a fake key sequence
- Add `META_REWIND` after the existing meta IDs in both Kotlin and C, then consume it in D1 and D2 game thread code through a new `android_rewind_pending` flag
- Add `META_REWIND` to `META_BUTTON_LABELS` with a label such as `Rewind`. This makes it available to touch button pickers and the gamepad/controller configuration UI because both derive extra action labels from the same map
- Add `META_REWIND` to `remainingBaseActionBindings` in `TouchOverlayView.kt` so the More menu exposes it when it is not already configured in the current touch layout. The existing `touchLayoutBoundActionBindings()` filtering should then hide it once the player assigns it to a touch button, radial segment, d-pad, or stick button mode
- Gamepad bindings already serialize meta action IDs into `controller_config.json` under `meta_bindings`, and `MainActivity.loadMetaBindings()` routes those IDs through `dispatchMetaAction()`. The same `META_REWIND` ID should therefore cover controller face buttons, d-pad bindings, and axis-as-button bindings
- Add `PREF_REWIND_SUPPORT_ENABLED = "rewind_support_enabled"` to `EnginePreferencesPage.kt`, defaulting to true from `dxx_prefs`. Put it in a local gameplay options section on the Game Preferences page, not in pilot-backed preferences, because it controls Android runtime behavior rather than pilot file data
- Add `PREF_REWIND_TARGET_SECONDS = "rewind_target_seconds"` to `EnginePreferencesPage.kt`, defaulting to `10` from `dxx_prefs`, and show it as a 5s, 10s, or 20s radio group only while rewind support is enabled
- `MainActivity.applyRewindPref()` should pass both rewind preferences to native code on game startup and when preferences are refreshed, for example through small JNI setters that update volatile native `android_rewind_enabled` and `android_rewind_target_seconds` values. Native should default to enabled and `10` seconds so old installs behave as expected before the first preference write
- If rewind is disabled and the player presses a bound touch, More menu, or gamepad action, the game-thread consumer should print a HUD message such as `Rewind is disabled` and return without restoring or capturing. Keeping the binding active avoids confusing users who intentionally leave the control mapped for later
- If rewind is disabled, the rewind manager should stop taking new snapshots and free or invalidate the ring buffer to save memory. Re-enabling should start fresh from the next eligible in-level capture point
- If rewind is enabled, target selection should use the configured 5s, 10s, or 20s amount immediately without requiring a restart or level reload

### Proposed Implementation Shape
1. Add shared rewind manager code under `android/app/src/main/cpp/shared/`, compiled into both games. It owns the ring buffer, capture cadence, saved metadata, enable flag, target amount, and public calls such as `android_rewind_reset_level()`, `android_rewind_update()`, `android_rewind_request()`, `android_rewind_set_enabled()`, and `android_rewind_set_target_seconds()`
2. Add a small `android_rewind_state.h` interface for the game-specific save bridge. D1 and D2 each implement wrappers such as `state_android_rewind_capture(buffer, size)` and `state_android_rewind_restore(buffer, size)` by calling their local state serializer. This keeps the big duplicated save code in D1 and D2, while the ring policy stays shared
3. Refactor `state_save_all_sub()` and `state_restore_all_sub()` in D1 and D2 into filename wrappers around stream helpers. Keep disk save behavior byte-for-byte compatible where possible. The rewind helper should still write/read the DGSS header and normal payload, but should skip thumbnail rendering and Android save metadata
4. If PhysFS cannot provide a writable memory-backed `PHYSFS_file` in the pinned version, add a tiny local stream abstraction with read, write, seek, tell, length, and close operations. Implement PhysFS and memory versions, then mechanically replace direct `PHYSFS_*` calls only inside the extracted save/restore helpers. Avoid broader source churn
5. Implement the ring as fixed slot metadata plus preallocated growable byte buffers. Reuse slot buffers on overwrite to avoid 5 second malloc/free churn. Store frame count, RNG trace count, player time counters, current level, captured `GameTime64`, and a short reason or status for diagnostics
6. Implement target selection as nearest valid point at least the configured 5s, 10s, or 20s target amount older than current player level time, with the single-remaining-point exception. After restore, discard newer points and schedule the next capture for 5 seconds after the restored time
7. Hook lifecycle in both games. Reset the ring on new game, level transition, mission change, player death, secret level transition, demo playback, multiplayer start, and return to menu. Call `android_rewind_update(FrameTime)` only during active single-player in-level gameplay, not while paused, in menus, during movies, during endlevel sequencing, or after control-center countdown starts
8. Hook input. Add `META_REWIND` constants to `TouchBindings.kt` and `android_meta_actions.h`; add `volatile int android_rewind_pending` and `volatile int android_rewind_enabled` in `android_meta_actions.c`; set pending only on button down. In D1 and D2 `android_handle_ingame_saveload_request()` or an adjacent game-thread handler, consume the pending flag before normal key handling and call the rewind manager
9. Add preference wiring. Keep the Game Preferences switch with default enabled, add a rewind amount radio group with 5s, 10s, or 20s choices defaulting to 10s, store both in `dxx_prefs`, send changes to native, and have native apply them immediately. A disabled press should show the HUD message and should not alter input-demo state
10. Add touch and gamepad binding coverage. Add the label to `META_BUTTON_LABELS`, include it in touch More menu candidates, and rely on the existing controller `meta_bindings` path for gamepad. Verify export/import through `HumanReadableConfig` still round-trips the label
11. Add demo maintenance APIs. Add `input_demo_recorder_truncate(frame_count)` and `input_demo_rng_trace_truncate(event_count)`, with tests that prove all frame-aligned vectors and trace events stay in sync. On rewind restore, truncate after restore succeeds, before the next frame capture, and before accepting new direct command events
12. Add overlay and HUD feedback. Once a target is selected, send a pop-open overlay line such as `Rewinding 8 seconds`, then restore. If no point exists, show `No rewind point yet`. If disabled, show `Rewind is disabled`. If blocked by multiplayer, death, menu, endlevel, countdown state, or non-host coop, prefer a specific HUD or local overlay message during button press and no repeated messages during passive update. Do not show a second post-restore `Rewound` line
13. Add diagnostics behind Android debug logging. Log capture size, allocation growth, capture duration, restore duration, skipped capture reason, target selection, rounded display seconds, ring slot count, and input-demo truncation counts under `DLOG_GAME` or a new lightweight category if this gets noisy
14. Add tests and validation, then run `android\run-code-quality.ps1 --fix`, host tests, and the relevant Android automation test before considering implementation complete

### Test Plan
- Add host/unit tests for the memory stream and ring buffer wrap or pop behavior
- Add input-demo recorder tests for truncating frames, frame events, pending pulses, RNG frames, and per-frame state vectors
- Add rewind target-selection tests covering the 5s, 10s, and 20s thresholds, the 7.99 second style skip caused by 5 second capture spacing, repeated rewinds, and the single-remaining-point exception
- Add overlay helper tests or automation checks that verify a selected rewind adds a `rewind` entry to `overlay_ringbuf` with `Rewinding N seconds` before restore and no post-restore `Rewound` line
- Add Engine Preferences tests or Compose/UI checks that verify the 5s, 10s, or 20s radio group only appears while rewind support is enabled, defaults to 10s, and persists changes into `dxx_prefs`
- Add a high-level Android automation test that starts single-player, waits for at least two capture points, triggers rewind, and verifies via introspection that player time and position move backward
- Add a demo-recording automation test if feasible: record inputs, trigger rewind, continue, flush, then replay and verify the resulting demo has a continuous kept timeline
