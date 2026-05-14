# Android autosave and resume offer research

## Goal
Design Android minimize and exit-to-launcher autosaves, launcher resume prompt, and playthrough time tracking for D1 and D2 with minimal base-game changes.

## Research checklist
- [x] Map D1 and D2 save slot numbering, naming, thumbnail, and metadata storage paths
- [x] Map Android activity lifecycle and game pause/minimize/exit-to-launcher flows
- [x] Map launcher preferences and Game Preferences UI hooks for a global hide-resume-offer option
- [x] Determine whether playthrough time requires new save data and where it should live
- [x] Propose phased implementation and test coverage

## Findings
- Both games expose 10 save slots as internal indexes 0 through 9. User-visible slot 10 is index 9, and the second-to-last slot is index 8.
- Android save files live under `filesDir/d1x-redux/Players` and `filesDir/d2x-redux/Players` because Android forces `GameArg.SysUsePlayersDir = 1` in each PhysFS setup path.
- Single-player save names are `Players/<callsign>.sg0` through `Players/<callsign>.sg9`. Coop uses `mg` files and already has its own Android autosave rotation in slots 5 through 9, so this plan should start with single-player autosaves only.
- The current save description field is 20 bytes. `AUTO SAVE` fits. The exit-to-launcher slot needs a label decision, with `AUTO EXIT` as the proposed distinct label.
- Current saves already include a 100 x 50 thumbnail. Current Android-era D1 and D2 saves store thumbnails as packed 6-bit RGB.
- D1 and D2 already persist level time and total game time in `player.time_level`, `player.hours_level`, `player.time_total`, and `player.hours_total`. Those values reset on new game, increment during play, are saved in `player_rw`, and restore from save. No new engine time counter is needed for the requested playthrough semantics.
- Single-player saves do not store wall-clock save time or level name in an easy launcher-readable form. File mtime can answer wall-clock time, but a small Android metadata trailer is cleaner and avoids Kotlin duplicating the save format.
- Android minimize is feasible. `MainActivity.onStop()` already runs for backgrounding and calls `nativeOnPause()`, which pauses audio/surface work and injects Escape for single-player gameplay. The compromise is that Android can still kill a background app, so the autosave should be best effort and must be queued immediately when `onStop()` happens.
- The lifecycle caveat is thumbnail capture. Save code renders and reads pixels for the thumbnail, while `nativeOnPause()` currently pauses the Android surface first. Implementation should either request and consume autosave before surface pause affects rendering, or use a cached last-visible thumbnail fallback for background saves.

## Proposed design
- Add Android-only save metadata written by D1 and D2 `state_save_all_sub()` after the normal save data and after existing coop metadata. Use a fixed trailer at EOF with tag, version, game id, save kind, wall-clock timestamp, callsign, description, mission filename, current level number, current level name, level time seconds, total playthrough seconds, and a small thumbnail payload or thumbnail descriptor.
- Add Android-only direct save helpers in both games, for example `android_save_to_slot(slot, desc, kind)`, that build the normal save filename without opening the menu and call the existing save writer. D2 must preserve the current secret-level companion file behavior when saving to a normal slot.
- Add volatile Android autosave requests in native glue. `MainActivity.onStop()` queues kind `minimize` for slot 9 and description `AUTO SAVE`. `META_RETURN_TO_LAUNCHER` queues kind `exit_to_launcher` for slot 8, then quits only after the game thread has saved or decided the state is not saveable.
- Consume autosave requests on the game thread from the same area as the existing Android save/load request flags in `gamecntl.c`. Skip safely for multiplayer, no loaded level, dead player, incompatible D2 secret/final-boss states, demo playback, and other unsaveable states. Log the skip reason through Android debug logging.
- Add a launcher-side native save indexer, exposed through a small Kotlin wrapper, that scans both game save directories and returns the newest metadata-backed candidate across all callsigns and both games. This keeps detailed save knowledge in native code and gives Kotlin a stable DTO.
- Add launcher UI state in `SetupScreen`: when the global preference allows resume offers and a candidate exists, show a small modal resume offer with thumbnail on the left, buttons `Load Last Save` and `Stop Showing This`, and lines for save time, game/level, level time, and total playthrough time.
- Add `PREF_SHOW_RESUME_OFFER`, default true, to `dxx_prefs`, include it in config import/export, and add a switch under `EnginePreferencesPage` in the existing Game Preferences screen. `Stop Showing This` writes false.
- For `Load Last Save`, launch the matching game with callsign and relative save path extras. Add Android startup handling so the game loads the pilot and restores the save path without going through the menu. D2 already has a filename override restore path; D1 needs a matching Android-only direct restore entry point.

## Phased work plan
1. Native save metadata and parser [done]
	- [x] Add shared Android save metadata constants/structs
	- [x] Write metadata from D1 and D2 saves
	- [x] Add a native scanner unit test with synthetic metadata-backed saves
	- [x] Reuse d1/d2 code where possible, but keep the new implementation in android/ to minimize d1/d2 diff size

2. Direct autosave hooks [in progress]
	- [ ] Add a launcher-wide setting to enable (default) or disable autosave behavior in "game preferences"
	- [x] Add direct slot-save helpers in D1 and D2
	- [x] Add queued autosave request flags and game-thread consumption
	- [x] Route minimize and exit-to-launcher through the queue
	- [x] Add debug log entries for saved, skipped, and failed autosaves
	- [x] Validate Android native compile for the autosave path

3. Launcher resume offer [done]
	- [x] Add native save scanning bridge and Kotlin wrapper
	- [x] Add Compose resume dialog
	- [x] Add preference toggle and config export/import coverage in "game preferences"
	- [x] Add setup introspection fields for current resume candidate and preference state
	- [x] Add setup introspection field for the current resume candidate

