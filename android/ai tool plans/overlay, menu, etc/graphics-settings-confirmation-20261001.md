# Android graphics settings confirmation

Status: design only, implementation not started

## Required behavior

Treat a changed graphics configuration as a trial until the user explicitly chooses OK while looking at a playable 3D scene. Cancel, Back, B, and a five-second timeout restore the complete last accepted graphics configuration. Do not use an all-off configuration as the normal rollback target

Compare configuration values, not edit history or the existing graphics settings generation counter. Editing a setting and editing it back before the challenge starts must not show a prompt

The user confirmed that resolution and color depth belong in this feature, in addition to filtering, MSAA, and AF

## Protected configuration and comparison

Use one value snapshot containing:

| Config field | Purpose |
| --- | --- |
| `TexFilt` | Nearest, bilinear, or trilinear world filtering |
| `AnisoLevel` | AF |
| `MsaaLevel` | MSAA |
| `MenuTexFilt` | Filtering for menus and other non-world textures |
| `HudTexFilt` | Filtering for HUD textures |
| `ResolutionX`, `ResolutionY` | Render dimensions |
| `AspectX`, `AspectY` | Preserve the aspect settings accompanying a resolution change |
| `ColorDepth` | RGB565 or RGBA8888 request |

Gamma, FOV, text insets, pilot visual effects, movie filtering, debug switches, and controller settings are outside this snapshot. They continue to save normally and are not reverted with a failed trial. The protected snapshot covers the risky renderer settings and the selective filtering switches discussed with the user

Maintain three distinct values:

- `requested`: the settings saved by the launcher or most recent menu edit
- `current`: the coherent protected configuration actually being tried by the engine
- `accepted`: the durable snapshot from the last successful OK

At startup, requested and current converge as the engine initializes. In a running engine, fields awaiting application remain requested only. OK must never accept a saved resolution/color-depth change that has not reached the renderer

Normalize values using the same native validation rules used to apply them. Dimensions must pass `android_render_resolution_valid`; ratio values must be positive and reduced consistently. Read active engine dimensions from `Game_screen_mode`, rather than assuming `GameCfg.ResolutionX/Y` tracks a native menu change. Invalid values are rejected rather than becoming a new trial

GPU capabilities and effective sample counts belong in diagnostics, separate from equality. A requested 4x mode falling back to 2x must not repeatedly challenge on every frame. Structural renderer failure immediately rejects the trial; a supported capability fallback can be acknowledged by OK

The decision is:

```text
needs_confirmation = current != accepted && !all_enhancements_off(current)

all_enhancements_off(s) = s.TexFilt == 0
                         && s.MsaaLevel == 0
                         && s.AnisoLevel == 0
```

AF can enable effective filtering even when `TexFilt` is zero, so all three tests are necessary

The exemption uses those three fields exactly, even for a resolution or color-depth change. Menu/HUD switches may remain enabled because they have no effective filtering when both world filtering and AF are off. This honors the requested all-off exemption, but means resolution/color-depth problems in that configuration will not receive a five-second challenge

An exempt configuration does not overwrite `accepted`: the user defines acceptance as choosing OK. For example, accepting bilinear + 2x MSAA, then turning everything off, then trying trilinear + 4x MSAA and cancelling restores bilinear + 2x MSAA, including its accepted resolution and color depth

Use one app-wide accepted snapshot, matching the existing launcher/native behavior that mirrors these settings to root, D1, and D2 configs. Accepting in either game acknowledges that shared preference set. It is not a claim that both engines were separately tested. Level and robot previews, metadata workers, and headless runs cannot validate or replace it

## When the challenge starts

Eligibility must be determined on the game thread:

- A real game level is active and `Screen_mode == SCREEN_GAME`
- `Game_wind` exists and is the front engine window
- The frame will render the main gameplay view, not the automap, briefing, preview, loading screen, or a cockpit subview alone
- The Activity is foreground and its surface is available
- No unrelated Kotlin modal is blocking the gameplay view

Do not infer eligibility from `gameStarted`, a successful EGL swap, or `ogl_start_frame()` alone. The engine draws visible game windows underneath native menus, and the GL entry point is also used by other 3D views

| Source of change | Challenge timing |
| --- | --- |
| Launcher before game launch | At the first eligible gameplay render attempt after loading/briefings |
| Launcher while a game is retained in the background | After returning, applying its coherent snapshot, and becoming eligible |
| Kotlin Video Info settings | After 750 ms without a protected edit, while gameplay is eligible; the Video Info editor itself may remain open |
| Native menus opened mid-level | After the complete native menu stack closes, at the next eligible gameplay render attempt |
| Native menus before starting a level | At the first eligible gameplay render attempt after the level starts |

