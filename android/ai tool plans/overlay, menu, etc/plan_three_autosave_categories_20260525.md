# Three Autosave Categories Plan 2026-05-25

## Goal
- [x] Add a third single-player Android autosave category: highest progress for the active level set
- [x] Write/update highest-progress save during exit/close autosave paths only when new progress is greater
- [x] Keep the resume popup focused on the newest save, but add a chooser for newest candidates by category
- [x] Move "Stop Showing This" to the popup top row, left of "Resume Recent Save", with small text

## Current State
- Android single-player autosaves use slots 8 and 9:
  - slot 8: `auto_exit`, description `AUTO EXIT`
  - slot 9: `auto_minimize`, description `AUTO SAVE`
- `ResumeSaveBridge.findNewest()` currently returns one newest metadata-backed save across D1/D2.
- `SetupResumePanel.kt` renders only that one candidate and has two bottom buttons: "Stop Showing This" and "Load Last Save".
- Existing metadata includes game, mission filename, level number, wall-clock save time, level/total seconds, thumbnail, path, callsign, and description.

## Proposed Model
- Add `ANDROID_SAVE_META_KIND_AUTO_PROGRESS = 3`.
- Reserve slot 7 for the highest-progress save, leaving slots 8 and 9 unchanged.
- Treat "active level set" as `(game_id, mission_name, callsign)` for single-player saves.
- Define progress ordering conservatively:
  - Primary key: `level_num` for normal positive levels
  - Tie-breaker: `total_seconds`, then `level_seconds`, then wall-clock time
  - Do not treat secret levels as higher progress; D2 already skips Android autosave in secret levels
- Only overwrite slot 7 if the existing slot 7 is missing, invalid, for a different active level set, or lower progress.
- On overwrite, write description `AUTO BEST` or `BEST SAVE` and metadata kind `auto_progress`.

## Native Save Work
- [x] Add save kind constant and string mapping:
  - `android/app/src/main/cpp/shared/android_save_meta.h`
  - `android/app/src/main/cpp/shared/android_save_meta.c`
  - `android/app/src/main/cpp/jni_resume_save.cpp`
- [x] Add named slot constants in shared Android state code:
  - progress slot = 7
  - exit slot = 8
  - minimize slot = 9
- [x] Replace scattered `N_SAVE_SLOTS - 2` / `N_SAVE_SLOTS - 1` Android autosave choices in D1/D2 with shared or duplicated named constants where practical.
- [x] Add a helper in `state_android_shared.c`:
  - reads the existing progress-slot metadata
  - verifies same game/mission/callsign
  - compares progress
  - writes slot 7 only when the new save wins
- [x] Call that helper after successful exit/close autosaves:
  - queued return-to-launcher `auto_exit`
  - abort/close game `auto_exit`
  - background/close `auto_minimize`
- [x] Avoid multiplayer/coop progress saves. Existing `state_android_save_to_slot()` rejects `GM_MULTI`; keep that behavior.

## Resume Candidate API
- [x] Keep `findNewest()` for the pop-open's default candidate, or replace internally with a richer call that still exposes newest.
- [x] Add JNI method returning category candidates as JSON:
  - `latest_overall`
  - `highest_progress`
  - `last_exit`
  - `last_minimize`
- [x] Candidate selection rules:
  - highest progress: highest valid `auto_progress` by level and time, since the native slot is maintained per active level set
  - last exit: newest `auto_exit`
  - last minimize: newest `auto_minimize`
  - filter sentinel coop callsign as today
- [x] Kotlin `ResumeSaveBridge` should parse this into a small data object and load thumbnails for each present candidate.

## UI Work
- [x] In `SetupResumePanel.kt`, move "Stop Showing This" into the header row before "Resume Recent Save".
  - Use small text and compact padding so it does not crowd the thumbnail/title/hide icon.
- [x] Bottom row:
  - left button: "Load Last Save"
  - right button: "Choose Save"
- [x] Add a chooser dialog/sheet from "Choose Save":
  - large button for "Highest Progress" if present
  - large button for "Last Exit Save" if present
  - large button for "Last Minimize Save" if present
  - cancel button
- [x] Each large button should show enough scan text to avoid ambiguity:
  - category label
  - game + callsign
  - mission/level
  - save timestamp
  - optional thumbnail if cheap to include cleanly
- [x] On selecting a category, launch using the existing `onLaunchGame(candidate.game, candidate)` path.

## Testing
- [ ] Add or extend setup/resume candidate parsing tests if a JVM test harness exists for this area.
- [ ] Add focused native metadata tests if there is a lightweight host target for `android_save_meta`.
- [ ] Android manual/integration validation:
  - create D2 minimize save, exit save, progress save
  - verify `ResumeSaveBridge` exposes three categories
  - verify progress save is not overwritten by lower-level/lower-progress saves
  - verify it is overwritten after reaching a later level
  - verify "Load Last Save" still launches newest overall
  - verify "Choose Save" launches each category candidate
- [x] Run `android/run-code-quality.ps1 -Fix` after Kotlin changes and the Android native debug build.

## Validation Run
- [x] `android\run-code-quality.ps1 -Fix`
- [x] `cd android; .\gradlew.bat :app:externalNativeBuildDebug`
- [x] `cd android; .\gradlew.bat :app:compileDebugKotlin`
- [ ] On-device category behavior validation

## Risks And Notes
- Save slots are limited to 10 and coop autosave also uses slots 5-9 under a sentinel callsign. The new progress slot must remain single-player only to avoid coop confusion.
- Slot 7 may contain a user's manual save in older/current installs. Since the project is pre-release and Android launcher compatibility is not required, this is acceptable if we document it, but it is still worth being explicit in release notes/debug logs.
- Progress comparison should not depend on wall-clock time except as a tie-breaker.
- The active level-set filter depends on metadata being present. Old saves without Android metadata should not participate in the chooser categories.