4. Direct resume load [done]
	- [x] Pass candidate game, callsign, and save path through launch intent
	- [x] Add Android startup restore handling in D1 and D2
	- [x] Verify that the game reaches the loaded level without user menu input

5. Launcher resume follow-up [done]
	- [x] Replace the popup resume offer with a top slide-down launcher panel
	- [x] Flush startup resume input state so touch controls start cleanly after launcher resume
	- [x] Re-run Android compile smoke tests for the launcher resume path
	- [x] Make transient launcher resume intents single-use so recents/task restore does not replay a save load
	- [x] Add resume save path/callsign fallback derivation in the launcher handoff
	- [x] Compact the launcher resume panel to a 3-line summary with a much smaller thumbnail
	- [x] Reset touch overlay state on suspend/launcher handoff so touch input is re-armed on return

6. Launcher resume task and panel follow-up [in progress]
	- [x] Compact the slide-down panel header and move the thumbnail into the header row
	- [x] Make Stop Showing This match Load Last Save size while using the panel background color
	- [x] Rework Load Last Save so a resume candidate bypasses the generic live-game return branch
	- [x] Rework Return To Game so it brings the existing MainActivity task forward instead of just finishing SetupActivity
	- [x] Compile the Android debug/native path and verify the upload build script reaches its build phase

7. Validation
	- [x] Run native/unit tests for the metadata scanner
	- [x] Run an Android automation test for exit-to-launcher autosave and launcher resume offer
	- [ ] Run a manual or scripted HOME/minimize test and inspect the autosave slot via native scanner or setup introspection
	- [x] Run `android\run-code-quality.ps1 -Fix` after checking for stale formatter locks
	- [x] Run a normal Windows host build and Android native compile smoke test for D1 and D2

8. Touch-axis and refreshed autosave resume follow-up [done]
	- [x] Trace touch axis events past JNI/SDL/deadzone into gameplay control application
	- [x] Fix loaded-save touch axis controls so translate/look/throttle do not remain effectively disabled
	- [x] Fix refreshed autosave resume details when metadata callsign/path is missing or incomplete
	- [x] Re-run focused Android compile and the upload wrapper build-only validation

## Notes
- Keep D1 and D2 source edits small and mirrored where engine hooks are needed
- Prefer C save/config code as the source of truth, with Kotlin calling narrow native helpers

9. Direct-load resume regressions reported from build 13334 [done]
	- [x] Treat build 13334 device results as current and reproduce or instrument with emulator where useful
	- [x] Re-check launcher candidate selection and game launch routing when first Load Last Save opens pilot select
	- [x] Re-check startup resume argument consumption from Kotlin through JNI into D1 and D2
	- [x] Re-check post-restore control state after direct-loaded saves from runtime introspection/logs, not only code review
	- [x] Patch root causes and validate with build plus emulator/manual-friendly diagnostics
	- [x] Preserve resume save extras through launcher automation continuation so the regression test covers real direct restore
	- [x] Validate D1 and D2 autosave resume automation reaches in-game with non-255 joystick axis bindings

10. Resume offer visibility regression [done]
	- [x] Confirm whether setup introspection sees a candidate while the panel buttons are absent
	- [x] Refresh the Compose resume candidate after delayed exit-autosave completion
	- [x] Keep per-save panel dismissal stable across automatic setup refreshes
	- [x] Validate the launcher shows Load Last Save when a candidate exists
	- [x] Validate D1 and D2 autosave resume automation assert both resume panel buttons

11. Device resume offer still absent in build 13336 [done]
	- [x] Treat device logs as current and avoid assuming a UI refresh race
	- [x] Add scanner fallback so any single-player `.sg#` save can produce a resume candidate even without readable metadata
	- [x] Stop rejecting candidates solely because metadata callsign and pilot filename do not match exactly
	- [x] Add launcher debug log lines for candidate selection and panel show/hide gates
	- [x] Validate fresh setup launch and autosave resume flows with emulator diagnostics

12. Re-show offer after load and first direct resume [done]
	- [x] Do not mark a save offer dismissed when the user taps Load Last Save
	- [x] Let Android startup resume restore from a save-derived callsign even if the `.plr` file is missing on the first attempt
	- [x] Validate that returning to the launcher after loading still shows the current save offer
	- [x] Validate first-attempt D1 and D2 direct resume from clean launcher state

13. Real-device direct load still reaches pilot select [done]
	- [x] Add durable diagnostics for the exact resume candidate, launch extras, JNI argv, and startup resume result
	- [x] Verify the automated test is asserting a true direct restore, not passing after normal menu state or stale game state
	- [x] Make Android startup resume survive missing intent extras by writing a pending resume launch request before starting the game process
	- [x] Validate with D1 and D2 from a force-stopped/fresh game process and with pilot files cleared

14. Persistent game-log diagnostics for stubborn direct-load pilot select [in progress]
	- [x] Log launcher resume candidate, launch intent extras, pending resume file state, and game activity state to the downloadable Game Logs category
	- [x] Log JNI startup parameters, consumed resume values, and final argv through DLOG_GAME
	- [x] Log D1 and D2 startup resume decisions and pilot fallback state through DLOG_GAME
	- [x] Serialize save header/metadata details and a save-file digest when restore opens a save, including failure branches before restore can enter the level
	- [x] Run code quality checks scoped to the touched Kotlin files and rebuild the Android debug APK
	- [ ] Focused resume validation: attempted with Game Logs enabled, but adb/logcat wedged before the test produced output; retry on a fresh emulator/device