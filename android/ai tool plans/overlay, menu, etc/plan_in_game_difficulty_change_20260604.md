# In-game Difficulty Change Planning

Implementation started and core feature completed in this pass.

## Goal

Add an Android overlay settings tray action named "Change Difficulty" that is visible only while playing single-player or coop. It opens a small overlay menu listing the five difficulty levels:

- Trainee
- Rookie
- Hotshot
- Ace
- Insane

The current difficulty is highlighted green. Selecting a level immediately applies what can be changed mid-level, records a demo event, and persists the choice for subsequent levels. The menu closes on controller B/back or tapping outside the menu.

## Implementation Status

Completed:

- Added mirrored D1/D2 difficulty helpers, including live visibility/authority checks, history tracking, save restore sanitization, HUD message printing, and replay-safe application.
- Added Android settings tray "Change Difficulty" action with a five-row overlay menu, current-difficulty green highlight, controller up/down/A/B handling, row taps, and outside-tap close.
- Added JNI/provider plumbing so the button is visible in single-player/coop, disabled for non-host coop clients, and hidden for non-coop multiplayer and replay/playback.
- Added input-demo direct command recording/parsing/replay for `change_difficulty`, with replay applying the same HUD message without rewriting the user's default difficulty.
- Added coop host broadcast/client apply packet `MULTI_DIFFICULTY`, bumped D1/D2 multiplayer protocol versions, and ignored late packets during end-level.
- Added save-file difficulty history fields to D1/D2 state saves, Android save metadata, coop metadata, coop autosave history JSON, coop progress JSON, resume-save JSON, and introspection JSON.
- Mirrored D1's early coop metadata trailer lookup with D2 so Android save footers are skipped when reading the coop trailer for player matching.

Validated:

- `.\android\run-code-quality.ps1 -Fix` passed.
- `$env:JAVA_HOME='C:\local\jdk-21'; $env:Path="$env:JAVA_HOME\bin;$env:Path"; .\gradlew.bat :app:assembleDebug` passed.
- `git diff --check` passed with only line-ending normalization warnings.

Remaining useful follow-up:

- Add a focused automation or input-demo regression that changes difficulty mid-level, replays it, and checks final difficulty plus the HUD/console message.
- Run a two-emulator coop smoke test to confirm host-to-client difficulty propagation and matching history fields on both peers.

Crash follow-up 2026-06-04:

- User reported a D2 launch crash in the newest build.
- Tombstone symbolicates to `d2/2d/bitblt.c:570` via `show_fullscr(&nm_background)` in `d2/main/newmenu.c:446`, while `android_listbox_draw_scaled()` is drawing a startup listbox.
- Suspected cause: `nm_draw_background()` clamps to the virtual screen size (`SWIDTH`/`SHEIGHT`) even when the current canvas is a smaller Android scaled/offscreen canvas, which lets `gr_bitmap_scale_to()` write past the offscreen bitmap.
- Fixed: `nm_draw_background()` now clamps to the current canvas bitmap dimensions in D1 and D2 before creating its sub-canvas.
- Validated: rebuilt `:app:assembleDebug` and ran `test_menu_scale_d2.json5 -Game d2 -Install`; the test passed and D2 reached the menu with menu scaling active.

## Core Engine Facts

- D1 and D2 both define `NDL` as 5 in `d1/main/game.h` and `d2/main/game.h`.
- The live difficulty global is `Difficulty_level`.
- The display names come from `MENU_DIFFICULTY_TEXT(u)` in `d1/main/text.h` and `d2/main/text.h`.
- New single-player games set `Difficulty_level = PlayerCfg.DefaultDifficulty` and then call `do_difficulty_menu()` in `d1/main/menu.c` and `d2/main/menu.c`.
- Network games set `Netgame.difficulty` and copy it to `Difficulty_level` in `d1/main/net_udp.c` and `d2/main/net_udp.c`.
- Save games already serialize `Difficulty_level`; coop save metadata also stores `Netgame.difficulty`.
- Android save files also append `android_save_meta_disk` after any coop trailer from `android/app/src/main/cpp/shared/state_android_shared.c`.
- Android introspection already emits `difficulty`, `game_mode`, and multiplayer info from `android/app/src/main/cpp/shared/game_introspect.cpp`.

## Proposed Engine API

