# Mission ZIP external readmes

## Goal

Support `.pdf`, `.rtf`, `.doc`, and `.docx` files in mission ZIPs as documentation and alternate readme candidates. Keep `.txt` readmes higher priority because they can be viewed inside the launcher. When a binary document is selected as the readme, the top-level `View readme` action should extract it to a cache file and open the Android system viewer. The same external view action should be available from constituent file details for those document types.

## Existing shape

- `MissionZip.inspect()` already scans ZIP entries into `MissionZip.Constituent`
- `MissionZip.ScanResult.readme` currently points at the chosen `.txt` constituent
- `MissionZip.sortedConstituents()` currently moves `.txt` files to the top of the file list
- `MissionZip.readTextFile()` is intentionally `.txt`-only and drives `MissionZipTextDialog`
- `GameFileFormats` already marks `.txt`, `.md`, and `.rtf` as documentation, but does not yet include `.pdf`, `.doc`, or `.docx`
- `SetupSections.kt` already exposes:
  - top-level `View readme`
  - constituent details popover
  - in-launcher text view for `.txt`
- `android/app/src/main/res/xml/file_paths.xml` already exposes `cache/file_view/` through `FileProvider`

## Design

### Cache cleanup

Do not delete the extracted file immediately after launching the external viewer. Android grants another app temporary read access to a `content://` URI, and some viewers open lazily after the launcher callback has already returned. Immediate cleanup can race the viewer.

Use opportunistic cleanup instead:

- Store extracted documents in `context.cacheDir/file_view/`
- Before writing a new extracted document, delete `file_view` files older than 24 hours
- After writing, prune oldest non-current files if the directory grows past a modest cap, such as 64 MB
- Never delete the just-created file during that prune pass
- Let Android's ordinary cache eviction handle pressure between launcher sessions

This keeps repeated testing/import browsing from accumulating stale PDFs, while preserving a long enough window for the external app to open and re-read the file.

### Documentation classification

Add a small, launcher-local document classification in `MissionZip`:

- inline text readme: `.txt`
- external document readme: `.pdf`, `.rtf`, `.doc`, `.docx`
- all readme-capable documents: inline text plus external documents

Also add `.pdf`, `.doc`, and `.docx` to `GameFileFormats` as documentation with `MISSION_ZIP_DOCUMENTATION`, and keep `.rtf` in that role. Suggested labels:

- `.pdf`: `PDF document`
- `.rtf`: existing `Rich text file`
- `.doc`: `Word document`
- `.docx`: `Word document`

### Readme selection

Replace the current `.txt`-only `chooseReadme()` with a two-tier selector:

1. Choose among `.txt` files using the existing rules
2. If no `.txt` exists, choose among `.pdf`, `.rtf`, `.doc`, and `.docx` using the same name/size rules

The common rule inside each tier should remain:

1. Single candidate wins
2. `README.<ext>` wins, case-insensitive
3. Candidate whose leaf name starts with the ZIP stem wins
4. Largest candidate wins

This keeps text readmes preferred even if a larger PDF exists. For Castaway-style packages with only a PDF readme, the PDF becomes `ScanResult.readme`.

### File list ordering

Change `sortedConstituents()` from `.txt` first to documentation first:

1. `.txt`
2. `.pdf`, `.rtf`, `.doc`, `.docx`
3. everything else by path

Within each group, keep path sorting. This makes document files easy to find without disturbing game asset ordering more than needed.

### External file extraction

Add a small extraction helper near `MissionZip` or in a new launcher utility file:

```kotlin
fun extractZipConstituentToCache(
    context: Context,
    archive: File,
    constituent: MissionZip.Constituent,
    cacheSubdir: String = "file_view",
): Uri
```

Behavior:

- Resolve the ZIP entry by `constituent.path`
- Write to `context.cacheDir/file_view/<safe display name>`
- Delete any existing file with the same cache name before writing
- Copy by stream, not through `readBytes()`
- Use `FileProvider.getUriForFile(context, "com.dxxredux.app.fileprovider", copy)`
- Preserve the original extension so Android can route to the right app
- Return a `content://` URI with read permission

