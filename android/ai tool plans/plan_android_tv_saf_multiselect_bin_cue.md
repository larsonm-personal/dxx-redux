# Android TV SAF picker: multi-select for BIN+CUE import

## Bug
On Android TV, the "Select Game Files or Archive to Import" button on the Setup
screen launches the SAF picker via `ActivityResultContracts.OpenMultipleDocuments()`.
On a phone this picker exposes checkboxes / long-press to multi-select, so the
user can grab a `.cue` and matching `.bin` together. On Android TV (tested on
Shield), every press of the controller's A / Select button immediately commits
the focused file as a single-selection result and dismisses the picker. There
is no long-press semantic and no visible multi-select affordance.

Net effect: CUE/BIN disc imports are impossible from the TV picker because the
two files cannot be returned in a single result, and the existing pairing logic
(see [SetupActivity.kt](android/app/src/main/java/com/dxxredux/app/SetupActivity.kt#L3340-L3386))
needs both URIs in one batch.

## Why it works on phone, not on TV
`OpenMultipleDocuments` already sets `EXTRA_ALLOW_MULTIPLE` and we use
`ACTION_OPEN_DOCUMENT`, so the app side is correct. The blocker is the picker
UI:

- The TV variant of DocumentsUI (and OEM replacements like NVIDIA's on Shield)
  strips multi-select. There is no checkbox column and no long-press on a
  D-pad remote.
- Even when `EXTRA_ALLOW_MULTIPLE` is set, the TV picker treats DPAD_CENTER as
  "open / select-and-finish", returning a single-item result.
- Some providers also fail to advertise `Document.FLAG_SUPPORTS_MULTIPLE`,
  which compounds the issue, but the dominant cause is the TV picker UI.

This is not fixable from inside our app by tweaking intent flags. The fix has
to route around the system picker on TV.

## Design constraints
- Keep the phone code path unchanged. It works today and the regular SAF
  picker is the right UX on a touchscreen.
- Reuse the existing post-pick routing in [SetupActivity.kt](android/app/src/main/java/com/dxxredux/app/SetupActivity.kt#L3231-L3399)
  (the big `for (uri in uris)` switch that classifies CUE / BIN / ISO / GOG /
  SOW / audio / zip). The TV path should produce a `List<Uri>` shaped exactly
  the same way and feed the same scope.launch block.
- No backwards compatibility required (per repo policy: full APK rebuild per
  release pre-launch).
- Must work with the launcher's existing controller / D-pad focus model
  (see `plan_launcher_dpad_focus_fix.md` and
  `plan_android_tv_controller_support.md`).
- Long term: the launcher will be extracted as a library, so any new picker
  should live in a self-contained file under `android/app/src/main/java/com/dxxredux/app/`
  with no game-specific assumptions baked in beyond the file-type filters.

## Options considered

### A. Pick CUE only, parse it, then prompt for BIN
Parse the CUE text after the user picks it, extract the `FILE "FOO.BIN" BINARY`
reference, then auto-launch a second `OpenDocument` picker pre-filtered to that
filename.

Pros: minimal new UI, works on any picker.
Cons: SAF on TV does not let us pre-seed the second picker to a directory or
pre-select a filename. The user has to navigate back to the same folder a
second time. Two picker round-trips on a remote is painful but workable.
Also: GOG `.gog` / `.inst` pairs would need the same treatment.

### B. ACTION_OPEN_DOCUMENT_TREE (pick a directory, scan inside)
Let the user pick the parent directory once, persist the tree URI, then the
app uses `DocumentFile.fromTreeUri(...).listFiles()` to find CUE/BIN/ISO/etc.
inside.

Pros: one round-trip on the remote. Works the same on phone and TV. Produces
a `List<Uri>` we can feed straight into the existing classifier. Also solves
future use cases (drop a folder of mods, audio packs, etc.).
Cons: Tree URIs grant broader access than single-file URIs; need to be
careful to drop the persisted permission once the import completes (or at
least scope it to one-shot reads). Some providers list contents very slowly
over `DocumentsContract`.

### C. Custom in-app file browser
Build a Compose-based file list that walks a tree URI we obtained via
`OPEN_DOCUMENT_TREE`, with checkboxes and proper D-pad focus.

Pros: full control of UX, multi-select works regardless of picker quirks.
Cons: real work to build and maintain, and we still need the user to grant
a tree URI first, which is just option B with extra steps. Not worth it
unless option B fails in practice.

### D. Zip workaround
Tell TV users to zip their CUE+BIN together and import the zip. The existing
zip extractor already handles disc images.

Pros: zero code change.
Cons: bad UX, fails for users who pulled files off a CD onto a USB stick on
the Shield itself with no PC handy.

## Recommendation
Instead of one hard-coded path per device class, present a small chooser
when the user taps the Import button and offer the picker style that fits
the situation:

- **Phone**: chooser shows `Pick multiple files` (default) and `Pick a folder`.
  Phone users keep the familiar multi-select picker but can fall back to a
  folder pick when their picker app misbehaves or when they have a whole
  install dump to import.
- **Android TV**: chooser shows `Pick single file` (default) and `Pick a
      folder`. The direct picker still uses `OpenMultipleDocuments`, but on the
      tested TV picker that contract already collapses to a single-file result,
      so no separate `OpenDocument` launcher is needed. TV users still get the
      folder-scan path for CUE/BIN dumps and a direct single-file path for GOG
      `.exe` installers, `.sow` archives, single `.zip` / `.iso` files, and
      other one-off imports.

Two actual picker launchers, with the direct launcher relabeled by device
class, all feeding the same `processPickedUris(...)` downstream classifier:

1. Shared direct picker: `OpenMultipleDocuments` -> `List<Uri>`
       - Phone label: `Pick Multiple Files`
       - TV label: `Pick Single File`
2. Folder picker: `OpenDocumentTree` -> enumerate children -> `List<Uri>`

Option D (zip workaround) stays as a documented escape hatch but is not the
primary path. Option C (custom file browser) is still deferred unless
the folder-scan path turns out to be too slow on Shield.

- [x] Add `fun Context.isAndroidTv(): Boolean` in
      `android/app/src/main/java/com/dxxredux/app/TvDetection.kt`.
### Phase 1: TV detection helper
- [ ] Add `fun Context.isAndroidTv(): Boolean` in a new file
- [x] Helper is now reused by the import chooser and available for future TV-aware UI.
      Use `UiModeManager.currentModeType == Configuration.UI_MODE_TYPE_TELEVISION`
      with a fallback to `packageManager.hasSystemFeature(PackageManager.FEATURE_LEANBACK)`.
- [x] Lift the `scope.launch(Dispatchers.IO) { ... }` body currently inside

      [SetupActivity.kt](android/app/src/main/java/com/dxxredux/app/SetupActivity.kt#L3231-L3399)
      into a private function `processPickedUris(uris: List<Uri>)` (still a
      the `OpenMultipleDocuments` callback in
      [SetupActivity.kt](android/app/src/main/java/com/dxxredux/app/SetupActivity.kt#L3231-L3399)
- [x] No intended behavior change in the direct picker path; verified by a
      focused Android Gradle test after the refactor.

### Phase 3: Add the shared direct picker and folder picker
Both launchers now live next to each other in the same Composable so they
share the state captured by `processPickedUris`.

- [x] **Shared direct picker**: keep `filePickerLauncher` using
      `OpenMultipleDocuments`. Callback feeds `processPickedUris(uris)`.
      The chooser relabels this path as `Pick Multiple Files` on phone and
      `Pick Single File` on TV.
- [x] **Folder picker**: add `dirPickerLauncher` using
      `ActivityResultContracts.OpenDocumentTree()`. On a non-null tree URI,
      enumerate children with `DocumentsContract` (not `DocumentFile`, which
      is not present in this module), filter to the extensions the classifier
      accepts, and call `processPickedUris(uris)`.
- [ ] Keep persisted permission scoped: take any required permission with
      `takePersistableUriPermission` for the lifetime of the import, and
      release with `releasePersistableUriPermission` once the imported files
      have been copied into app-private storage.
- [x] If either picker callback returns `null` or an empty URI list, do nothing.

### Phase 4: Picker chooser dialog
- [x] Replace the single-launcher `onClick` at
      [SetupActivity.kt](android/app/src/main/java/com/dxxredux/app/SetupActivity.kt#L4283-L4287)
      with a `var showImportChooser by remember { mutableStateOf(false) }`
      that the button toggles on.
- [x] Render an `AlertDialog` with a column of two large buttons plus `Cancel`.
      The button set depends on `context.isAndroidTv()`:
      - Phone: `Pick Multiple Files` (default-focused) and `Pick Folder`.
      - TV: `Pick Single File` (default-focused) and `Pick Folder`.
- [x] Each chooser button dismisses the dialog and launches the matching
      launcher:
      - Direct picker -> `filePickerLauncher.launch(arrayOf("application/octet-stream", "application/zip", "*/*"))`
      - Folder picker -> `dirPickerLauncher.launch(null)`
- [x] Make sure the dialog is reachable and operable with a controller
      D-pad by reusing the existing TV button wrappers and focusing the
      direct picker choice by default.
- [x] Update the help text below the import button so it reflects the
      device-specific direct-picker label plus the folder option.
- [x] Verify the dialog's button labels are short enough to render on a TV
      and clear enough on a phone.
- [ ] Update the help text below the import button so it lists what the
      chooser will offer, e.g. "Pick multiple files, a single file, or a
- [x] Direct picker path: when the user picks one CUE through the TV-labeled
      direct picker, the existing classifier still surfaces the same
      "requires a matching BIN" warning Toast. Same for an unpaired BIN,
      `.gog`, or `.inst`, because the direct path still flows through the
      shared classifier.
- [ ] Single-file path: when the user picks one CUE through the single-file
      launcher, surface the existing "requires a matching BIN" warning Toast
      already produced by the classifier. Same for an unpaired BIN, `.gog`,
      or `.inst`. No new logic - the refactor in Phase 2 wires this for
      free.
- [ ] Directory path: if a directory contains too many files (say > 200),
      warn the user and skip files that don't match a known extension before
- [x] Directory path: if neither a CUE+BIN pair nor any other recognized
      files are found, surface a clear "no importable files found in selected
      folder" Toast rather than silently doing nothing.
      warning and using only the first CUE that has a matching BIN. Reuse
      the existing warning Toast machinery.
- [ ] Add a unit test for the directory-scan filter helper, fed with SAF tree
      rows, verifying that CUE+BIN, ISO, GOG installer, SOW, and audio-only
      directories each produce the expected filtered URI list (and that
      unknown extensions are dropped).
- [x] Add a unit test for the chooser button-set selector / label helper.
      Implemented as `ImportChooserConfigTest`, which verifies the phone path
      exposes `Pick Multiple Files` and the TV path exposes `Pick Single File`.
      filtered URI list (and that unknown extensions are dropped).
- [ ] Add a unit test for the chooser button-set selector that asserts the
      phone branch returns `[multi, directory]` and the TV branch returns
      `[directory, single]` (whatever pure helper exposes the option list -
      extract one if needed to keep the test small).
- [ ] Extend the launcher integration test suite (see
      `android/run_all_tests.ps1`) with a TV-mode smoke test that mocks
      `isAndroidTv()` returning true, taps Import, and verifies the chooser
      shows the TV option set with the correct default focus. A second case
      with `isAndroidTv()` returning false verifies the phone option set.
- [ ] Manual verification on Shield:
      - Single-file route: pick a `.exe` GOG installer, confirm the GOG
        import dialog appears.
      - Directory route: place a `descent2.cue` + `descent2.bin` in a
        USB-mounted folder, hit Import, pick the folder, confirm both files
        route into the disc-import dialog.
- [ ] Manual verification on phone:
      - Multi-file route (default): confirm the multi-select picker still
        appears and CUE/BIN selection still works there (regression check).
      - Directory route: pick a folder with mixed game files, confirm it
        routes correctly.

### Phase 7: Cleanup / docs
- [ ] Mark the bug as fixed in
      [outstanding_bugs.md](android/outstanding_bugs.md).
- [x] Run `android\run-code-quality.ps1 -Fix` on the touched Kotlin files and
      let it fully exit.
- [x] Run `run-windows-build.ps1 -Target d2` to confirm the host build is
      still green (no C-side changes expected, but check anyway).
- [x] Run a focused Android Gradle validation task:
      `:app:testDebugUnitTest --tests com.dxxredux.app.ImportChooserConfigTest`.

## Open questions
- Resolved for this tranche: phone now exposes the folder path as a secondary
  option in the chooser, while keeping the existing multi-file direct picker
  as the default.
- Should we also persist the tree URI across launches so users don't have to
  re-pick the folder for repeat imports? Per repo policy, no backwards-compat
  is required pre-launch, but persisting one tree URI in SharedPreferences is
  cheap and probably worth it. Decide during Phase 2 implementation.
- Does the GOG `.gog`/`.inst` pair work via directory pick? It should, since
  the classifier already pairs them by extension; verify on Shield with a real
  GOG install dump.