Add mirrored D1/D2 helpers, with small Android-facing wrappers:

- `int android_get_current_difficulty(void)`
- `int android_get_min_historical_difficulty(void)`
- `int android_get_max_historical_difficulty(void)`
- `int android_difficulty_changed_this_run(void)`
- `int android_can_change_difficulty(void)`
- `int android_change_difficulty(int difficulty, int from_network)`

Behavior:

- Clamp/validate `difficulty` to `0..NDL-1`.
- Allow only in a live game window, not movie/editor/menu-only screens.
- Allow single-player normal games.
- Allow coop games.
- Block non-coop multiplayer.
- In coop, only the host/master should originate a change. Clients apply a received host packet.
- Set `Difficulty_level`.
- In network games also set `Netgame.difficulty`.
- Set `PlayerCfg.DefaultDifficulty` so the next new single-player level/game starts from the new value, matching the existing difficulty menu behavior.
- Track historical difficulty fields:
  - `Difficulty_changed_this_run`
  - `Difficulty_level_min_seen`
  - `Difficulty_level_max_seen`
- Show a HUD message such as `Difficulty changed to Ace`.

History behavior:

- At a new run start, initialize `Difficulty_changed_this_run = 0` and min/max to the starting `Difficulty_level`.
- When a live or replayed change applies to a different value, set `Difficulty_changed_this_run = 1`, update min/max, and leave the current level state intact.
- On level transition, carry the historical flag and min/max forward for the whole save/progress run rather than resetting achievement history. The next level starts from the current difficulty, but the save tag still remembers that the run has changed difficulty.
- On level start after a carried transition, include the carried/current difficulty in min/max, but do not clear `Difficulty_changed_this_run`.
- If a change is selected to the already-current difficulty, close the UI but do not record a demo event, do not set the changed flag, and do not print a message.
- A restored old save with no history fields should initialize min=max=`Difficulty_level` and changed=0.

## Android UI Change Map

- `android/app/src/main/java/com/dxxredux/app/TouchOverlayView.kt`
  - Add `ADMIN_DIFFICULTY`.
  - Add label `Change Difficulty`.
  - Add child menu state similar to brightness/FOV child handling, but discrete list instead of slider.
  - Draw a centered or tray-attached panel with stable row rectangles.
  - Highlight current difficulty in green.
  - Support touch outside panel to close.
  - Support touch row selection to apply and close.
  - Support controller up/down/A/B/back.

- `android/app/src/main/java/com/dxxredux/app/AdminTrayPolicy.kt`
  - Add visibility policy so difficulty appears only for single-player or coop.
  - Make it non-checkbox, non-slider, and non-auto-closing until the child menu handles selection/cancel.
  - Add utility for difficulty names and range, or keep names near the overlay if that is the existing style.

- `android/app/src/main/java/com/dxxredux/app/MainActivity.kt`
  - Add external methods for get/can-set/set difficulty.
  - Wire `ADMIN_DIFFICULTY` to open the child menu.
  - Provide current difficulty and enabled state providers.
  - Use `nativeIsCoop` or `Game_mode`-based helper rather than relying only on Kotlin's `isMultiplayerGame`.
  - Close music/video/settings child overlays when the difficulty menu opens.

- `android/app/src/main/cpp/jni_main.c`
  - Add JNI methods:
    - `nativeGetDifficulty()`
    - `nativeCanChangeDifficulty()`
    - `nativeSetDifficulty(int difficulty)`
  - Route to D1/D2 symbols by linking into each game library the same way other MainActivity native hooks do.

## Coop Network Change Map

- `d1/main/multi.h` and `d2/main/multi.h`
  - Add a new packet ID, for example `MULTI_DIFFICULTY`.
  - Increment `MULTI_PROTO_VERSION` for D1 and D2.
  - Add length entry for a compact packet, likely 2 bytes: packet type plus new difficulty.

- `d1/main/multi.c` and `d2/main/multi.c`
  - Add `multi_send_difficulty(int difficulty)` host/master broadcast.
  - Add `multi_do_difficulty(const ubyte *buf)` to validate and apply host-provided difficulty.
  - Add a dispatch case in the packet switch.
  - In non-host clients, `nativeSetDifficulty()` should send a request only if a request packet is intentionally added. Simpler first cut: show disabled for clients and make host-only changes.
  - Update `Netgame.difficulty` and `Difficulty_level` together when applying.
  - Update historical min/max and `Difficulty_changed_this_run` on every peer when the packet applies.
  - Print the same HUD message on every peer so coop players see the setting take effect.

