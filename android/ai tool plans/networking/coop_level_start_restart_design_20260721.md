# Cooperative level-start restart design

## Goal

Design a host-only shortcut that restarts a cooperative multiplayer level from a retained beginning-of-level save, preserving the party equipment recorded at that point. The live retained save must survive ordinary gameplay saves and be replaced only after the party enters another level. A durable copy of the highest achieved mainline level-start must also be offered when hosting later cooperative games.

## Plan

- [x] Trace D1 and D2 multiplayer save/restore, level entry, host authority, and overlay game-menu paths
- [x] Define lifecycle, storage, synchronization, failure handling, and user experience
- [x] Identify minimal paired D1/D2 changes and shared Android-facing changes
- [x] Define integration and multiplayer regression coverage
- [x] Record the recommended design and mark this design tranche complete

### Highest-level retained checkpoint amendment

- [x] Trace lobby save discovery, eligibility, and highest-level progress records
- [x] Add durable checkpoint lifecycle and new-game picker behavior
- [x] Extend implementation impact and regression coverage

## Scope

The original tranche produced the design. The core Android implementation is now complete for D1 and D2: session-pinned host restart, synchronized cooperative restore, durable highest mainline level-start retention, and Create Game restore selection.

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
- Clients never see the in-game shortcut. This first version does not add client restart requests; the new-game retained-checkpoint offer is host-side.

### Checkpoint lifecycle

Keep one dedicated pinned checkpoint in memory, separate from the rotating rewind snapshots and disk autosave slots. Also retain a durable copy of the highest mainline level-start reached for each mission so it can be offered when hosting a later game.

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
- Leaving cooperative gameplay, host migration away from the local process, or aborting to the main menu clears the live checkpoint. It does not delete the durable highest-level checkpoint.
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

Use `state_save_to_memory()` with a new checkpoint description such as `LEVEL START`. Do not reserve a user slot or reuse slots 5 through 9. Persist the exact successfully captured buffer through a dedicated checkpoint writer, so the live restart and later new-game restore represent the same frame.

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

### Durable highest-level checkpoint

The durable checkpoint is related to, but distinct from, the live checkpoint:

- Keep one durable highest mainline level-start checkpoint per game variant and mission.
- Compare positive mainline level numbers. A newly captured level replaces the durable file only when its level is greater than the recorded level for that mission.
- A same-level capture may repair a missing or corrupt durable file, but does not normally overwrite a valid file. This preserves the original beginning-of-level state.
- Secret levels remain valid live restart checkpoints but do not replace the durable highest mainline checkpoint. Returning from a secret level can replace it only if the returned positive level is higher than the stored mainline level.
- Replaying or manually starting a lower level never lowers the retained high-water mark.
- Loading the retained checkpoint in a new session does not rewrite it. Reaching the next higher level creates its replacement.
- Scope the durable artifact to the cooperative save set and mission, not to the current callsign. Participant metadata still determines how equipment is mapped and whether it should be offered to the local installation.

Use a dedicated engine-owned path under the existing cooperative save-set directory, plus a normalized manifest such as `coop_level_start_checkpoints.json`. The C implementation owns path construction, save validation, and manifest writing. Kotlin reads only the normalized manifest and treats the checkpoint ID as opaque.

Each manifest entry should include:

- stable checkpoint ID
- game and mission identity
- level number and level name
- capture timestamp
- participant callsigns and client IDs
- player count, total score, and difficulty metadata used by the picker
- file size, checksum, and format version for validation
- type `level_start_highest`

Write replacement transactionally: write and close a temporary file, reopen it to validate the save header, identity, size, and checksum, replace the durable file, then atomically update the manifest. If any step fails, preserve the previous valid highest checkpoint. On discovery, omit corrupt or missing entries and log the reason.

### New-game save picker

The existing create-game dialog currently combines rotating `full_save` entries with a non-restorable progress `checkpoint` marker. Add the durable level-start entry as a third, actually restorable type:

- Display it as `[Start] L<n> - Highest level start`, including party names and capture age.
- Offer it only for cooperative mode, the selected game and mission, a valid positive level, and when the local installation's client ID appears in its participant metadata.
- Keep it visible alongside rotating full saves. Do not silently replace or hide the newest autosave.
- Selecting it sets the hosted level to its level and records its opaque checkpoint ID as the restore source.
- Do not auto-select it ahead of the existing most-recent `full_save`; it is an explicit retained alternative. If no full save exists, it may become the default restorable selection.
- The existing progress-only `checkpoint` row remains non-restorable and continues to mean start fresh at the next unlocked level.

The current `coop_restore_slot.txt` handoff can represent only slots 0 through 9. Replace the Android-only handoff with a normalized `coop_restore_selection.json` union:

```json
{
  "kind": "level_start_highest",
  "checkpoint_id": "opaque-engine-id"
}
```

Other forms are `{"kind":"slot","slot":7}` and `{"kind":"fresh"}`. The C engine reads and deletes this one-shot selection, resolves checkpoint IDs through its own manifest, and rejects path text supplied by Kotlin. This avoids reserving slot 10, colliding with manual or autosave slots, or teaching Kotlin engine filename rules.

For a retained-checkpoint launch, the host starts the selected level normally, waits for multiplayer synchronization, then uses the same authoritative buffer transfer as an ordinary cooperative restore. The restored save becomes the live current-level checkpoint without being recaptured or relabeled. Clients who were in the saved party recover their beginning equipment; later or replacement players spawn fresh under the existing metadata policy.

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
int coop_level_restart_persist_if_highest(void);
int coop_level_restart_resolve_retained(const char *checkpoint_id,
                                        rewind_memory_buffer *buffer);
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
- Android save-set path helpers plus a retained-checkpoint manifest reader/writer owned by C
- `android_meta_actions.c/.h`, `android_input.c`, and native build source lists