Use a safe cache filename that keeps the leaf name but strips path separators. If duplicate names become a problem later, prefix with a short hash of `archive.absolutePath + ":" + constituent.path`.

### MIME and open behavior

Add a simple MIME mapper:

- `.txt`: `text/plain`
- `.pdf`: `application/pdf`
- `.rtf`: `application/rtf`
- `.doc`: `application/msword`
- `.docx`: `application/vnd.openxmlformats-officedocument.wordprocessingml.document`
- fallback: `application/octet-stream`

Add `openExternalFile(context, uri, mimeType, label)` that launches `ACTION_VIEW` with `FLAG_GRANT_READ_URI_PERMISSION` and catches `ActivityNotFoundException` or generic failure. Toast text should be format-specific but short, for example `No PDF viewer available`.

For `.txt`, keep the existing in-launcher `MissionZipTextDialog`. Do not send `.txt` to an external viewer from the readme button unless a later UX change asks for that.

### UI behavior

Top-level mod details dialog:

- If `details.missionZip.readme` is `.txt`, `View readme` opens `MissionZipTextDialog`
- If it is `.pdf`, `.rtf`, `.doc`, or `.docx`, `View readme` extracts to cache and opens `ACTION_VIEW`
- If extraction or launch fails, show a toast

Constituent details dialog:

- `.txt`: keep `View`
- `.pdf`, `.rtf`, `.doc`, `.docx`: show `View external`
- Other files: no view button, unless they already have a metadata button

The constituent dialog callback should be changed from one generic `onView` to either:

- `viewAction: MissionZipViewAction?`, where action has `label` and `onClick`

or keep a nullable label plus callback. A tiny data class is probably the least noisy.

### Suggested implementation points

- `GameFileFormats.kt`
  - Add `.pdf`, `.doc`, `.docx`
  - Consider adding `fun isExternalDocument(filename: String)` only if it is used outside `MissionZip`
- `MissionZip.kt`
  - Add document extension sets
  - Add `isInlineTextReadme()`, `isExternalDocumentReadme()`, `isReadmeCandidate()`
  - Update sorting and readme selection
  - Keep `readTextFile()` `.txt`-only
- `SetupSections.kt`
  - Add context access inside `ModDetailsDialog`
  - Route readme button based on document kind
  - Add external view button for constituent details
  - Add extraction/open helpers if not placed in a shared file
- `file_paths.xml`
  - No change expected; `cache/file_view/` already exists

## Tests

Add or extend local JVM tests:

- `GameFileFormatsTest`
  - `.pdf`, `.rtf`, `.doc`, and `.docx` are documentation and mission ZIP documentation
- `MissionZipTest`
  - `.txt` readme wins over `.pdf`
  - PDF becomes readme when no `.txt` exists
  - `README.pdf` wins over a larger PDF
  - ZIP-stem `.docx` wins before largest fallback
  - file ordering puts `.txt` before external docs before other files
  - `readTextFile()` still rejects external document formats

Add one focused helper test for cache extraction if the helper is pure enough for JVM tests. If it depends on Android `Context` and `FileProvider`, leave that part to an emulator/manual pass and test only the safe filename/MIME classifier locally.

## Manual verification

Use a Castaway-style mission ZIP containing a PDF readme:

1. Import the mission ZIP
2. Open the mod details dialog
3. Confirm `View readme` appears and opens the system PDF viewer
4. Open the PDF row under `Files`
5. Confirm details show `View external` and it opens the same viewer
6. Confirm a TXT-bearing mission ZIP still opens the in-launcher text viewer

## Status

- [x] Survey current mission ZIP readme implementation
- [x] Draft design
- [x] Add cache cleanup policy
- [x] Implement document classification and readme selection
- [x] Implement external extraction, cache cleanup, and viewer launch
- [x] Update UI actions
- [x] Add and run focused tests