Open decision:

- Host-only is safer and less code. Client request support is friendlier but adds another packet and denial path.

## Demo Compatibility Change Map

Use input-demo frame events, not classic demo events. The current input-demo format already has per-frame `events`, and direct commands already use it.

- `android/app/src/main/cpp/shared/input_demo_recorder.h/.cpp`
  - Add `input_demo_recorder_stage_direct_command_change_difficulty(int difficulty, ...)`.
  - Emit canonical JSON:
    - `{"kind":"direct_command","command":"change_difficulty","difficulty":3}`
  - Stage the command for the next captured frame, matching existing direct-command staging.
  - Do not emit a command when the selected difficulty equals the current difficulty.

- `android/app/src/main/cpp/shared/input_demo_replay.h/.cpp`
  - Add `INPUT_DEMO_REPLAY_DIRECT_COMMAND_CHANGE_DIFFICULTY`.
  - Parse the `change_difficulty` command.
  - Validate `difficulty` is an integer in `0..NDL-1`.
  - The direct-command parser currently fails on unknown direct-command names, so new demos containing `change_difficulty` require replay builds that know this command. Existing demos remain compatible because they do not contain the new command.

- `d1/main/gamecntl.c` and `d2/main/gamecntl.c`
  - Add difficulty application in direct-command replay.
  - D2 currently has a broad `input_demo_replay_apply_direct_commands()`.
  - D1 currently only applies death-abort direct commands, so D1 needs a broader replay path or a specific difficulty-event path.
  - Apply the command from `ReadControlsReplayFrame()` before the replay frame advances into game simulation.
  - Route replay application through the same core helper as live UI and coop packets, with a flag that allows demo playback but suppresses recorder staging.

- `android/app/src/main/cpp/shared/input_demo_hooks_shared.c`
  - Final result capture already records current `Difficulty_level`, so changed-difficulty demos should naturally compare final difficulty correctly.

Detailed compatibility notes:

- Existing demos without events continue to parse and replay.
- Existing demos with older known direct commands continue to parse and replay because the parser adds one more recognized command rather than changing existing command shapes.
- New demos with `change_difficulty` require current replay code. This is acceptable for input demos because direct-command events are semantic game actions, not ignored annotations.
- The header `difficulty` remains the starting difficulty. The final result `difficulty` becomes the ending/current difficulty.
- Do not add difficulty-history fields to the input-demo header unless there is a separate test need. The header parser is strict about metadata keys, and changing header metadata would require fixture/parser updates without helping determinism.
- The replay event should not consume RNG. Applying it before the frame's game simulation preserves deterministic replay because all later AI, weapon, collision, and scoring code reads the same new `Difficulty_level` at the same point.
- If a UI selection happens while the overlay/settings tray has paused or slowed interaction, the staged event applies to the next recorded frame. Live application should use the same game-thread boundary so the recording and replay agree.
- `input_demo_recorder_truncate()` already clears future `frame_events` and `pending_frame_events`; add a regression test that rewinds/truncates across a difficulty change to prove stale future difficulty commands are removed.
- During replay, do not check Android UI visibility, host/client authority, or `android_can_change_difficulty()`. Replay is authoritative from the demo event stream. Still validate the value range.
- During replay, print the same HUD message as live play: `Difficulty changed to Ace`. This means the message appears in replay exactly when the difficulty changes.
- Classic `.dem` playback should not get a live difficulty UI and does not need a new classic demo opcode unless there is a separate classic-demo recording goal.

## Save File Tagging Change Map

Add a save-file tag for future achievement tracking:

- `difficulty_changed`
- `difficulty_min`
- `difficulty_max`

Core D1/D2 save state:

- Add mirrored globals in D1 and D2 near `Difficulty_level`, or a tiny shared Android-only module if that keeps base-game churn lower.
- Bump `STATE_VERSION` in `d1/main/state.c` from 12 to 13 and `d2/main/state.c` from 27 to 28.
- Write the three fields immediately after the existing `Difficulty_level` integer in both save files.
- On restore:
  - For new versions, read and sanitize the three fields.
  - For old versions, set `difficulty_changed = 0`, `difficulty_min = Difficulty_level`, and `difficulty_max = Difficulty_level`.
  - Clamp corrupted min/max to `0..NDL-1`; if min > max after clamping, reset both to current difficulty and clear the changed flag.
