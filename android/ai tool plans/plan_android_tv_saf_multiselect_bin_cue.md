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
- **Android TV**: chooser shows `Pick a folder` (default) and `Pick a single
  file`. TV users get the folder-scan path for CUE/BIN dumps, but can still
  reach single-file import for things like a GOG `.exe` installer, a `.sow`
  archive, a single `.zip`, a single `.iso`, or a single audio file - all of
  which work fine with the crippled TV picker because they only need one
  URI.

Three picker routes total, all feeding the same `processImportUris(...)`
downstream classifier:

1. `OpenMultipleDocuments` -> `List<Uri>`  (phone only)
2. `OpenDocument` -> `Uri?` wrapped as a 1-element list  (TV only, also
   useful as a future phone fallback)
3. `OpenDocumentTree` -> enumerate children -> `List<Uri>`  (both)

Option D (zip workaround) stays as a documented escape hatch but is not the
primary path. Option C (custom file browser) is still deferred unless
Option 3 turns out to be too slow on Shield.

## Implementation plan

### Phase 1: TV detection helper
- [ ] Add `fun Context.isAndroidTv(): Boolean` in a new file
      `android/app/src/main/java/com/dxxredux/app/util/TvDetection.kt`.
      Use `UiModeManager.currentModeType == Configuration.UI_MODE_TYPE_TELEVISION`
      with a fallback to `packageManager.hasSystemFeature(PackageManager.FEATURE_LEANBACK)`.
- [ ] No tests needed yet; reused by import button and any future TV-aware UI.

### Phase 2: Refactor the import pipeline
- [ ] Lift the `scope.launch(Dispatchers.IO) { ... }` body currently inside
      the `OpenMultipleDocuments` callback in
      [SetupActivity.kt](android/app/src/main/java/com/dxxredux/app/SetupActivity.kt#L3231-L3399)
      into a private function `processImportUris(uris: List<Uri>)` (still a
      Composable-scoped lambda capturing the existing `mutableStateOf` setters
      and `scope`). All three launchers call it with the same shape of input.
- [ ] No behavior change in this phase; this is a pure refactor that the
      following phases build on. Verify the existing phone multi-pick path
      still routes CUE+BIN/ISO/GOG/SOW correctly after the move.

### Phase 3: Add the three picker launchers
All three live next to each other in the same Composable so they share the
state captured by `processImportUris`.

- [ ] **Multi-file** (existing): keep `filePickerLauncher` using
      `OpenMultipleDocuments`. Callback feeds `processImportUris(uris)`.
- [ ] **Single-file** (new): add `singleFilePickerLauncher` using
      `ActivityResultContracts.OpenDocument()`. On a non-null result, wrap
      it as `processImportUris(listOf(uri))`. Same MIME filter as the
      multi-file launcher.
- [ ] **Directory** (new): add `dirPickerLauncher` using
      `ActivityResultContracts.OpenDocumentTree()`. On a non-null tree URI,
      enumerate children with `DocumentFile.fromTreeUri(context, treeUri)
      ?.listFiles()`, filter to the extensions the classifier accepts
      (`.cue`, `.bin`, `.iso`, `.gog`, `.inst`, `.hog`, `.ham`, `.pig`,
      `.zip`, `.7z`, `.exe`, `.pkg`, `.sow`, `.dem`, audio files), and call
      `processImportUris(uris)`.
- [ ] Keep persisted permission scoped: take any required permission with
      `takePersistableUriPermission` for the lifetime of the import, and
      release with `releasePersistableUriPermission` in a `finally` block
      once the imported files have been copied into app-private storage.
- [ ] If any picker callback returns `null` (user cancelled), do nothing.

### Phase 4: Picker chooser dialog
- [ ] Replace the single-launcher `onClick` at
      [SetupActivity.kt](android/app/src/main/java/com/dxxredux/app/SetupActivity.kt#L4283-L4287)
      with a `var showImportChooser by remember { mutableStateOf(false) }`
      that the button toggles on.
- [ ] Render an `AlertDialog` (or the project's existing themed dialog
      wrapper, if any) with a column of two large buttons. The button set
      depends on `context.isAndroidTv()`:
      - Phone: `Pick multiple files` (default-focused) and `Pick a folder`.
      - TV: `Pick a folder` (default-focused) and `Pick a single file`.
      Always include a `Cancel` row.
- [ ] Each chooser button dismisses the dialog and launches the matching
      launcher:
      - Multi-file -> `filePickerLauncher.launch(arrayOf("application/octet-stream", "application/zip", "*/*"))`
      - Single-file -> `singleFilePickerLauncher.launch(arrayOf("application/octet-stream", "application/zip", "*/*"))`
      - Directory -> `dirPickerLauncher.launch(null)`
- [ ] Make sure the dialog is reachable and operable with a controller
      D-pad: default focus on the recommended option, B/Back dismisses.
      Reuse the existing focus-helper patterns from
      `plan_launcher_dpad_focus_fix.md` so the dialog behaves like the rest
      of the launcher on TV.
- [ ] Update the help text below the import button so it lists what the
      chooser will offer, e.g. "Pick multiple files, a single file, or a
      folder containing the files to import".
- [ ] Verify the dialog's button labels are short enough to render on a TV
      and clear enough on a phone.

### Phase 5: Edge cases and warnings
- [ ] Single-file path: when the user picks one CUE through the single-file
      launcher, surface the existing "requires a matching BIN" warning Toast
      already produced by the classifier. Same for an unpaired BIN, `.gog`,
      or `.inst`. No new logic - the refactor in Phase 2 wires this for
      free.
- [ ] Directory path: if a directory contains too many files (say > 200),
      warn the user and skip files that don't match a known extension before
      classification, to keep import responsive.
- [ ] Directory path: if multiple CUE files are present in one folder, the
      current classifier code assumes one CUE; preserve that behavior by
      warning and using only the first CUE that has a matching BIN. Reuse
      the existing warning Toast machinery.
- [ ] Directory path: if neither a CUE+BIN pair nor any other recognized
      files are found, surface a clear "no game files found in selected
      folder" Toast rather than silently doing nothing.

### Phase 6: Tests
- [ ] Add a unit test for the directory-scan filter helper, fed with a mock
      list of `DocumentFile` names, verifying that CUE+BIN, ISO, GOG
      installer, SOW, and audio-only directories each produce the expected
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
- [ ] Run `android\run-code-quality.ps1 --fix` and let it fully exit.
- [ ] Run `run-windows-build.ps1 -Target d2` to confirm the host build is
      still green (no C-side changes expected, but check anyway).

## Open questions
- Do we want the same TV directory path on phone too, as a power-user option?
  Probably not by default, but it could be a small toggle in the launcher's
  advanced tab. Defer until we hear it asked for.
- Should we also persist the tree URI across launches so users don't have to
  re-pick the folder for repeat imports? Per repo policy, no backwards-compat
  is required pre-launch, but persisting one tree URI in SharedPreferences is
  cheap and probably worth it. Decide during Phase 2 implementation.
- Does the GOG `.gog`/`.inst` pair work via directory pick? It should, since
  the classifier already pairs them by extension; verify on Shield with a real
  GOG install dump.
