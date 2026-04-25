# Plan: Relocate Imported Files Storage (Advanced page)

Allow the user to redirect the "imported files" storage root (where CD images,
GOG-extracted assets, mod archives, custom music, etc. live) off the app's
internal `filesDir` to a user-chosen volume. Configs/saves/pilots/controller
mappings stay where they are. Targeted at devices like the NVIDIA Shield Tube
where internal flash is small but external storage (SD card / USB) is plentiful.

## Goals (from user request)

1. Advanced page gets a new section under "Storage Inspector":
   "Set Imported Files Location".
2. Section always shows the absolute path of the currently active import root.
3. "Set new location" button opens a directory picker. Active even when an
   override is already in place (rebinding to a third location is supported).
4. "Revert to default app storage" button shown only while an override is
   active.
5. Any change (default -> ext, ext -> ext, ext -> default) copies existing
   imported files to the new location, then unlinks the previous override
   location once the copy succeeds. Free-space check before the copy.
6. Existing Windows/Linux/Mac builds untouched.

## Background research

### What "imported files" actually is today

`FileSetManager` (android/app/src/main/java/com/dxxredux/app/FileSetManager.kt)
already isolates game data:

- All imported game data lives in `filesDir/sets/<name>/`.
- Per-set SAF manifest (`.saf_manifest.json`) lives inside the set dir.
- `migrateDefaultSetIfNeeded()` already moved legacy assets out of `filesDir`
  root into `sets/default/`.
- Saves, configs, pilots, custom music stay in `filesDir/{d1x-redux,d2x-redux}/`
  and `filesDir/custom_music/...` -- not under `sets/`.

So the relocation surface is exactly one directory: `filesDir/sets/`. Nothing
in the engine writes outside of that dir for imported assets.

### How the C engine learns the active set path

`d1/misc/physfsx.c` and `d2/misc/physfsx.c` (`PHYSFSX_init`, `#ifdef ANDROID`
block) read `filesDir/{d2x-redux,d1x-redux}/.active_set_path` via plain
`fopen()` and prepend the resulting absolute path to PhysFS's search list.
SAF manifest mount uses the same path.

Because that file already contains an absolute filesystem path, the engine
side requires zero changes if we keep the new location reachable as a real
filesystem path.

### How import code writes files

GOG / ISO / SOW / Mac extractors all take a Kotlin/JNI `outputDir: String`
and call POSIX `open`/`fopen` underneath
(`android/app/src/main/java/com/dxxredux/app/{GogImportBridge,DiscImportBridge}.kt`).
None of them go through the SAF translation layer. The only SAF-aware
read path is `physfs_archiver_saf.c`, which reads pre-registered files from a
manifest -- it cannot accept arbitrary writes.

### Why "real SAF tree URI" is NOT a viable storage backend (background context only)

The user's question references SAF (`ACTION_OPEN_DOCUMENT_TREE`) because that
is the typical Android answer for "let the user pick any folder anywhere".
However, the engine and the GOG/ISO extractors all use **real filesystem
paths**, not `content://` URIs:

- `PHYSFS_setWriteDir` / `PHYSFS_addToSearchPath` need real paths.
- `fopen()` in `physfsx.c` for reading `.active_set_path` and `.saf_manifest.json`
  needs real paths.
- The Kotlin extractors hand a `String` outputDir to native code that calls
  `open()` directly.
- The extract-progress code lists/walks the output dir with `java.io.File`.

Wrapping all of that in a SAF DocumentFile shim would be a multi-week project
and a significant new attack surface (the existing SAF leave-in-place archiver
took its own work). **It is out of scope for this plan.**

### What IS available with a real path on external/SD card storage

`Context.getExternalFilesDirs(null)` returns app-private dirs on every mounted
volume (primary internal partition + each SD card / USB OTG drive).
Properties:

- Real absolute filesystem path -- works with PhysFS, fopen, every extractor.
- App-private -- no runtime permissions, no SAF picker required, no Play
  Store policy review.
