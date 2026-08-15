# Map reactor countdown pause planning pass

Date: 2026-08-14
Status: implementation complete, focused validation complete

## Implementation result

- [x] Pilot-backed preference added for paired D1/D2, default on
- [x] Launcher switch and import/export support added
- [x] Automap Settings-tray secrets/reactor/objectives access gate added
- [x] Transient game-thread reactor pause/resume state added without cheater state
- [x] Host-authoritative cooperative request/state synchronization added
- [x] Restore, level initialization, and host migration reset transient pause state
- [x] Introspection and automation controls added
- [x] Focused admin-tray tests, native Android builds, and Windows D1/D2 build completed

## Request

Add a third Android automap Settings-tray action beside the existing secrets
and objectives actions.  The new action pauses or resumes an active reactor
countdown without setting the classic `cheats.enabled` / `CHEATER!` state.
Gate all three gameplay actions behind one pilot-backed launcher preference,
shown below Boss health bar as:

- Title: `Map cheats accessible`
- Description: `Secrets view button, reactor extend, and objectives extend options`

The reactor pause state is transient and is not added to save games.

## Confirmed current architecture

- The requested map "extra" menu is the Android Settings/admin tray while the
  automap is active. `AdminTrayPolicy.kt` currently appends
  `ADMIN_AUTOMAP_SECRET_REVEAL` and `ADMIN_AUTOMAP_OBJECTIVES` unconditionally.
- `TouchOverlayView.kt` draws and activates those actions. `MainActivity.kt`
  supplies native state and calls the JNI functions in `android_input.c`.
- Secrets and objectives directly change their own display state and do not
  route through the classic cheat-code handlers in `gamecntl.c`, so they do
  not set `cheats.enabled`.
- D1 and D2 decrement `Countdown_timer` in their paired `main/cntrlcen.c`
  implementations. The countdown also drives ship rocking, warning audio,
  palette flash, and final player death.
- The launcher Engine Preferences page already reads and transactionally
  patches paired D1/D2 pilot preferences through `NativePilotPreferences.kt`,
  `android_pilot_prefs.cpp`, and the shared PLX text helpers. Boss health bar
  is the closest model for the new preference.
- Multiplayer commands and their fixed lengths are defined in each game's
  `multi.h` and dispatched in `multi.c`. UDP dispatch can provide the
  authenticated sender, and the protocol version is checked at join time.
- A reactor-destroy packet starts the countdown independently on every peer.
  New joins are already rejected once the reactor is destroyed, so pause sync
  does not need a late-join snapshot path. Host migration and restore paths
  still need an explicit transient-state reset.

## Product decisions and assumptions

- Treat the launcher copy as one Boolean preference controlling access to all
  three map-cheat actions, rather than three independent preferences. This
  matches the singular title and keeps secrets/objectives behavior grouped as
  requested.
- Default the new preference to enabled to preserve the existing availability
  of secrets and objectives. If the desired product policy is opt-in cheats,
  change only the default to disabled before implementation.
- Apply the access gate only to the live game. Keep the standalone level
  preview's secrets/objectives controls available because those are inspection
  tools and there is no live player or reactor countdown there.
- Show the reactor action only while the automap is active and the preference
  is enabled. Label it `Reactor: Paused` or `Reactor: Running`; disable it when
  no countdown is active. Activating it while inactive must not arm a future
  countdown.
- In single player, the local player may toggle directly through a game-thread
  request. In cooperative multiplayer, any active player with the local menu
  exposed may request a toggle, but the host is authoritative. Do not expose
  or process the game-changing action in competitive multiplayer.
- Pausing freezes countdown progression and all simulation consequences owned
  by `do_countdown_frame`, including ship rocking, countdown sounds, terminal
  flash, and death. The cosmetic dead-reactor fireball may continue because it
  is outside countdown progression and uses the FX RNG.