Reset the 750 ms debounce only for an actual protected value change. Compare again when it expires. Turning an option back to its accepted value, or reaching all-off, removes the pending challenge. Returning from native menus needs no extra 750 ms if the user has already stopped editing

For Kotlin live edits, coalesce renderer application along with the debounce. Show the requested values in the editor immediately, but keep rendering the previous configuration during the quiet period. At expiry, compare the proposed next-current snapshot (using the mode actually running), prepare the overlay if needed, and then apply that batch. This keeps the first potentially failing render inside an armed trial and avoids rebuilding the same resources for every tap. If the final batch equals accepted or is all-off, apply it without a challenge

One trial covers the complete final snapshot after the debounce. Once the prompt is active, protected edits and opening new settings menus are blocked until it resolves. This avoids having OK acknowledge a different configuration than the one displayed

If a native menu or lifecycle transition nevertheless interrupts an active trial, cancel it and restore accepted settings. Do not pause/reset its deadline and offer repeated five-second extensions

## Overlay and input

Add a dedicated Kotlin `GraphicsConfirmationOverlay`, above Video Info, touch controls, popups, and the game `SurfaceView` in the Activity's existing `FrameLayout`

Suggested layout:

```text
Keep these graphics settings?
<brief list of changed settings>

[ OK ]   [ Cancel (5) ]
           ^ initially selected
```

- Start with Cancel selected on every trial
- D-pad left/up selects OK; right/down selects Cancel, with bounded movement rather than wraparound
- A, D-pad center, or Enter activates the selected button; A does not always mean OK
- B, Android Back, and Escape cancel immediately
- Touch activates either button; outside touches are consumed without accepting
- Display `Cancel (5)` through `Cancel (1)` using `ceil(remaining_ms / 1000)`; expiry cancels, with no additional second at zero
- Derive display and expiry from a monotonic absolute deadline using `SystemClock.elapsedRealtime`, never game time, frame count, or a sequence of decrement callbacks
- Consume button press and release, D-pad repeats, touch drags, analog/hat navigation, and mapped meta actions before other overlay/native routing
- Do not let a button held while editing activate the new prompt. Require release and a fresh press for activation, and retain the release destination across dismissal using the existing controller dispatch mechanism
- Release gameplay inputs when acquiring modal ownership; keep suppressed inputs from becoming stuck or producing a shot/menu action after dismissal
- Save/restore the underlying Video Info selection and polling state. Refresh its values from the engine after resolution, rather than keeping its optimistic cycle values after rollback

The dialog must remain visible over a black game surface. Keep drawing it in Kotlin rather than with game textures. It uses Android's compositor and is independent of the game's EGL context, though a device-wide compositor/driver failure is outside that guarantee

Pause single-player simulation with the existing owned overlay-time pause, without creating a native menu window or stopping render/event processing. Do not acquire a second unmatched pause if the settings tray already owns one. Release only the pause owned by this flow. In multiplayer, continue normal network processing and rendering; the modal captures local input without freezing the session

## Native coordinator and UI handshake

Implement the shared policy in `android/app/src/main/cpp/shared/android_graphics_safety.cpp/.h`, with a C interface for engine call sites. Compile it for both Android engines and keep hooks in `d1/` and `d2/` small and guarded

The coordinator owns coherent snapshots, generations, eligibility, decisions, and rollback requests. Kotlin owns overlay presentation/input and an independent deadline watchdog. JNI calls enqueue changes/decisions or perform file-only operations; they must not perform GL work on the UI thread

Suggested state transitions:

```text
IDLE -> WAITING_FOR_SCENE_OR_DEBOUNCE -> PREPARING_OVERLAY -> CHALLENGE
CHALLENGE + valid OK -> ACCEPTING -> IDLE
CHALLENGE + Cancel/expiry/interruption -> RESTORING -> IDLE
any pending state + current == accepted or all-off -> IDLE
RESTORING + renderer cannot recover -> RECOVERY_REQUIRED
```

Each request carries a unique trial ID and snapshot generation. State transitions and deadline checks are serialized. Dismiss/OK events with an old ID or generation are ignored. OK received at or after the deadline loses to timeout. Duplicate Cancel/timeout events are idempotent

At the first eligible main-view frame:

1. Compare the fully normalized current snapshot, or the next-current batch awaiting live application, with accepted
2. Prepare a trial ID and publish a durable record of the configuration being tried
3. Notify Kotlin to display the overlay and capture input
4. Kotlin acknowledges that the overlay has drawn and arms an absolute five-second deadline through a file/state-only JNI call
5. On the next eligible render iteration, try the candidate with that armed deadline

While preparing the overlay, skip the candidate gameplay draw and keep pumping the engine event loop. Thus the timer is armed before the first candidate gameplay render and cannot depend on that render completing successfully. If UI preparation fails or does not acknowledge promptly, cancel instead of applying an unmonitored trial. Normally this adds only one UI draw/engine iteration

Native menus can already apply changes to the renderer before returning to gameplay. Record an unvalidated attempt before those changes and defer the visible five-second challenge until the game is eligible. Do not display the confirmation inside a native menu

Pump decisions/rollback at a game-thread event-loop boundary even while the game is paused or a menu is visible. Add a main-view hook before `game_render_frame()` in both engines to provide accurate eligibility. If necessary, split its notification before pending options are consumed in `ogl_start_frame()`, while retaining the gameplay eligibility token so previews/subviews cannot start trials

Poll the deadline natively as a backup when event processing continues. Kotlin also checks it independently, so a native GL hang cannot prevent the timeout decision from being persisted. Use a shared monotonic time domain (`CLOCK_BOOTTIME` on native Android and `elapsedRealtime` in Kotlin), with documented constants/interfaces

## Durable acceptance, pending attempts, and process boundaries

The launcher and `MainActivity` run in different processes (`MainActivity` is `:game`). SharedPreferences generations remain a refresh hint, not the accepted snapshot, a lock, or the authority for graphics state

Store one authoritative record at the app files root, for example `graphics_safety.json`:

```json
{
  "accepted": { "protected fields": "values" },
  "attempt": null
}
```

An attempt records its ID, owner session, candidate, phase, and armed deadline where applicable. A staged launcher edit changes normal config files without an attempt marker. An attempt marker means a running engine has started applying an unvalidated configuration; it is written before protected GL changes, including startup EGL initialization and native-menu mode switches

Use native serialization and file APIs exposed through a small variant-specific launcher bridge, following the existing native preference bridges. Kotlin should not duplicate parsing of engine configuration. Use a process-shared file lock and synced temporary file + atomic replacement. Both engine libraries use the same lock path; a process-local mutex alone is insufficient

Never hold the record/config lock while executing GL calls, waiting for an engine response, invoking Kotlin, or waiting for a UI draw. The Kotlin watchdog must be able to publish a rollback decision while the render thread is stalled

The accepted record is the durable commit point:

- OK checks ID, generation, and deadline, then durably replaces accepted with the exact tried snapshot and clears the attempt in one record replacement
- If acceptance persistence fails, do not dismiss as accepted; restore the previous accepted snapshot and show an ordinary Kotlin save-failure message
- Cancel/expiry first records the decision and accepted restore target, then patches config and queues renderer restoration
- Restore only protected keys, preserving unrelated config/player changes
- Patch every protected key across root and existing D1/D2 configs as one batch. Extend `graphics_config_transaction` beyond its current single-key interface; avoid successive independent transactions for each field
- Keep the restore marker until config patching succeeds. If the process stops between file replacements, startup repairs all protected keys from accepted before reading config/applying GL settings
- After a completed live restore, publish a resolved result to Kotlin and dismiss the overlay; show `Restoring graphics settings...` while recovery is in progress
- Synchronize the launcher's `render_resolution` display with restored config and advance its refresh generation. Make resolution config values authoritative when rebuilding the page, so stale cross-process preferences cannot undo a rollback

Normal config writes and protected launcher/native edits must participate in this same serialization boundary, or be reconciled before publication. Otherwise a native `WriteConfigFile()` or a simultaneous launcher edit can overwrite restored values. Launcher edits arriving during an active challenge are deferred until the current trial resolves; they never mutate the challenged snapshot

On normal exit before any eligible gameplay challenge, clear ownership cleanly and leave requested values staged for a later level launch. On an abandoned attempt after crash/force-stop, restore accepted before renderer initialization on the next launch. A staged launcher change with no abandoned attempt must survive app restarts and still challenge at its first level

At feature initialization with no accepted record, create a documented bootstrap rollback snapshot using conservative native defaults: 640x480, the normal valid display aspect, RGB565, nearest filtering, no AF/MSAA, and disabled menu/HUD filtering. Preserve an enhanced current request as an unvalidated candidate rather than silently blessing it. Once OK succeeds, this bootstrap is replaced permanently by the user's accepted snapshot. No migration/legacy readers are needed for the pre-release format