Paired upstream-tree hooks:

- D1 and D2 `multi.c`: call the capture-ready hook beside `coop_load_progress_inventory()` and dispatch the generalized transfer APIs
- D1 and D2 `gameseq.c`: identify natural level transitions separately from restore-driven `StartNewLevelSub`
- D1 and D2 `game.c` or the shared pending-action consumer only if needed for the game-thread request

Kotlin:

- `AdminTrayPolicy.kt`
- `TouchOverlayView.kt`
- `MainActivity.kt`
- `MultiplayerScreen.kt`, `CreateGameDialog.kt`, `LobbyScreen.kt`, and `LanDiscoveryTab.kt` for retained-save discovery and typed restore selection
- `MultiplayerResumePrefs.kt` for persisting the typed selection in host-resume records
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
- durable high-water replacement for levels 1, 2, and replayed level 1
- same-level preservation, missing-file repair, corrupt candidate rejection, and transactional failure preservation
- secret-level capture without replacing the highest positive mainline checkpoint
- independent retained checkpoints for different missions

### Save metadata tests

Extend host tests to round-trip cooperative metadata through a memory `rewind_file`, including client IDs, callsigns, absent players, and D2 Guide-Bot ownership fields.

### Kotlin tests

Extend `AdminTrayUiTest` and overlay visibility tests:

- Restart Level appears only for a ready cooperative host
- it is absent for clients, non-coop multiplayer, single player, preview, automap, and no-checkpoint states
- it disables or disappears while busy
- dead-host restart-only mode does not expose flight controls
- activation closes the tray and opens confirmation

Extend multiplayer picker tests:

- parse and display `level_start_highest` beside rotating full saves
- filter it by game, mission, positive level, and local client ID
- select it through the opaque checkpoint ID without exposing a file path
- preserve newest full-save default selection, with retained checkpoint fallback when no full save exists
- selecting it updates the hosted level and writes the typed one-shot restore selection
- progress-only checkpoints remain non-restorable
- missing, corrupt, stale-manifest, and checksum-mismatched retained files are not offered

### Two-emulator integration tests

Create maintained D1 and D2 automation scripts that:

1. Host and client begin a cooperative level with distinct carried equipment.
2. Verify the checkpoint becomes ready after inventory restoration.
3. Consume ammunition, collect equipment, alter the mine, and kill at least one party member.
4. Invoke Restart Level as host.
5. Verify both emulators stay connected, return to the same level start positions, restore their own initial equipment, and restore mine/object state.
6. Restart a second time and verify the same original equipment, proving the checkpoint was not overwritten.
7. Advance to the next level, verify a new checkpoint identity, change equipment, restart, and verify the new level's initial equipment.
8. Exit to the launcher, create a new cooperative game, and verify the highest level-start appears alongside rotating saves.
9. Select it, launch with changed player slot ordering, and verify matching players receive the retained beginning equipment on both emulators.
10. Replay a lower level and verify the higher retained offer remains unchanged; then reach a higher level and verify it replaces the offer.

Add focused cases for reactor countdown, a late joiner, transfer timeout/disconnect, and host migration. For the first implementation, host migration may explicitly clear the checkpoint and hide the action; transferring the pinned checkpoint during migration can be a later enhancement.

Use introspection fields for checkpoint state, identity, capture generation, and transfer status. Do not validate through screenshots.

## Non-goals for the first implementation

- restoring the transient live checkpoint after process death when it is not also the durable highest checkpoint
- client-initiated restart requests
- non-cooperative multiplayer
- desktop UI exposure outside Android
- checkpoint transfer during host migration
- changing the existing rotating autosave history

## Implementation order

1. [x] Fix cooperative metadata trailers for memory saves.
2. [x] Add the pinned checkpoint manager, replacement lifecycle, leave cleanup, and host-migration cleanup.
3. [x] Add the durable highest-checkpoint writer, manifest, high-water rule, and picker-side size/checksum validation.
4. [x] Add transfer kind 2 and host-last application semantics.
5. [x] Replace the slot-only launch handoff with typed restore selection and add the retained entry to the cooperative Create Game picker.
6. [x] Add matched D1/D2 natural-transition and capture-ready hooks.
7. [x] Add JNI state/request access and the Settings tray action with confirmation.
8. [x] Extend introspection with the checkpoint state.
9. [x] Run scoped code quality, Windows D1/D2 host builds, Android native builds, and focused Kotlin tests.
10. [ ] Follow up with maintained D1/D2 two-emulator restart and relaunch integration scripts, including dead-host tray access.

## Implementation validation

- Scoped code-quality pass: passed
- Windows D1 and D2 build: passed
- Android native build for arm64-v8a, armeabi-v7a, and x86_64: passed
- Focused tray, typed-selection, and retained-checkpoint integrity unit tests: passed
- Full Android unit run: 489 passed, 1 skipped, with one unrelated existing mission ZIP expansion-ratio test failure

## Resume-offer correctness follow-up

1. [x] Extend multiplayer resume records with the typed retained-checkpoint identity.
2. [x] Re-resolve host resume choices against current valid saves and prefer the highest restorable level when the recorded choice is stale or missing.
3. [x] Use the resolved typed choice for labels and LAN/online launch handoff so a retained checkpoint cannot become an implicit fresh start.
4. [x] Add focused record, resolver, label, and handoff tests.
5. [x] Run scoped code quality, focused Kotlin tests, and the Android build. Windows host verification was not repeated because this follow-up changes Kotlin only.

Validation: the focused resume tests and the complete debug APK assembly passed. The APK assembly also rebuilt all three Android native ABIs successfully.