- Do not touch `cheats.enabled`, inject a cheat string, play the cheater sound,
  replace the score with `CHEATER!`, or broadcast the cheater chat message.

## Implementation phases

### 1. Pilot preference and launcher UI

- Add a tail Boolean such as `MapCheatsAccessible` to both `player_config`
  structs in `d1/main/playsave.h` and `d2/main/playsave.h`.
- Initialize it in both `new_player_config()` paths, parse a paired PLX text key
  such as `mapcheatsaccessible=`, and write it in the same section in both
  games. Keep the new field text-backed so binary `.plr` layouts and versions
  are unchanged.
- Extend `playsave_android_shared.[ch]` HUD/preference helpers so the shared C
  parser remains the format source of truth. Preserve unknown PLX keys and use
  the existing transactional multi-pilot patch flow.
- Extend `android_pilot_prefs.cpp` read arrays, write contexts, log output, and
  JNI arguments for both game libraries. Extend `NativePilotPreferences.kt`'s
  model, defensive decoding, and all paired-game write helpers.
- Add `mapCheatsAccessible` state, dirty-state comparison, reset behavior, and
  the requested switch immediately below Boss health bar in
  `EnginePreferencesPage.kt`.
- Include the field in launcher config import/export wherever Engine
  Preferences are serialized, using a stable key such as
  `map_cheats_accessible`.

### 2. Transient reactor pause domain API

- Add paired D1/D2 reactor API declarations in `cntrlcen.h` and implementations
  in `cntrlcen.c`, for example:
  - query whether a countdown is active
  - query whether it is paused
  - request/toggle pause through the authoritative path
  - apply an authoritative paused state and remaining timer
  - reset transient pause state
- Keep the paused flag outside `player_config`, `cheats`, and save-state
  structures. Return early from countdown simulation while paused, before SIM
  RNG rocking and timer/audio/death processing.
- Clear the transient flag when a level initializes, when a reactor countdown
  starts, on end-level/level teardown, and after any state restore. A loaded
  save with an active countdown therefore resumes running, matching the request
  not to persist pause status.
- Consume the Android request on the game thread. Add a pending action in the
  shared Android meta/action plumbing or an equivalent atomic mailbox; do not
  mutate reactor globals directly from the Kotlin UI thread.
- Publish the active, paused, fixed-point timer, and displayed seconds through
  introspection. Add a focused automation setter/toggle so tests do not depend
  on screen-coordinate tapping.

### 3. Automap Settings-tray integration

- Add an `ADMIN_AUTOMAP_REACTOR` action ID in `TouchOverlayView.kt` without
  renumbering existing IDs.
- Add live-game native providers for:
  - loaded pilot's `MapCheatsAccessible`
  - countdown active state
  - paused state
  - reactor toggle request
- Pass `mapCheatsAccessible` into `adminTrayVisibleActions()`. When automap is
  active, append secrets, reactor, and objectives only when enabled. Preserve
  the preview-mode list as secrets/objectives only.
- Render the reactor row as a stateful checkbox-style action that remains open
  after activation, so its behavior matches secrets/objectives. Disable and
  dim the row if the reactor countdown is inactive.
- On preference-disabled sessions, remove all three actions rather than merely
  disabling them. Native entry points must still enforce the preference so a
  stale UI callback cannot bypass it.

### 4. Cooperative multiplayer authority and synchronization

- Add a compact reactor-pause command to both `multi.h` command tables and
  increment both `MULTI_PROTO_VERSION` values together.
- Use one fixed-length command with request/state variants, or two explicit
  fixed-length commands. The authoritative state must include paused/running
  and the host's exact `Countdown_timer` value; recompute
  `Countdown_seconds_left` from that value when applying it.
- Client flow: enqueue local UI request, send a reliable direct request to the
  current host, and leave local state unchanged until the host state arrives.
- Host flow: validate cooperative mode, active reactor countdown, requester is
  a connected playing peer, and authenticated sender identity; toggle the host
  state, then reliably broadcast the resulting state plus exact timer.
