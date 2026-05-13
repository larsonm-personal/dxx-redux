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

3. Launcher resume offer [in progress]
	- [x] Add native save scanning bridge and Kotlin wrapper
	- [ ] Add Compose resume dialog
	- [x] Add preference toggle and config export/import coverage in "game preferences"
	- [x] Add setup introspection fields for current resume candidate and preference state
	- [x] Add setup introspection field for the current resume candidate

4. Direct resume load
	- [ ] Pass candidate game, callsign, and save path through launch intent
	- [ ] Add Android startup restore handling in D1 and D2
	- [ ] Verify that the game reaches the loaded level without user menu input

5. Validation
	- [x] Run native/unit tests for the metadata scanner
	- [ ] Run an Android automation test for exit-to-launcher autosave and launcher resume offer
	- [ ] Run a manual or scripted HOME/minimize test and inspect the autosave slot via native scanner or setup introspection
	- [ ] Run `android\run-code-quality.ps1 --fix` after checking for stale formatter locks
	- [x] Run a normal Windows host build and Android native compile smoke test for D1 and D2

## Notes
- Keep D1 and D2 source edits small and mirrored where engine hooks are needed
- Prefer C save/config code as the source of truth, with Kotlin calling narrow native helpers