- Ensure coop save restore uses the same restored historical fields after `Difficulty_level` is read, and then keeps `Netgame.difficulty` synchronized with the current difficulty.
- Include the fields in rewind save state, because rewind uses the normal save machinery. If a rewind crosses back before a difficulty change, history should roll back with the restored save snapshot.

Android launcher metadata:

- Extend `android_save_meta_write_params` and `android_save_meta_disk` with the three difficulty history fields.
- Bump `ANDROID_SAVE_META_VERSION` from 2 to 3.
- Populate the metadata in `state_android_write_save_metadata()` from the engine history globals.
- Because this Android footer is currently strict-versioned, old Android metadata readers will ignore v3 saves. That is acceptable before release per project guidance, but the plan should include launcher updates wherever save metadata is listed or sorted.

Coop metadata:

- Extend `coop_save_metadata` with the same three fields and bump `COOP_SAVE_META_VER` from 3 to 4.
- Populate them in `coop_write_save_metadata()` from the engine history globals.
- Accept old coop metadata versions when reading; for v1-v3 metadata, infer changed=0 and min=max=`meta.difficulty`.
- Consider adding these fields to `coop_progress.json` and `coop_progress_inventory.bin` if achievements may inspect coop progress files outside a full save. If not, core save state plus Android save metadata is sufficient.

Achievement semantics:

- The run should be considered "difficulty changed" if the player ever successfully selected a different difficulty, even if they later return to the starting difficulty.
- `difficulty_min` and `difficulty_max` are historical across the restored run/save lineage, not just the current level.
- A save started on Hotshot, changed to Trainee, then changed back to Hotshot should have `difficulty_changed=1`, min=Trainee, max=Hotshot.
- A save started on Rookie and never changed should have `difficulty_changed=0`, min=Rookie, max=Rookie.
- Coop host and clients must converge on the same history fields. The host packet should carry enough information to let clients update history identically, or every peer can derive the same result from old/current/new difficulty if the packet is applied once.

## Runtime Difficulty Effects

These change immediately because code reads `Difficulty_level` live:

- Robot AI movement and pursuit
  - `d1/main/ai.c`, `d2/main/ai.c`, `d2/main/ai2.c`, `d1/main/aipath.c`, `d2/main/aipath.c`
  - Robot max speed, turn time, path length, circling distance, evasion, field of view, pursuit thresholds, firing timing, aiming inaccuracy, boss invulnerability dot, and misc-sound timing all use `Difficulty_level` directly.

- Robot shooting and projectile choice physics
  - `d1/main/ai.c`, `d2/main/ai2.c`, `d1/main/laser.c`, `d2/main/laser.c`
  - New robot shots use weapon speed/damage tables at the new difficulty.
  - Existing in-flight shots keep their current velocity and shields unless code later clamps them using the new difficulty.

- Player weapon behavior
  - `d1/main/laser.c`, `d2/main/laser.c`, `d2/main/controls.c`
  - Weapon speed, weapon strength, homing behavior, guided missile speed, Omega damage/speed, and low-difficulty energy usage reductions change for future fire/updates.

- Collision and damage
  - `d1/main/collide.c`, `d2/main/collide.c`, `d1/main/fireball.c`, `d2/main/fireball.c`
  - Lava/volatile wall damage, blast force, player bump damage, robot damage probability, and weapon explosion strength use the new difficulty at the time of collision/explosion.

- Powerup pickup amount
  - `d1/main/powerup.c`, `d2/main/powerup.c`
  - Energy boosts and shield repairs use the new difficulty when collected.

- Materialization centers after they are already enabled
  - `d1/main/fuelcen.c`, `d2/main/fuelcen.c`
  - `MATCEN_LIFE`, spawn limits, and ongoing spawn checks read the current difficulty.

- Reactor/control-center behavior before destruction
  - `d1/main/cntrlcen.c`, `d2/main/cntrlcen.c`
  - Reactor firing cadence uses current difficulty.

- End-level scoring
  - `d1/main/gameseq.c`, `d2/main/gameseq.c`
  - Skill/hostage/shield/energy bonuses use whichever difficulty is current at level-end scoring time.

