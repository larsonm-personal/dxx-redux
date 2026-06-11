# Descent Maximum Two-Pack ZIP View Design

## Goal

Make parent mission ZIPs that mainly contain child mission ZIPs behave as archive containers:

- Parent ZIP view is minimal and shows only child ZIP buttons.
- Tapping a child ZIP opens the full mission ZIP details view for that child.
- Child ZIP details show the normal mission summary, files, readme, music, and level metadata buttons.
- Child ZIPs must not jump straight to level metadata from the parent view.
- The existing direct mission ZIP behavior should stay unchanged.

## Current Behavior

- `MissionZip.inspect(file)` only returns a `ScanResult` when the archive itself has a mission descriptor and HOG or DXA assets.
- `MissionZip.isImportCandidate(input)` treats a ZIP with a `rebirth` child ZIP as importable.
- `ModManager.importMissionZipFile()` currently falls back to `importNestedRebirthMissionZip()` when the parent is not a direct mission ZIP.
- That fallback extracts and registers a single child, which is correct for Enemy Within's Rebirth-vs-XL package but wrong for a two-pack where both children should remain selectable.
- `ModDetailsDialog()` renders a full mission ZIP view whenever `details.missionZip != null`.
- File buttons in that view always open `MissionZipConstituentDialog()`, and that dialog may offer level metadata.

## Design

### 1. Add a container scan result

Extend `MissionZip` with child archive metadata, separate from normal mission constituents:

```kotlin
data class NestedArchive(
    val path: String,
    val name: String,
    val sizeBytes: Long,
    val compressedSizeBytes: Long,
    val scan: ScanResult?,
)

data class ContainerResult(
    val children: List<NestedArchive>,
    val totalSizeBytes: Long,
)
```

Add:

```kotlin
fun inspectContainer(file: File): ContainerResult?
fun inspectNestedZip(file: File, childPath: String): ScanResult?
```

Rules:

- A container result exists only when the parent has one or more child `.zip` entries whose contents are valid mission ZIPs.
- If the parent itself is a valid direct mission ZIP, keep direct mission ZIP behavior. Do not reinterpret ordinary mission ZIPs as containers just because they include some extra ZIP.
- For Descent Maximum style packages, both valid children appear.
- For Enemy Within style packages, the parent can also appear as a container if opened directly in the details UI, but import behavior can still choose the Rebirth child when that is the known desired install path.

Implementation detail:

- Use temp files under `cacheDir` or an app private temp directory for nested scan extraction. Do not read large child ZIPs fully into memory.
- Limit nested scan to one level for now. A child ZIP gets a full view, but child-of-child ZIPs are not recursively expanded unless a later package requires it.

### 2. Extend mod details with container mode

Add a field to `ModManager.ModDetails`:

```kotlin
val missionZipContainer: MissionZip.ContainerResult? = null
```

Update `getMissionZipDetails()`:

1. Try `MissionZip.inspect(modFile)`.
2. If direct scan succeeds, return the current full mission ZIP details.
3. If direct scan fails, try `MissionZip.inspectContainer(modFile)`.
4. If container scan succeeds, return a details object with:
   - `fileCount = container.children.size`
   - `categories = emptyList()`
   - `missionZip = null`
   - `missionZipContainer = container`
   - no top-level mission metadata
5. If both fail, keep the current problem message.

This keeps the parent view from trying to synthesize metadata for the parent archive.

### 3. Make the parent UI intentionally minimal

In `ModDetailsDialog()`:

- If `details.missionZipContainer != null`, render only:
  - archive path
  - archive size
  - game/state rows if useful
  - a `Packages` or `Mission ZIPs` section with one button per child ZIP
- Suppress:
  - top-level level metadata buttons
  - readme button
  - music button
  - mission section
  - files/categories/patch/base-file sections
  - feature notes

Child button subtitle should be simple:

- Valid child: mission title and size, for example `Descent Maximum (fixed), 42 MB`
- Invalid/unreadable child, if shown: `Could not read mission ZIP`