All-off configurations bypass the visible challenge and do not advance accepted. Do not leave an active five-second trial for them. Pre-level renderer attempt records can still support recovery from an abnormal process exit; normal all-off gameplay needs no confirmation

## Applying and restoring renderer state

Restoration must update active renderer state as well as config files. Otherwise the launcher would look repaired while the game remains black

### Filtering, AF, and MSAA

Reuse the existing texture invalidation and MSAA FBO rebuild paths, applying a coherent snapshot on the GL/game thread. Add a restore reason so setters do not recursively arm a new trial. Refresh `GameCfg`, the corresponding OGL values, and pending flags together. Do not rewrite unchanged fields or rebuild resources for a no-op

Route protected Kotlin JNI setters through a mailbox rather than directly mutating `GameCfg` from the UI thread. Native menu callbacks already execute on the game thread and can use the same coordinator. Keep other existing graphics APIs working

### Resolution and color depth

The launcher currently saves these for engine initialization. `readGraphicsConfigSnapshot()` does not apply them live on resume, and `nativeSetGraphicsOption()` has no corresponding setter. The safety flow must keep them requested-only until actually applied, then include them in the challenged snapshot

Native resolution menus already call `gr_set_mode()` and `game_init_render_buffers()`. On Android, that route destroys/reinitializes EGL using `GameCfg.ColorDepth`; it is a useful starting point for a shared game-thread restore operation

For a running engine, restore accepted dimensions, aspect, and color depth in one mode operation. Update `Game_screen_mode`, `GameCfg`, canvases, cockpit/game render buffers, viewport and touch-coordinate mapping. Rebuild context-dependent textures, shaders, MSAA targets and caches correctly. Do not treat a `SurfaceView` size callback as sufficient to change renderer color depth

Implement a checked Android mode-change adapter rather than assuming the existing `void android_egl_surface_initialize()` succeeded. Report config/surface/context creation failure explicitly, preserve the accepted target, and enter recovery if an accepted-mode rebuild fails. Keep desktop mode-setting behavior intact

Retain the launcher's existing next-start semantics for new resolution/color-depth edits while a game is retained. Other live edits are challenged against the mode actually running. On the next real startup, the requested mode is tried and included in confirmation. In-game native resolution changes remain live and challenge after all menus close

## Black frames, hangs, and recovery

A black image with an otherwise responsive engine should restore live on timeout. No framebuffer-color heuristic is required; the user decides whether the trial works

A GL call that never returns prevents the game thread from applying any new configuration. Kotlin can still persist the accepted restore target and request rollback. If the engine does not acknowledge restoration within one second, show a Kotlin recovery message, shut down the isolated game process through the existing controlled exit/recovery path, and return to the launcher with accepted settings repaired. Use a bounded process-exit fallback if normal shutdown is also stuck

Do not claim seamless in-level recovery from a blocked driver. Forced recovery may lose play since the last save; the normal live rollback path preserves the running level. This limitation and the independent persistence-before-GL design are necessary for the original black-screen failure case

If renderer initialization fails before any level starts, recover immediately from the attempt marker without pretending a five-second gameplay trial occurred. Previews/metadata workers use the requested configuration only under their own existing lifecycle; they must never write this acceptance record

A hard stall during initial EGL creation or a native-menu mode switch can occur before the requested gameplay challenge is eligible. The five-second trial cannot cover that interval without violating the menu/level timing requirement. The durable attempt marker provides recovery on process restart; the trial watchdog guarantee begins when the gameplay confirmation is armed

## Concrete change map

