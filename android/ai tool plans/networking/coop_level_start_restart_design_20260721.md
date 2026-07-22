# Cooperative level-start restart design

## Goal

Design a host-only shortcut that restarts a cooperative multiplayer level from a retained beginning-of-level save, preserving the party equipment recorded at that point. The retained save must survive ordinary gameplay saves and be replaced only after the party enters another level.

## Plan

- [x] Trace D1 and D2 multiplayer save/restore, level entry, host authority, and overlay game-menu paths
- [x] Define lifecycle, storage, synchronization, failure handling, and user experience
- [x] Identify minimal paired D1/D2 changes and shared Android-facing changes
- [x] Define integration and multiplayer regression coverage
- [x] Record the recommended design and mark this design tranche complete

## Scope

Design only. No game source implementation or build is planned in this tranche.

## Existing code findings

- The Android bottom-center `Settings` admin tray is the requested overlay menu. It already exposes Save, Load, and Game Menu shortcuts through `AdminTrayPolicy.kt`, `TouchOverlayView.kt`, and `MainActivity.kt`.
- The shared Android rewind manager captures full in-memory game saves on the host. Cooperative rewind then uses `multi_save_transfer.c` to distribute an authoritative save buffer to connected clients.
- Ordinary rewind history is not suitable for this feature. It rotates every five seconds, is cleared when a player dies, depends on the rewind preference, and rejects a restore unless every player is alive.
- Periodic cooperative autosaves are disk-backed, rotate through slots 5 through 9, and intentionally represent current progress rather than the beginning of the level.
- The first rewind capture happens before the first simulation frame. However, cooperative carried inventory can be applied later by `coop_load_progress_inventory()` inside the first multiplayer frame. A level-start checkpoint captured at the existing rewind hook can therefore be too early.
- In-memory saves currently omit the cooperative metadata trailer. Android cooperative restore remapping expects that trailer, especially for host-owned save buffers sent to clients. The level-restart implementation should close this gap instead of depending on player slot order.
- The touch overlay is normally hidden while the local player is dead. Without a small policy extension, a dead host could not invoke the restart shortcut.

## Recommended behavior

### User experience

- Add `Restart Level` to the Settings admin tray only when native state reports all of the following:
  - a cooperative multiplayer game is active
  - the local player is the current host
  - a valid level-start checkpoint exists for the active mission and level
  - no save transfer or level transition is already active
- Selecting it opens a short confirmation: `Restart from the beginning of this level? Current level progress will be lost.`
- Confirming closes the tray and queues the restart on the game thread. The button is disabled while the transfer is running.
- Show concise HUD results to the whole party: `Restarting level from checkpoint`, `Level restarted`, or a specific failure such as `Level-start checkpoint unavailable` or `Party sync failed`.
- If the host is dead and a restart checkpoint is available, keep a restart-only Settings surface accessible. Do not reactivate flight controls or other gameplay buttons in this state.
- Clients never see the shortcut. This first version does not add client requests or a lobby option.

### Checkpoint lifecycle

Keep one dedicated pinned checkpoint in memory, separate from the rotating rewind snapshots and disk autosave slots.

The checkpoint identity is:

- game variant, already implicit in the process
- mission filename
- signed level number, including secret levels
- the save buffer and its size/capacity
- saved game time and collision timing overrides needed by the existing memory restore path
- a capture generation and status for diagnostics

Use this state machine:

1. `EMPTY`: no usable checkpoint.
2. `WAITING_FOR_LEVEL_READY`: mission or level identity changed naturally and the host is waiting for cooperative startup work to finish.
3. `READY`: one successful snapshot is pinned. Gameplay, deaths, manual saves, periodic autosaves, and restarts of the same identity do not replace it.
4. `RESTARTING`: the pinned buffer is being distributed and applied. Duplicate requests are rejected.
5. Return to `READY` after success or a recoverable transfer failure. The same buffer remains available for repeated retries.

Replacement rules:

- Replace only after a successful natural transition to a different mission or signed level number.
- A transition to a D2 secret level and a return from it each count as a new level identity and receive a new checkpoint.
- Restoring the pinned checkpoint invokes `StartNewLevelSub`, but must not arm a new capture because the identity is unchanged and the manager is in `RESTARTING`.
- Leaving cooperative gameplay, host migration away from the local process, or aborting to the main menu clears the checkpoint.
- If a manual or startup save restore changes the active identity to a mid-level state, mark that identity ineligible rather than calling it a level-start checkpoint. Capture resumes at the next natural level transition. A same-identity manual restore leaves the existing pinned checkpoint intact.
- Keep the checkpoint independent of the user's rewind enabled preference.

### Capture point

Do not capture at the existing pre-simulation rewind hook.

Add a cooperative level-ready hook immediately after the host's first completed `coop_load_progress_inventory()` attempt in `multi_do_frame()`, after auto-restore startup has either completed or been ruled out. At this point level synchronization and carried-equipment restoration have run, while player movement and weapon processing for that frame have not yet occurred.

The capture function retries on later frames until these conditions hold:

- local process is host of a cooperative game and is not an observer
- mission and nonzero level identity are stable
- startup auto-restore is no longer pending
- all currently connected players have valid player objects and are alive at the initial point
- callsigns are unique
- no save transfer or endlevel sequence is active

After one successful capture, never recapture that identity. A failed allocation or save logs the failure and retries without discarding a previous identity's buffer until the new checkpoint succeeds. This gives replacement transactional semantics.

### Save format and party identity

Use `state_save_to_memory()` with a new checkpoint description such as `LEVEL START`. Do not reserve a user slot or reuse slots 5 through 9.

Before relying on this path, make cooperative metadata available to memory saves:

- refactor cooperative metadata construction in `coop_save.c` into one shared builder
- keep the current PHYSFS trailer writer for disk saves
- add a `rewind_file` trailer writer used by memory saves
- have the existing state restore reader consume that trailer unchanged

This preserves the current source of truth for matching saved players by stable client ID, then callsign, and also carries D2 Guide-Bot ownership metadata. It benefits cooperative rewind as well as level restart.

Party changes after capture use the existing cooperative restore policy:

- players present at level start recover their saved equipment even if player slots changed
- a later joiner not present in the checkpoint spawns fresh and receives an explanatory HUD message
- a disconnected saved player remains represented in cooperative absent-player metadata for a later return
- duplicate live callsigns block restart because identity mapping would be ambiguous

### Host-authoritative synchronization

Extend the existing shared save-transfer protocol instead of creating new packets in D1 and D2:

- add transfer kind `LEVEL_START_RESTART`
- expose a shared API that queues an in-memory buffer without first restoring the host
- use the existing size cap, checksum, reliable begin/chunk/apply flow, timeout suspension, and per-peer readiness masks
- clients validate the complete checksum, acknowledge apply readiness, call `multi_prepare_restore_sync()`, then restore with `state_restore_coop_from_memory()`
- after all required clients acknowledge apply readiness, the host restores its still-pinned buffer with the same cooperative memory restore path
- refresh peer timeout timestamps and send score/state updates after restore

Unlike the current rewind flow, do not restore the host before the transfer has been accepted and copied. If queuing or client preparation fails, the host stays in the current world and the pinned checkpoint remains usable.

Restart eligibility deliberately differs from rewind eligibility:

- require host, cooperative mode, a matching checkpoint, unique identities, and reliable packet delivery
- do not require all players to be alive at request time
- allow restart during the reactor countdown while the game is still on the same level
- reject once `Endlevel_sequence` has committed the transition or the active level identity no longer matches

## Proposed interfaces

New shared module `coop_level_restart.c/.h`:

```c
void coop_level_restart_note_natural_level(int level_num);
void coop_level_restart_maybe_capture_ready(void);
void coop_level_restart_note_restore_begin(void);
void coop_level_restart_note_restore_end(int restored);
int coop_level_restart_get_state(void);
int coop_level_restart_request(void);
const rewind_memory_buffer *coop_level_restart_buffer(void);
void coop_level_restart_transfer_finished(int restored);
void coop_level_restart_clear(void);
```

Use a small shared state enum for JNI and tests, for example `HIDDEN`, `CAPTURING`, `READY`, `BUSY`, and `BLOCKED`. Kotlin should use only this coarse state and must not reproduce game rules.

Android UI and dispatch additions:

- `META_COOP_RESTART_LEVEL` in the C and Kotlin duplicated meta-action constants
- one pending flag consumed by `android_process_pending_game_actions()`
- `nativeGetCoopLevelRestartState()` for tray visibility/enabled state
- `ADMIN_RESTART_LEVEL` in `TouchOverlayView`
- `canShowCoopLevelRestart` input to `adminTrayVisibleActions()`

Native code must revalidate every rule when consuming the request. Kotlin visibility is convenience, not authority.

## Expected file impact

Shared Android code:

- new `android/app/src/main/cpp/shared/coop/coop_level_restart.c` and `.h`
- `android_rewind.c/.h` only if timing override helpers are generalized; do not put checkpoint lifecycle into rotating rewind history
- `multi_save_transfer.c` and D1/D2 `multi.h` declarations
- `coop_save.c/.h` and `state_android_shared.c` for memory cooperative metadata
- `android_meta_actions.c/.h`, `android_input.c`, and native build source lists

Paired upstream-tree hooks:

- D1 and D2 `multi.c`: call the capture-ready hook beside `coop_load_progress_inventory()` and dispatch the generalized transfer APIs
- D1 and D2 `gameseq.c`: identify natural level transitions separately from restore-driven `StartNewLevelSub`
- D1 and D2 `game.c` or the shared pending-action consumer only if needed for the game-thread request

Kotlin:

- `AdminTrayPolicy.kt`
- `TouchOverlayView.kt`
- `MainActivity.kt`
- `TouchBindings.kt` only if the action is also made bindable; the tray shortcut itself does not require a general binding

Keep all D1 and D2 edits under Android guards and matched in style.

## Test design

### Host policy tests

Add a pure C policy test covering:

- capture only after natural level readiness
- no overwrite on later frames, player death, same-level restart, manual save, or periodic autosave
- replacement on normal, secret, and mission identity changes
- host-only request
- request allowed with dead party members
- duplicate callsign, observer host, endlevel, missing checkpoint, mismatched identity, and active transfer rejection
- retry after allocation/capture/transfer failure retains the old valid buffer until replacement succeeds

### Save metadata tests

Extend host tests to round-trip cooperative metadata through a memory `rewind_file`, including client IDs, callsigns, absent players, and D2 Guide-Bot ownership fields.

### Kotlin tests

Extend `AdminTrayUiTest` and overlay visibility tests:

- Restart Level appears only for a ready cooperative host
- it is absent for clients, non-coop multiplayer, single player, preview, automap, and no-checkpoint states
- it disables or disappears while busy
- dead-host restart-only mode does not expose flight controls
- activation closes the tray and opens confirmation

### Two-emulator integration tests

Create maintained D1 and D2 automation scripts that:

1. Host and client begin a cooperative level with distinct carried equipment.
2. Verify the checkpoint becomes ready after inventory restoration.
3. Consume ammunition, collect equipment, alter the mine, and kill at least one party member.
4. Invoke Restart Level as host.
5. Verify both emulators stay connected, return to the same level start positions, restore their own initial equipment, and restore mine/object state.
6. Restart a second time and verify the same original equipment, proving the checkpoint was not overwritten.
7. Advance to the next level, verify a new checkpoint identity, change equipment, restart, and verify the new level's initial equipment.

Add focused cases for reactor countdown, a late joiner, transfer timeout/disconnect, and host migration. For the first implementation, host migration may explicitly clear the checkpoint and hide the action; transferring the pinned checkpoint during migration can be a later enhancement.

Use introspection fields for checkpoint state, identity, capture generation, and transfer status. Do not validate through screenshots.

## Non-goals for the first implementation

- persistence across process death or relaunch
- client-initiated restart requests
- non-cooperative multiplayer
- desktop UI exposure outside Android
- checkpoint transfer during host migration
- changing the existing rotating autosave history

## Implementation order

1. Fix cooperative metadata trailers for memory saves and add their round-trip test.
2. Add the pinned checkpoint manager and pure lifecycle policy tests.
3. Add transfer kind 2 and host-last application semantics.
4. Add matched D1/D2 natural-transition and capture-ready hooks.
5. Add JNI state/request access and the Settings tray action, confirmation, and dead-host mode.
6. Extend introspection and run the D1/D2 two-emulator integration tests.
7. Run scoped code quality, Windows host builds and CTest for D1/D2, Android native build, Kotlin unit tests, and the integration scripts.