Prefer showing only valid child ZIPs for this first pass, since the user asked for buttons for sub ZIPs rather than a forensic parent archive browser.

### 4. Open child ZIPs as full ZIP views

Add dialog navigation state:

```kotlin
var childDetailsTarget by remember { mutableStateOf<MissionZip.NestedArchive?>(null) }
```

When a child is clicked:

- Extract that child ZIP to a temp file.
- Build a child `ModDetails` from the child scan.
- Open the same details body component with:
  - title = child mission title or child ZIP name
  - archivePath = temp child ZIP path
  - missionZip = child scan
  - missionZipMusic = MissionZipMusic.inspect(tempChildZip)

Best structure:

- Split `ModDetailsDialog()` into a thin shell and a reusable `ModDetailsContent()`.
- The shell owns parent/child dialog state.
- The content receives `displayName`, `details`, `setDir`, and callbacks.

The important behavior is that a child ZIP view is indistinguishable from a normal imported mission ZIP details view. Its own top-level metadata buttons target the child archive, not the parent.

### 5. Keep constituent clicks scoped to the currently viewed archive

For the child full view:

- File buttons still open `MissionZipConstituentDialog()`.
- Level metadata buttons still come from `LevelMetadataTargets.missionZipTargets(childArchivePath, setDir, childScan)`.
- `MissionZipConstituentDialog()` should only be used for constituents inside the current archive.

For the parent container view:

- Child ZIP buttons must not call `MissionZipConstituentDialog()`.
- Child ZIP buttons must not call `LevelMetadataTargets.zipConstituent()`.

This is the part that prevents the current "straight to level metadata" feel.

### 6. Import behavior decision

Recommended for now:

- Preserve existing `importNestedRebirthMissionZip()` behavior for packages with exactly one child ZIP whose name contains `rebirth`.
- Add a new branch for multi-child mission ZIP containers:
  - Store the parent ZIP as the mod.
  - Register it as `kind = mission_zip`.
  - Set `missionTitle = null` or a generated name from the parent filename.
  - Set `importMode = "zip_container"`.

This avoids flattening the Descent Maximum two-pack into one of its children, and allows the launcher details view to offer both sub packages.

Longer term, a child could be enabled/disabled independently, but that is not required for the requested details view change.

### 7. Tests

Add or update JVM tests:

- `MissionZipTest`
  - `detectsTwoPackContainerWithTwoMissionZipChildren`
  - `directMissionZipWithExtraZipStillScansAsDirectMissionZip`
  - `containerIgnoresPlainNestedZip`
- `ModManagerMissionZipTest`
  - `twoPackMissionZipImportsParentContainer`
  - `twoPackParentDetailsExposeOnlyChildArchives`
  - `childArchiveDetailsUseFullMissionZipScan`
  - keep `parentMissionZipImportsRebirthChild` passing unless the intended import behavior changes.

If UI tests are available for setup dialogs, add one high-level automation assertion:

- Parent details JSON or UI state has child buttons.
- Parent has zero level metadata targets.
- Opening a child exposes normal mission metadata targets.

## Implementation Phases

- [ ] Add `MissionZip.ContainerResult` and nested archive scanning helpers.
- [ ] Extend `ModManager.ModDetails` and import registration for multi-child containers.
- [ ] Refactor `ModDetailsDialog()` into reusable parent and child details content.
- [ ] Add tests for direct ZIPs, Rebirth single-child imports, and Descent Maximum two-pack behavior.
- [ ] Run scoped code quality for touched Kotlin files and relevant Gradle unit tests.

## Open Questions

- Should a two-pack container be launchable as a combined mod, or should launch require opening/enabling a specific child package?
- Should the parent mod list row display the parent filename, or a synthesized title such as `Descent Maximum two-pack`?
- Should invalid child ZIPs be hidden or shown with an error row?

Recommended answers for first implementation:

- Keep launch semantics conservative and treat the parent as a selectable container only.
- Use the parent filename-derived display name.
- Hide invalid child ZIPs unless all children are invalid, then show the existing unreadable archive problem.