| File or area | Change |
| --- | --- |
| New shared `android_graphics_safety.cpp/.h` | Snapshot, state machine, locked durable record, mailbox, deadline decisions |
| `shared/android_graphics_options.c/.h` | Coherent protected setters and restore path; keep existing unprotected behavior |
| `shared/graphics_config_transaction.c/.h` | Multi-key config batch patching and recoverable restore publication |
| `jni_main.c` and new launcher JNI bridge | Trial callbacks, draw acknowledgement, decisions, read/stage/repair APIs |
| `shared/android_egl_surface.c/.h` | Checked context/surface creation and recovery results |
| `d1/arch/sdl/event.c`, `d2/arch/sdl/event.c` | Pump game-thread mailbox/deadline/restores while paused or in menus |
| `d1/main/game.c`, `d2/main/game.c` | Main-gameplay eligibility hook before rendering |
| `d1/main/menu.c`, `d2/main/menu.c` | Record native resolution attempts and protected changes; preserve deferred prompt behavior |
| `d1/arch/ogl/gr.c`, `d2/arch/ogl/gr.c` | Small Android mode restore adapters and resource rebuild hooks |
| `d1/main/config.c`, `d2/main/config.c` | Recovery before protected config consumption; serialize protected config publication |
| New `GraphicsConfirmationOverlay.kt` | Modal UI, countdown, default Cancel, fresh-press input |
| `MainActivity.kt` | Highest-priority input routing, lifecycle cancellation, overlay pause ownership, watchdog/recovery |
| `VideoInfoOverlay.kt` | Report genuine edits, debounce integration, refresh after restore |
| `GraphicsSettingsPage.kt`, `SetupConfigFiles.kt`, `SetupActivity.kt` | Stage/read coherent requests through native bridge; repair abandoned trials before launch; refresh resolution UI |
| Android CMake source lists | Compile shared implementation into both engine variants; add host-test target |
| `game_introspect.cpp` and automation | Stable safety state and decision actions for integration verification |

## Implementation sequence

- [ ] Add snapshot normalization, equality/exemption policy, trial state machine, durable record and config batch transaction
- [ ] Integrate initialization, attempted-configuration markers, native menu changes, config writers, and abandoned-attempt repair
- [ ] Add game-thread mailbox, gameplay eligibility hooks, filtering/FBO restore, and checked resolution/color-depth restore
- [ ] Add Kotlin modal, controller/touch routing, absolute deadline and independent watchdog, lifecycle/pause handling
- [ ] Integrate launcher staging/resume semantics, UI refresh, introspection and automation
- [ ] Build both Android games, run host coverage and serial device integration tests, and verify desktop guards
- [ ] Run scoped mixed-language quality tooling on the changed paths and update this plan with results

## Verification

Use meaningful host coverage for the state machine and persistence fault cases, then a reusable Android integration runner for D1 and D2. Expose accepted/current/requested, trial ID/generation/phase, remaining time, restore reason/status, and actual render dimensions/color depth through introspection. Android input automation must exercise real D-pad/A/B dispatch for the modal

Required scenarios:

1. Launcher edit -> first gameplay frame -> Cancel and timeout restore the full previously accepted tuple in engine and every mirrored config
2. OK before expiry persists the tried tuple; a fresh process/next level does not prompt for it
3. Edit -> original value, no-op edits, and multi-setting edits that return to accepted produce no prompt
4. All-off never prompts, including resolution/color-depth changes; it does not overwrite an enhanced accepted snapshot
5. Multiple Video Info edits within 750 ms produce one prompt for the final values; OK cannot accept a later edit
6. Native nested graphics/resolution menus mid-level never show the prompt until all menus close, even while the game draws behind them
7. Main-menu edits survive mission selection and briefings and challenge at the first gameplay attempt
8. Automap, level/robot preview, metadata jobs, demos without gameplay rendering, and loading/menu draws cannot start or accept trials
9. Default Cancel + A cancels; explicit navigation to OK + A accepts; B/Back/Escape cancel; held A/repeat/release and touch drags cannot leak into gameplay or auto-accept
10. A running-engine launcher mode change remains requested-only until next startup; an unrelated live filtering change does not falsely validate that future mode
11. Accepted resolution/aspect/color depth restore recreates a working renderer, buffers and input coordinates, alongside accepted filtering/MSAA/AF
12. Background, surface loss, native interruption and Activity replacement during a challenge cannot extend the deadline or lose the rollback target
13. Inject a black output frame with continuing event processing: Kotlin overlay stays visible and timeout restores live
14. Inject a render-thread stall after trial arming: Kotlin watchdog durably rejects and uses bounded process recovery without waiting on a GL-held lock
15. Kill during an unvalidated renderer attempt or partial config repair: next launcher/startup repairs accepted values before GL initialization; merely staged edits still survive
16. Fail record sync/rename and config batch publication: no false acceptance, preserved old accepted snapshot, repair marker retained and surfaced failure
17. Race OK against expiry and a stale callback; exactly one outcome wins, deadline equality cancels, and unrelated config/pilot values survive rollback
18. Single-player pause ownership balances across existing settings trays; multiplayer keeps processing its session during the prompt

Run the Retroid Pocket 4 Pro device checks for actual filtering/MSAA/AF and context/mode restoration. Emulator fault injection verifies coordination and recovery; it cannot establish that the physical device's GPU driver is fixed