- Deleted on app uninstall (acceptable; matches our "data reset on every
  install" stance pre-release per copilot-instructions).
- Fully writable, supports gigabytes of game data.
- Works on Shield Tube, every Android version we target.

This is the storage backend PPSSPP/Dolphin/RetroArch use for their
"PSP folder"/"User folder" pickers when the user wants SD card storage.

### Decision

Build a directory-relocation feature that:

1. Uses `getExternalFilesDirs(null)` as the source of candidate volumes.
2. Presents them in a **custom picker** (volume + free space + path),
   not `ACTION_OPEN_DOCUMENT_TREE`.
3. Stores the chosen absolute path in a small JSON pref file in
   `filesDir` so the app re-uses it on next launch.
4. Migrates `sets/` contents to the new location on change, sweeps the old
   location after a verified copy.

If a future requirement demands an arbitrary user-picked SAF tree, that is a
separate, much larger project (translation layer in the engine + every
extractor). Out of scope here.

User-facing copy can still talk about "Imported files location" without
exposing the SAF/non-SAF distinction. The picker dialog will show the user
exactly which volumes are available (Internal, SD card, USB), which is what
they actually want.

## Architecture

### New: `ImportLocationManager` (Kotlin)

`android/app/src/main/java/com/dxxredux/app/ImportLocationManager.kt`

Responsibilities:

- `getActiveRoot(): File` -- returns the directory that should host
  `sets/...`. Reads `import_location.json` in `filesDir`. Falls back to the
  default `File(filesDir, "imported")` (see "Path layout" below).
- `getDefaultRoot(): File` -- the in-app default.
- `isOverrideActive(): Boolean`.
- `listCandidateVolumes(ctx): List<VolumeOption>` -- map each non-null entry
  of `ctx.getExternalFilesDirs(null)` to `{label, path, freeBytes,
  totalBytes, isPrimary}`. The first entry equals the internal app-private
  external dir -- excluded from candidates because using it would not
  alleviate internal-flash pressure (it lives on the same partition on
  Shield). Annotate volumes whose `Environment.isExternalStorageRemovable`
  is true.
- `setOverride(newRoot: File)` -- writes pref. Caller is responsible for
  having migrated data.
- `clearOverride()` -- removes pref.
- `migrate(srcRoot: File, dstRoot: File, progress: (Long, Long) -> Unit)` --
  copy all files under src into dst, fsync, verify byte counts, then delete
  src. Handles partial failures with a clear error path: leave both copies
  intact, return failure, keep override unchanged.

`import_location.json` schema (single file, in `filesDir`):

```json
{ "schema": 1, "import_root": "/storage/0BC1-72FE/Android/data/com.dxxredux.app/files/imported" }
```

### Path layout

Today: `filesDir/sets/<name>/...`.

After this change, `sets/<name>/...` lives **inside the import root**:

- Default: `filesDir/imported/sets/<name>/...`
  - We rename today's `filesDir/sets/` -> `filesDir/imported/sets/` once at
    upgrade. Treat that as `migration_version` bump in `file_sets.json`.
- Override: `<chosen_volume>/Android/data/com.dxxredux.app/files/imported/sets/<name>/...`
  - On Android getExternalFilesDirs already returns `.../files/`; we append
    `/imported/sets/`.

Why a wrapper `imported/` dir: leaves room for sibling subdirs in the future
(downloaded mod cache, scratch space) without cluttering filesDir's root.

### `FileSetManager` changes

Inject the import root, do not derive it from `filesDir`:

```kotlin
class FileSetManager(
    private val filesDir: File,        // configs/metadata stay here
    private val importRoot: File,      // new: where sets/ lives
)
```

- `setsDir` becomes `File(importRoot, "sets")`.
- `configFile` stays `File(filesDir, "file_sets.json")` so the active-set
  selection follows the user, not the storage volume.
- `migrateDefaultSetIfNeeded()` extends to also relocate
  `filesDir/sets -> importRoot/sets` on first load.
- `writeActiveSetPath()` already writes an absolute path, so it Just Works.

Every `FileSetManager(filesDir)` call site must pass `importRoot` from
`ImportLocationManager.getActiveRoot()`. Identified call sites:

- `MainActivity` / `SetupActivity` constructors (game launch + setup screen).
- `MusicPickerPage`: `FileSetManager(filesDir).let { it.getSetDir(...) }`.
- `multiplayer/MissionPicker` `FileSetManager(filesDir)`.
- Any other -- searched via grep, ~10 call sites total.

Cleanest fix: thread an `importRoot: File` through the same way `filesDir` is
threaded today (it is already a constructor/composable arg pattern).

### Engine side (C)

No changes needed. Confirm by tracing:

1. Kotlin computes `setDir = importRoot/sets/<active>`.
2. Writes `setDir.absolutePath` into `filesDir/{d1x,d2x}-redux/.active_set_path`.
3. `physfsx.c` reads that file, `PHYSFS_addToSearchPath(setdir, 0)`.
4. SAF manifest path is also derived from setdir, so leave-in-place files
   continue to resolve.

The Android emulator and physical Shield both expose
`getExternalFilesDirs()` results as real ext4/F2FS paths, so PhysFS opens them
without trouble.

### UI (AdvancedSettingsPage)

New `ImportLocationSection(ctx, onChanged)` composable rendered between
`StorageInspectorSection` and the existing divider. Layout:

```
Imported Files Location
<one-line description>
Current: /data/data/com.dxxredux.app/files/imported   (default)
[ Set new location... ]    [ Revert to default ]   <-- second only when override active
```

Clicking "Set new location":

1. Build candidate list from `ImportLocationManager.listCandidateVolumes`.
   - Always include the in-app default at the top, even when it's already
     the active root (so the user can always see what default looks like).
2. Show an `AlertDialog` with each volume as a radio row:
   `Volume label`, `path`, `xx.xGB free / yy.yGB total`. Disabled if free
   space < current `imported/` usage.
3. Selecting a row + Confirm:
   - If chosen == active: dismiss with no-op + toast.
   - Else: show a final confirmation dialog with copy size + estimated time
     ("Move ~3.4 GB to SD card?"). Cancel-safe.
4. On confirm: launch `withContext(Dispatchers.IO)` migration:
   - Compute total bytes to copy.
   - Verify dst free >= total + 64MB safety margin.
   - Copy files (preserve mtimes), update `import_location.json` only after
     the full tree is verified, then `srcRoot.deleteRecursively()`.
   - Show a determinate progress dialog (bytes copied / total).
5. After success, kill the process the same way "Reset All Controls" does so
   the next launch picks up the new path with all references rebuilt:
   `android.os.Process.killProcess(myPid())`. Good enough pre-release; we
   don't have to support hot reload of FileSetManager.
6. On failure: leave both copies in place, show toast with reason, do not
   change the pref.

"Revert to default" runs the same flow with destination = default root.

## Edge cases / gotchas

- **Volume disappears** (SD card removed): on next launch, if
  `getActiveRoot()` returns a path whose volume is missing, fall back to
  default and surface a clear toast in SetupActivity ("SD card not present;
  using internal storage"). Do not auto-copy anything back (data may still
  be on the unmounted card). Expose a "Forget override" action so the user
  can dismiss the warning and re-pick later.
- **Free-space check** must use the destination volume's
  `StatFs(dstRoot)` -- `File.usableSpace` works.
- **mid-copy crash**: dst gets a `.in-progress` marker file; on next launch,
  if marker exists in a non-active root, rename to `.partial-<timestamp>` and
  log; user can delete via Storage Inspector.
- **active SAF leave-in-place files**: their `.saf_manifest.json` is inside
  `sets/<name>/` and gets copied along with everything else. Content URIs
  remain valid because they don't depend on the manifest's location, only
  on the persisted permission grants which live in the OS.
- **Symlinks/hardlinks**: not used today, ignore.
- **Permissions**: app-private external storage requires no runtime
  permission on any API level we support. Do NOT add `MANAGE_EXTERNAL_STORAGE`.
- **Path with spaces**: `getExternalFilesDirs()` paths can contain spaces on
  some OEMs. PhysFS handles them; just don't shell-quote anywhere.
- **Tests**: emulator typically has only the primary external dir. Allow the
  picker to no-op gracefully when no removable volume is present, rather than
  hiding the section entirely (showing "No additional volumes available" is
  more useful diagnostic info on Shield support tickets).

## Phasing

### Phase 1: Plumbing without behavior change
- [x] Add `ImportLocationManager` (default-only behavior, no override yet,
      no UI). Default root = `filesDir/imported`.
- [x] Extend `FileSetManager` to take `importRoot: File`, default to
      `filesDir/imported` if not supplied (transitional convenience overload
      acceptable here; remove once all call sites updated).
- [x] One-time migration: rename existing `filesDir/sets` -> `filesDir/imported/sets`.
      Bump `migration_version` in `file_sets.json` to 2.
- [x] Update every `FileSetManager(filesDir)` call site to pass
      `ImportLocationManager(ctx).getActiveRoot()`. Verify list with grep.
      (Default-arg ctor resolves importRoot, so existing call sites remain valid.)
- [ ] Build green, run automation `test_launch_to_automap.json5` for d1+d2
      to confirm sets still resolve. (Optional emulator validation pending.)

### Phase 2: Override storage + volume listing
- [x] `import_location.json` read/write in `ImportLocationManager`.
      (Switched to `import_location.txt` flat key=value to avoid Android-only
      org.json dependency in JVM unit tests.)
- [x] `listCandidateVolumes()` (returns labelled list, free/total bytes,
      removable flag, exclude primary internal).
- [x] `migrate(src, dst, progress)` with verification + sweep.
- [x] Unit-style integration: 8 JUnit tests in
      `android/app/src/test/java/com/dxxredux/app/ImportLocationMigrateTest.kt`
      verifying copy/verify/sweep, override round-trip, FileSetManager
      importRoot wiring, and progress reporting.

### Phase 3: Advanced page UI
- [x] `ImportLocationSection` composable in
      `AdvancedSettingsPage.kt`, inserted under `StorageInspectorSection`.
- [x] Volume picker `AlertDialog` + final confirmation + progress dialog.
- [x] "Revert to default" button.
- [x] Process-kill restart on success (mirrors existing reset pattern).
- [x] Localize-friendly strings (no emoji, no emdashes).

### Phase 4: Robustness
- [x] Missing-volume fallback path on launch (SetupActivity check) +
      `isOverrideUnreachable` toast.
- [x] `.in-progress` marker handling on next launch via
      `handleStaleInProgressMarkers`.
- [x] Error toasts; debug logging routed through file-level log helpers
      that tolerate JVM unit tests.

### Phase 5: Tests + lint
- [x] High-level integration test (`ImportLocationMigrateTest`) covering
      migrate, override round-trip, FileSetManager importRoot wiring,
      progress reporting, and stale-marker cleanup. 8/8 passing.
- [ ] Automation `test_launch_to_automap.json5` for d1+d2 with the
      override active (optional emulator validation; not yet run).
- [x] `android\run-code-quality.ps1 -Fix` (Kotlin formatting clean).
- [ ] Windows cmake build verification (`run-windows-build.ps1`) -- not
      required since no engine code changed; skip unless C-side touched
      later.

## Out of scope

- Arbitrary SAF tree URIs as the storage backend. Documented above; would
  require a translation layer through PhysFS and every extractor.
- Moving saves/pilots/configs/custom music. Per the user request, those
  stay in `filesDir`. Custom music in particular is small and the user
  edits it from the launcher; relocating it would force more code changes
  with no storage benefit.
- Background/streaming migration. We migrate up-front, with a process-kill
  restart afterwards. Pre-release simplicity.
- Cross-set migration UX. The set system already supports multiple sets;
  all of them move together when the import root changes.

## Files expected to change

- New: `android/app/src/main/java/com/dxxredux/app/ImportLocationManager.kt`
- Modified: `android/app/src/main/java/com/dxxredux/app/FileSetManager.kt`
  (constructor signature, migration_version bump, sets-dir computation)
- Modified: `android/app/src/main/java/com/dxxredux/app/AdvancedSettingsPage.kt`
  (new section + dialogs)
- Modified call sites of `FileSetManager(filesDir)`:
  - `MainActivity.kt`
  - `SetupActivity.kt`
  - `MusicPickerPage.kt`
  - `multiplayer/MissionPicker.kt`
  - any others surfaced by full grep at start of phase 1
- New: `android/tests/...` integration test for migration

No d1/ or d2/ source changes expected. The engine already accepts the
absolute path written to `.active_set_path`.