- Pause/status/high-score labels
  - `d1/main/gamecntl.c`, `d2/main/gamecntl.c`, `d1/main/scores.c`, `d2/main/scores.c`
  - Labels and high-score recorded difficulty use the current difficulty.

## Startup/Baked Effects Not Recomputed Mid-level

These should not be recomputed when the player changes difficulty in-level:

- Initial player shields
  - `StartingShields` is assigned at player/new-game/new-ship setup, not dynamically changed.

- Initial concussion missile count
  - `Players[pnum].secondary_ammo[0] = 2 + NDL - Difficulty_level` occurs during initial-player and new-player setup.
  - Do not add or remove ammo mid-level.

- Existing robot shields
  - D2 `copy_defaults_to_robot()` scales boss/guidebot shields by difficulty at level load.
  - Existing robot shields should stay as-is. New matcen-created robots use `Robot_info` base strength from creation code, not a retroactive rescale.

- Existing in-flight weapon velocity/strength/shields
  - Already-created objects keep their stored fields. New shots use the new difficulty.

- Already-triggered reactor countdown
  - Countdown setup uses difficulty. Once the countdown is active, changing difficulty should not redo the countdown length. Reactor firing cadence before destruction can still change.

- Already-created materialization center capacity/max capacity
  - Creation sets `Capacity`/`MaxCapacity` from difficulty. Leave the current stored capacity alone. Ongoing per-frame logic can read the new difficulty where it already does.

- Existing score already awarded
  - Do not recalculate prior score. Future level-complete bonuses use current difficulty.

- Multiplayer powerup caps
  - Powerup caps are based on current mine contents and player inventory, not difficulty. Leave unchanged.

## Edge Cases

- Block while not actually in-game, while in movies/briefings, editor, main menu, or no loaded level.
- Block during demo playback unless the event is coming from replay.
- During input-demo recording, record one event only when the value actually changes.
- During input-demo replay, apply the event even though UI is absent.
- During input-demo replay, message printing must happen, but recorder staging must not happen.
- In coop, do not let clients independently change `Difficulty_level`; this would desync AI/damage. Prefer host-only UI visibility or disabled state.
- Apply host packet before simulation for the frame where it is processed.
- Late joiners should get the current `Netgame.difficulty` through existing join game info, but verify that the running game advertisement uses updated `Netgame.difficulty`.
- Late joiners also need historical difficulty fields if achievements can be tracked per client. If those fields are not in join info, the joining client should initialize history from host state on restore/start and receive future changes normally.
- Saved games after a change should write the new difficulty plus difficulty history fields.
- Rewind snapshots include save state and current difficulty through existing save paths; if a rewind crosses a difficulty-change point while recording an input demo, recorder truncation already removes future events, but this needs a test.
- Avoid non-coop multiplayer, where live difficulty changes can be fairness-sensitive and do not match the user's requested scope.
- Do not retroactively alter high-score entries or already-written save metadata.
- Do not mark cheats enabled. Difficulty changing is an explicit supported setting, not a cheat.

## Test Plan

- Kotlin unit test for `adminTrayVisibleActions()` showing difficulty in single-player/coop and hidden in non-coop.
- Kotlin unit test for difficulty child-menu navigation and outside-tap close if feasible with existing overlay tests.
- Native input-demo unit test that writes/parses/replays a `change_difficulty` direct command.
- Native input-demo parser negative test for out-of-range difficulty.
- Host/headless replay smoke test: start at difficulty 1, change to 3 mid-demo, assert final result difficulty 3.
- Replay smoke test asserts the console/HUD message ring contains `Difficulty changed to Ace`.
- Android integration script using introspection:
  - Start D2 single-player.
  - Open settings tray.
  - Select `Ace`.
  - Assert introspection `difficulty == 3`.
  - Assert the console/HUD message ring contains `Difficulty changed to Ace`.
  - Save and reload, assert difficulty persists.
  - Assert save history fields are changed=1, min includes the starting difficulty, and max includes `Ace`.
- Coop manual/integration test with two emulators:
  - Host changes difficulty.
  - Client introspection shows same difficulty.
  - Host and client history fields match.
  - Host and client both print the HUD message.
  - Host and client remain connected and robot behavior does not immediately desync.