- Receiver flow: accept authoritative state only from the authenticated current
  host, reject malformed mode/state/timer values, apply idempotently, and show
  a short HUD message identifying pause or resume. The host applies through the
  same state function before broadcasting.
- Host-local activation uses the same host apply/broadcast path. Competitive
  games, observers, inactive countdowns, stale requests after end-level, and
  unauthenticated or non-host state packets are ignored safely.
- Reset paused state during host migration and cooperative restore/level restart
  transitions. No late-join sync is required while reactor-destroyed joining
  remains prohibited; document that dependency near the packet handler.
- Keep secret reveal and objective overlays local. Only the reactor pause is
  synchronized because it changes shared gameplay progression.

### 5. Determinism, tests, and validation

- Ensure input-demo recording/replay can reproduce the game-thread reactor
  pause transition. Record the authoritative pause/resume event or incorporate
  it into the maintained deterministic input-event/state mechanism; do not add
  it to ordinary save-game serialization.
- Extend `AdminTrayUiTest.kt` for preference-off hiding, preference-on ordering,
  preview behavior, checkbox/open-menu behavior, and inactive-countdown
  disabling.
- Extend the native playsave text tests for missing-key default, read/write,
  unknown-key preservation, malformed Boolean normalization, and paired D1/D2
  launcher decoding/transactional writes.
- Add paired native tests around countdown active detection, pause freezing the
  timer and SIM RNG path, resume from the same timer, inactive toggle rejection,
  level/reset/restore clearing, and absence of changes to `cheats.enabled`.
- Add multiplayer packet tests for host-local toggle, client request, exact
  timer broadcast/apply, duplicate state idempotence, non-host spoof rejection,
  malformed packet rejection, competitive-mode rejection, and host-migration
  reset.
- Add or extend an Android integration script under `android/game_scripts/` to
  destroy a reactor, open/use the map action, verify the timer is unchanged
  across multiple frames while paused, resume it, and assert the player is not
  marked as a cheater. Run paired D1 and D2 coverage, plus a two-peer coop run
  that asserts equal pause state and remaining timer on both peers.
- Run scoped formatting on all changed C/C++/Kotlin/test files, the focused unit
  and integration tests, Android Gradle tests with JDK 21, and
  `run-windows-build.ps1` for host D1/D2 compatibility.

## Expected file groups

- Gameplay parity: `d1/main/cntrlcen.[ch]`, `d2/main/cntrlcen.[ch]`, paired
  `playsave.[ch]`, paired `multi.[ch]`, and narrow level/restore reset call sites
- Shared Android native: `playsave_android_shared.[ch]`,
  `android_pilot_prefs.cpp`, `android_input.c`, `android_meta_actions.[ch]`,
  introspection/automation, and any focused reactor-pause shared helper
- Android UI: `NativePilotPreferences.kt`, `EnginePreferencesPage.kt`,
  `ConfigImportExport.kt`, `AdminTrayPolicy.kt`, `TouchOverlayView.kt`, and
  `MainActivity.kt`
- Tests: admin-tray JVM tests, playsave native tests, multiplayer/native tests,
  and paired maintained game scripts

## Non-goals

- No ordinary save-game format/version change and no restoration of paused
  status from a save
- No classic cheat-code entry, cheater flag, score penalty, sound, or chat
  announcement
- No synchronization of local secrets or objectives overlay choices
- No reactor reset, reactor resurrection, countdown extension amount, or timer
  editing; this feature only pauses and resumes the current remaining time
- No desktop menu addition unless separately requested; the scope is the
  Android automap Settings tray and launcher preferences

## Pre-implementation confirmation

The only product choice worth confirming before code starts is the preference
default. This plan recommends enabled for compatibility with the two existing
map actions. Choosing disabled would make all three an explicit opt-in cheat
surface but otherwise does not change the design.
