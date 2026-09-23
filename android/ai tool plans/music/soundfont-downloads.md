# SoundFont download catalog

Implement a curated URL catalog, initially empty. Each entry supplies a display
name, direct download URL, short description, original website/readme URL and
license text. Show all fields before the user confirms a download.

1. Add the catalog and an HTTPS downloader using existing OkHttp dependencies
2. Stream through SoundfontStore's bounded, transactional SF2 import and native
   validation; handle GitHub HTTPS redirects, HTTP errors and cancellation
3. Add catalog selection, confirmation, progress and cancel/error UI to the shared
   SoundfontSelector used by Game Preferences and music setup
4. Verify download-to-store integration, failures, limits and cancellation with
   JVM tests; format scoped changes and build the Android app

Use direct SF2 release assets, not ZIP archives or GitHub HTML pages. Preserve
the current renderer selection when activating a downloaded bank. Keep the
existing 64 MiB limit. No remote catalog or network requests on page opening.
No sample downloads or unlicensed assets are added to the shipped catalog.

## Downloaded library management

Persist the download descriptor with each imported asset so its Info dialog
survives app restarts and removal from the download catalog. Preserve metadata
on local reimport and attach it when a download duplicates an existing local
asset. Add confirmed deletion for user-added fonts, including inactive fonts.
The bundled asset cannot be deleted. Deleting the active font first activates
the bundled font and saves that choice, retaining the renderer; failures must
preserve the existing asset and restore selection where possible. Share the
metadata presentation between download confirmation and retained Info.

Validate metadata persistence/deduplication and active/inactive deletion in JVM
tests; exercise retained Info and confirmed deletion on an emulator fixture.

## Results

Implemented the empty catalog, selection/confirmation dialogs, progress/cancel
UI and HTTPS download-to-import path. Streaming no longer holds the store's
preference lock. The confirmation includes all requested metadata and keeps
the existing renderer selection when activating a completed download.

Library management now stores the descriptor beside the downloaded asset and
shares its Info UI with download confirmation. The shared selector offers
confirmed deletion of user assets; active deletion selects the bundled bank
before removing the file. Download metadata survives local reimport, process
restart, settings resets and removal of entries from the shipped catalog.
Installed UI validation exposed activity-only automation; its button discovery,
clicks and scrolling now target the focused window on API 29+, including Compose
dialogs, using public WindowInspector. Older devices retain activity automation.

Validation: 14 targeted JVM tests pass (five downloader integration tests with
multiple failure cases, nine store/preference tests). Scoped formatting
passes, and the all-ABI Android debug APK builds without new compiler warnings.
Installed-app empty-catalog automation passes all six steps on emulator-5554.
Installed library management passes all 19 UI steps, including retained Info,
cancel, active deletion, preserved renderer and stopped preview. The runner also
verifies file removal and bundled selection after process restart, and restores
the prior asset manifest and MIDI preferences. Final library evidence is in
`temp/soundfont-library/device-complete/`; its build/JVM logs are in the parent
directory. Initial catalog evidence is in `temp/soundfont-downloads/`. APK is the normal
`android/app/build/outputs/apk/debug/app-debug.apk`.

No live release asset is tested because the production catalog is deliberately
empty. Hosted-asset and populated confirmation-dialog checks belong with the
first real entries. Reproduction and descriptor examples are documented in
`android/tests/soundfont-downloads.md`.
