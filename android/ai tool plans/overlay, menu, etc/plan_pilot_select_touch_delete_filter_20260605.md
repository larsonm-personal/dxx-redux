# Pilot Select Touch, Delete, And Filtering Plan

Created: 2026-06-05

## Request

- Fix wrong touch regions on the initial pilot select box, especially on phone hardware where the emulator may not reproduce it.
- Add a long-press path to open a delete-pilot confirmation.
- Hide pilots that cannot be selected because the player file is corrupt or for the wrong game/version.

## Plan

1. [done] Trace pilot select creation, Android mouse coordinate mapping, and recent scaled-menu touch fixes.
2. [done] Add Android debug logging around the first pilot menu touch mapping so phone logs can prove whether stale rectangles or bad file entries are involved.
3. [done] Confirm long-press delete handling already exists in the pilot list without disrupting controller/keyboard selection.
4. [done] Filter invalid pilot files using the existing C player-file validation path, keeping playsave.c as the source of truth.
5. [done] Run focused formatting/build/tests and update this plan with results.

## Notes

- Existing long-press delete is implemented in `android_pilot_listbox_hold.c`: holding a selectable pilot for about 600 ms sends the existing Ctrl+D delete path and suppresses the release click.
- The first-touch coordinate fix publishes the scaled listbox source/destination rectangle during listbox layout, before the first draw has to complete.
- Pilot filtering now skips `.plr` entries that fail the same header/version/size checks used by each game's player file reader.

## Validation

- `android/run-code-quality.ps1 --fix -Paths ...` passed on the touched scope.
- `./android/gradlew.bat -p android :app:assembleDebug` passed.
